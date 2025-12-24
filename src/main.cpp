#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "console.hpp"
#include "csv_writer.hpp"
#include "evaluator.hpp"
#include "expression_processor.hpp"
#include "file_utils.hpp"
#include "generate_mode.hpp"
#include "progress_bar.hpp"
#include "thread_pool.hpp"
#include "user_input.hpp"

// Точка входа в программу
int main(int argc, char** argv) {
    // Проверяем, запущен ли режим генерации
    if (argc >= 2 && std::string(argv[1]) == "generate") {
        try {
            runGenerateMode();
            return 0;
        }
        catch (const std::exception& ex) {
            std::cerr << Color::RED << Color::BOLD << "✗ Ошибка: "
                << Color::RESET << Color::RED << ex.what() << Color::RESET << "\n\n";
            return 1;
        }
    }

    printHeader();

    bool continueProcessing = true;

    while (continueProcessing) {
        try {
            // Интерактивный выбор входного файла
            std::filesystem::path inputPath = selectInputFile();

            // Интерактивный выбор выходного файла
            std::filesystem::path outputPath = selectOutputFile(inputPath);

            // Интерактивный ввод количества потоков
            std::size_t threadCount = selectThreadCount();

            std::cout << "\n";

            // Информация о конфигурации
            std::cout << Color::BOLD << "Конфигурация:\n" << Color::RESET;
            std::cout << "  Входной файл:  " << Color::YELLOW << inputPath << Color::RESET << "\n";
            std::cout << "  Выходной файл: " << Color::YELLOW << outputPath << Color::RESET << "\n";
            std::cout << "  Потоков:       " << Color::CYAN << threadCount << Color::RESET << "\n\n";

            // 0. Быстрый подсчет количества строк в файле
            std::cout << Color::BOLD << "Подсчет строк в файле..." << Color::RESET << std::flush;
            std::chrono::high_resolution_clock::time_point startCount = std::chrono::high_resolution_clock::now();
            std::size_t totalLines = countLinesInFile(inputPath);
            std::chrono::high_resolution_clock::time_point endCount = std::chrono::high_resolution_clock::now();
            std::chrono::milliseconds countDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endCount - startCount);
            std::cout << " " << Color::GREEN << "✓" << Color::RESET << " ("
                << totalLines << " строк, " << countDuration.count() << " мс)\n\n";

            // 1. Потоковое чтение и обработка файла по частям
            std::cout << Color::BOLD << "Обработка выражений:\n" << Color::RESET;
            std::chrono::high_resolution_clock::time_point startProcess = std::chrono::high_resolution_clock::now();

            expr::ExpressionEvaluator evaluator;
            expr::ThreadPool pool(threadCount);
            std::atomic<std::size_t> completed{ 0 }; // Счетчик обработанных задач

            // Инициализируем CSV writer
            expr::CsvWriter writer(outputPath);

            // Используем map для хранения результатов в правильном порядке
            // Ключ - номер строки, значение - результат
            std::map<std::size_t, expr::EvaluationRecord> resultBuffer;
            std::mutex bufferMutex; // Мьютекс для защиты буфера

            // Счетчики для статистики
            std::atomic<std::size_t> successCount{ 0 };
            std::atomic<std::size_t> errorCount{ 0 };

            // Ожидаемый следующий номер строки для записи
            std::size_t nextLineToWrite = 1;

            // Callback для обработки батча результатов
            std::function<void(const std::vector<expr::EvaluationRecord>&)> processBatch = [&](const std::vector<expr::EvaluationRecord>& batch) {
                // Добавляем результаты в буфер и записываем последовательные результаты
                std::lock_guard<std::mutex> lock(bufferMutex);
                for (const expr::EvaluationRecord& record : batch) {
                    resultBuffer[record.lineNumber] = record;

                    // Обновляем статистику
                    if (record.status == "success") {
                        successCount.fetch_add(1);
                    }
                    else {
                        errorCount.fetch_add(1);
                    }
                }

                // Записываем все последовательные результаты, которые готовы
                while (resultBuffer.find(nextLineToWrite) != resultBuffer.end()) {
                    writer.writeRecord(resultBuffer[nextLineToWrite]);
                    resultBuffer.erase(nextLineToWrite);
                    ++nextLineToWrite;
                }
            };

            // Запуск отображения прогресса в отдельном потоке (сразу после подсчета строк)
            std::thread progressThread(displayProgress, std::ref(completed), totalLines);

            // Читаем и обрабатываем файл по частям (streaming)
            // Передаем totalLines как atomic для обновления, но уже знаем точное значение
            std::atomic<std::size_t> totalLinesAtomic{ totalLines };
            processExpressionsStreaming(inputPath, evaluator, pool, completed, totalLinesAtomic, processBatch);

            // Ждем завершения всех задач
            while (completed.load() < totalLines) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Обрабатываем оставшиеся результаты в буфере
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                for (const std::pair<const std::size_t, expr::EvaluationRecord>& pair : resultBuffer) {
                    writer.writeRecord(pair.second);
                }
            }

            // Ожидание завершения потока прогресса
            progressThread.join();

            std::chrono::high_resolution_clock::time_point endProcess = std::chrono::high_resolution_clock::now();
            std::chrono::milliseconds processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endProcess - startProcess);

            std::cout << "\n" << Color::BOLD << "Запись результатов..." << Color::RESET << std::flush;
            std::cout << " " << Color::GREEN << "✓" << Color::RESET << "\n\n";

            // 4. Вывод итоговой статистики
            std::size_t finalSuccess = successCount.load();
            std::size_t finalError = errorCount.load();

            std::cout << Color::BOLD << "Статистика:\n" << Color::RESET;
            std::cout << "  Всего выражений:  " << Color::CYAN << totalLines << Color::RESET << "\n";
            std::cout << "  Успешно:          " << Color::GREEN << finalSuccess << Color::RESET << "\n";
            if (finalError > 0) {
                std::cout << "  Ошибок:           " << Color::RED << finalError << Color::RESET << "\n";
            }
            std::cout << "  Время обработки:  " << Color::MAGENTA << processDuration.count()
                << " мс" << Color::RESET << "\n";

            // Расчет производительности (выражений в секунду)
            if (processDuration.count() > 0) {
                std::cout << "  Производительность: " << Color::YELLOW
                    << static_cast<int>(totalLines * 1000.0 / processDuration.count())
                    << " выр/сек" << Color::RESET << "\n\n";
            }

            std::cout << Color::GREEN << "Результаты сохранены в: " << outputPath << Color::RESET << "\n\n";

            // Спрашиваем, хочет ли пользователь продолжить
            continueProcessing = askContinue();
            if (continueProcessing) {
                std::cout << "\n";
            }

        }
        catch (const std::exception& ex) {
            std::cerr << "\n" << Color::RED << Color::BOLD << "✗ Ошибка: "
                << Color::RESET << Color::RED << ex.what() << Color::RESET << "\n\n";

            // При ошибке тоже спрашиваем, хочет ли пользователь попробовать снова
            continueProcessing = askContinue();
            if (continueProcessing) {
                std::cout << "\n";
            }
        }
    }

    std::cout << Color::CYAN << "Работа завершена. До свидания!" << Color::RESET << "\n\n";
    return 0;
}
