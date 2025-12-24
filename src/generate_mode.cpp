#include "generate_mode.hpp"
#include "console.hpp"
#include "file_utils.hpp"
#include "user_input.hpp"
#include "expression_generator.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Режим генерации выражений
void runGenerateMode() {
    printHeader();

    std::cout << Color::BOLD << Color::CYAN << "Режим генерации выражений\n" << Color::RESET << "\n";

    try {
        // 1. Получаем количество выражений
        std::size_t expressionCount = askExpressionCount();

        // 2. Получаем имя файла
        std::filesystem::path fileName = selectGeneratedFileName(expressionCount);

        // 3. Определяем путь к папке tests
        std::filesystem::path projectDir = findProjectRoot();
        std::filesystem::path testsDir = projectDir / "tests";

        // Создаем папку tests если её нет
        if (!std::filesystem::exists(testsDir)) {
            std::filesystem::create_directories(testsDir);
        }

        std::filesystem::path outputPath = testsDir / fileName;

        std::cout << "\n";
        std::cout << Color::BOLD << "Конфигурация:\n" << Color::RESET;
        std::cout << "  Количество выражений: " << Color::CYAN << expressionCount << Color::RESET << "\n";
        std::cout << "  Выходной файл:        " << Color::YELLOW << outputPath << Color::RESET << "\n\n";

        // 4. Генерируем выражения
        std::cout << Color::BOLD << "Генерация выражений..." << Color::RESET << std::flush;
        std::chrono::high_resolution_clock::time_point startGen = std::chrono::high_resolution_clock::now();

        ExpressionGenerator generator;
        std::ofstream output(outputPath);
        if (!output.is_open()) {
            throw std::runtime_error("Не удалось создать файл: " + outputPath.string());
        }

        // Увеличиваем размер буфера для записи (1 МБ) для ускорения
        constexpr std::size_t bufferSize = 1024 * 1024;
        char* fileBuffer = new char[bufferSize];
        output.rdbuf()->pubsetbuf(fileBuffer, bufferSize);

        // Буферизация вывода: собираем строки в буфер и записываем батчами
        constexpr std::size_t batchSize = 10000;
        std::vector<std::string> buffer;
        buffer.reserve(batchSize);

        for (std::size_t i = 0; i < expressionCount; ++i) {
            // Генерируем выражения с глубиной от 4 до 8 для более длинных выражений
            int depth = 4 + (i % 5);
            buffer.emplace_back(generator.generate(depth));

            // Когда буфер заполнен, записываем батч в файл
            if (buffer.size() >= batchSize) {
                for (const auto& expr : buffer) {
                    output << expr << "\n";
                }
                buffer.clear();
                buffer.reserve(batchSize);
            }

            // Показываем прогресс для больших файлов
            if ((i + 1) % 10000 == 0) {
                std::cout << "\r  " << Color::CYAN << (i + 1) << "/" << expressionCount
                    << " выражений сгенерировано..." << Color::RESET << std::flush;
            }
        }

        // Записываем оставшиеся выражения
        if (!buffer.empty()) {
            for (const auto& expr : buffer) {
                output << expr << "\n";
            }
        }

        output.close();
        delete[] fileBuffer;

        std::chrono::high_resolution_clock::time_point endGen = std::chrono::high_resolution_clock::now();
        std::chrono::milliseconds genDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endGen - startGen);

        std::cout << "\r  " << Color::GREEN << "✓" << Color::RESET << " ("
            << expressionCount << " выражений, "
            << genDuration.count() << " мс)\n\n";

        std::cout << Color::GREEN << "Файл успешно создан: " << outputPath << Color::RESET << "\n\n";

    }
    catch (const std::exception& ex) {
        std::cerr << "\n" << Color::RED << Color::BOLD << "✗ Ошибка: "
            << Color::RESET << Color::RED << ex.what() << Color::RESET << "\n\n";
        throw;
    }
}

