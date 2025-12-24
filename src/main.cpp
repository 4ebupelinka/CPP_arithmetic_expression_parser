#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <future>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "csv_writer.hpp"
#include "evaluator.hpp"
#include "thread_pool.hpp"
#include "expression_generator.hpp"

namespace {

    // ANSI цветовые коды для форматирования вывода в терминал
    namespace Color {
        constexpr const char* RESET = "\033[0m";
        constexpr const char* BOLD = "\033[1m";
        constexpr const char* RED = "\033[31m";
        constexpr const char* GREEN = "\033[32m";
        constexpr const char* YELLOW = "\033[33m";
        constexpr const char* BLUE = "\033[34m";
        constexpr const char* MAGENTA = "\033[35m";
        constexpr const char* CYAN = "\033[36m";
        constexpr const char* GRAY = "\033[90m";
    }

    // Структура для хранения исходной строки выражения с ее номером
    struct ExpressionLine {
        std::size_t number;
        std::string text;
    };

    // Вывод приветственного заголовка программы
    void printHeader() {
        std::cout << Color::BOLD << Color::CYAN;
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║    Парсер простого арифметического выражения v1.0        ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << Color::RESET << "\n";
    }


    // Быстрый подсчет количества строк в файле
    // Читает файл блоками и считает символы новой строки
    std::size_t countLinesInFile(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            throw std::runtime_error("Не удалось открыть файл для подсчета строк");
        }

        // Увеличиваем размер буфера для быстрого чтения
        constexpr std::size_t bufferSize = 4 * 1024 * 1024;  // 4 МБ буфер
        char* fileBuffer = new char[bufferSize];
        input.rdbuf()->pubsetbuf(fileBuffer, bufferSize);

        std::size_t lineCount = 0;
        bool hasContent = false;

        // Выделяем буфер динамически, чтобы избежать переполнения стека
        std::vector<char> readBuffer(bufferSize);

        while (input.read(readBuffer.data(), bufferSize) || input.gcount() > 0) {
            std::size_t bytesRead = input.gcount();
            if (bytesRead > 0) {
                hasContent = true;
                for (std::size_t i = 0; i < bytesRead; ++i) {
                    if (readBuffer[i] == '\n') {
                        ++lineCount;
                    }
                }
            }
        }

        // Если файл не пустой и не заканчивается на \n, считаем последнюю строку
        if (hasContent) {
            input.clear();
            input.seekg(0, std::ios::end);
            std::streampos fileSize = input.tellg();
            if (fileSize > 0) {
                input.seekg(-1, std::ios::end);
                char lastChar;
                if (input.get(lastChar) && lastChar != '\n') {
                    ++lineCount;  // Последняя строка без \n
                }
            }
        }

        delete[] fileBuffer;
        return lineCount;
    }

    // Функция отображения прогресс-бара.
    // Запускается в отдельном потоке.
    void displayProgress(std::atomic<std::size_t>& completed, std::size_t total) {
        const int barWidth = 50;
        while (completed < total) {
            std::size_t current = completed.load();
            float progress = static_cast<float>(current) / total;
            int pos = static_cast<int>(barWidth * progress);

            std::cout << "\r  " << Color::CYAN << "[";
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) std::cout << "█";
                else if (i == pos) std::cout << "▒";
                else std::cout << "░";
            }
            std::cout << "] " << Color::BOLD << std::setw(3) << static_cast<int>(progress * 100.0f)
                << "%" << Color::RESET << " (" << current << "/" << total << ")";
            std::cout.flush();

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        // Финальное обновление до 100%
        std::cout << "\r  " << Color::GREEN << "[";
        for (int i = 0; i < barWidth; ++i) std::cout << "█";
        std::cout << "] " << Color::BOLD << "100%" << Color::RESET
            << " (" << total << "/" << total << ")\n";
    }

    // Потоковое чтение и обработка файла по частям (chunks) для экономии памяти
    // Читает файл порциями и сразу отправляет задачи в пул потоков
    // Обрабатывает futures батчами и вызывает callback для записи результатов
    // Это позволяет обрабатывать файлы любого размера без загрузки всего файла в память
    template<typename ProcessCallback>
    void processExpressionsStreaming(
        const std::filesystem::path& path,
        expr::ExpressionEvaluator& evaluator,
        expr::ThreadPool& pool,
        std::atomic<std::size_t>& completed,
        std::atomic<std::size_t>& totalLines,
        ProcessCallback&& processBatch,
        std::size_t chunkSize = 10000,  // Обрабатываем по 10000 строк за раз
        std::size_t batchSize = 1000) {  // Обрабатываем futures батчами по 1000

        std::ifstream input(path);
        if (!input.is_open()) {
            throw std::runtime_error("Не удалось открыть входной файл");
        }

        // Увеличиваем размер буфера для чтения (1 МБ) для ускорения
        // Важно: pubsetbuf должен быть вызван ДО первого чтения из потока
        constexpr std::size_t bufferSize = 1024 * 1024;
        char* fileBuffer = new char[bufferSize];
        input.rdbuf()->pubsetbuf(fileBuffer, bufferSize);

        std::vector<ExpressionLine> chunk;
        chunk.reserve(chunkSize);
        std::vector<std::future<expr::EvaluationRecord>> futures;
        futures.reserve(batchSize);

        std::string buffer;
        std::size_t lineNumber = 1;

        auto processFuturesBatch = [&]() {
            if (futures.empty()) return;

            // Собираем результаты батча
            std::vector<expr::EvaluationRecord> batch;
            batch.reserve(futures.size());
            for (auto& future : futures) {
                batch.push_back(future.get());
            }

            // Вызываем callback для обработки батча
            processBatch(batch);

            // Очищаем futures
            futures.clear();
            futures.reserve(batchSize);
            };

        while (std::getline(input, buffer)) {
            chunk.push_back({ lineNumber++, std::move(buffer) });
            // Не обновляем totalLines, так как оно уже известно из подсчета

            // Когда накопили достаточно строк, отправляем в обработку
            if (chunk.size() >= chunkSize) {
                // Отправляем весь chunk в пул потоков
                for (const auto& expressionLine : chunk) {
                    futures.emplace_back(pool.enqueue(
                        [expressionLine, &evaluator, &completed]() -> expr::EvaluationRecord {
                            expr::EvaluationRecord record;
                            record.lineNumber = expressionLine.number;
                            record.expression = expressionLine.text;
                            try {
                                if (expressionLine.text.empty()) {
                                    throw std::runtime_error("Пустая строка");
                                }
                                // Основная логика вычисления
                                double value = evaluator.evaluate(expressionLine.text);
                                record.value = value;
                                record.status = "success";
                                record.message = "";
                            }
                            catch (const std::exception& ex) {
                                record.value.reset();
                                record.status = "error";
                                record.message = ex.what();
                            }
                            completed.fetch_add(1); // Обновляем прогресс
                            return record;
                        }));

                    // Когда накопили достаточно futures, обрабатываем батч
                    if (futures.size() >= batchSize) {
                        processFuturesBatch();
                    }
                }
                // Очищаем chunk для следующей порции
                chunk.clear();
                chunk.reserve(chunkSize);
            }
        }

        // Обрабатываем оставшиеся строки (если их меньше chunkSize)
        if (!chunk.empty()) {
            for (const auto& expressionLine : chunk) {
                futures.emplace_back(pool.enqueue(
                    [expressionLine, &evaluator, &completed]() -> expr::EvaluationRecord {
                        expr::EvaluationRecord record;
                        record.lineNumber = expressionLine.number;
                        record.expression = expressionLine.text;
                        try {
                            if (expressionLine.text.empty()) {
                                throw std::runtime_error("Пустая строка");
                            }
                            // Основная логика вычисления
                            double value = evaluator.evaluate(expressionLine.text);
                            record.value = value;
                            record.status = "success";
                            record.message = "";
                        }
                        catch (const std::exception& ex) {
                            record.value.reset();
                            record.status = "error";
                            record.message = ex.what();
                        }
                        completed.fetch_add(1); // Обновляем прогресс
                        return record;
                    }));

                // Когда накопили достаточно futures, обрабатываем батч
                if (futures.size() >= batchSize) {
                    processFuturesBatch();
                }
            }
        }

        // Обрабатываем оставшиеся futures
        processFuturesBatch();

        delete[] fileBuffer;
    }

    // Поиск корневой директории проекта (ищет папку tests или файл CMakeLists.txt)
    std::filesystem::path findProjectRoot() {
        std::filesystem::path current = std::filesystem::current_path();

        // Поднимаемся вверх по директориям, пока не найдем папку tests или CMakeLists.txt
        while (!current.empty() && current != current.root_path()) {
            std::filesystem::path testsDir = current / "tests";
            std::filesystem::path cmakeFile = current / "CMakeLists.txt";

            if (std::filesystem::exists(testsDir) && std::filesystem::is_directory(testsDir)) {
                return current;
            }
            if (std::filesystem::exists(cmakeFile) && std::filesystem::is_regular_file(cmakeFile)) {
                return current;
            }

            current = current.parent_path();
        }

        // Если не нашли, возвращаем текущую директорию
        return std::filesystem::current_path();
    }

    // Поиск всех .txt файлов в директории проекта
    std::vector<std::filesystem::path> findTxtFiles(const std::filesystem::path& directory) {
        std::vector<std::filesystem::path> txtFiles;
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return txtFiles;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                txtFiles.push_back(entry.path());
            }
        }

        std::sort(txtFiles.begin(), txtFiles.end());
        return txtFiles;
    }

    // Получение текущего времени в формате для имени файла
    std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;

#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    // Безопасный парсинг числа из строки
    std::size_t parseNumber(const std::string& value) {
        try {
            std::size_t result = std::stoul(value);
            if (result == 0) {
                throw std::runtime_error("Число должно быть положительным");
            }
            return result;
        }
        catch (const std::exception&) {
            throw std::runtime_error("Некорректное числовое значение");
        }
    }

    // Интерактивный выбор входного файла
    std::filesystem::path selectInputFile() {
        std::filesystem::path projectDir = findProjectRoot();
        std::filesystem::path testsDir = projectDir / "tests";
        auto txtFiles = findTxtFiles(testsDir);

        if (txtFiles.empty()) {
            std::cout << Color::YELLOW << "Внимание: " << Color::RESET
                << "не найдено .txt файлов в папке tests.\n";
            std::cout << "Директория: " << Color::CYAN << testsDir << Color::RESET << "\n\n";
        }
        else {
            std::cout << Color::BOLD << "Найденные .txt файлы в папке tests:\n" << Color::RESET;
            for (std::size_t i = 0; i < txtFiles.size(); ++i) {
                std::cout << "  " << Color::CYAN << (i + 1) << Color::RESET << ". "
                    << Color::YELLOW << txtFiles[i].filename().string() << Color::RESET << "\n";
            }
            std::cout << "\n";
        }

        std::string input;
        std::cout << Color::BOLD << "Введите номер файла или путь до входного файла: " << Color::RESET;
        std::getline(std::cin, input);

        // Удаление пробелов
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);

        if (input.empty()) {
            throw std::runtime_error("Пустой ввод");
        }

        // Проверка, является ли ввод числом
        bool isNumber = true;
        for (char c : input) {
            if (!std::isdigit(c)) {
                isNumber = false;
                break;
            }
        }

        if (isNumber && !txtFiles.empty()) {
            std::size_t index = parseNumber(input);
            if (index > 0 && index <= txtFiles.size()) {
                return txtFiles[index - 1];
            }
            else {
                throw std::runtime_error("Номер файла вне допустимого диапазона");
            }
        }
        else {
            // Пользователь ввел путь
            std::filesystem::path inputPath = input;
            if (!std::filesystem::exists(inputPath)) {
                throw std::runtime_error("Файл не найден: " + inputPath.string());
            }
            return inputPath;
        }
    }

    // Интерактивный выбор выходного файла
    std::filesystem::path selectOutputFile(const std::filesystem::path& inputPath) {
        std::cout << Color::BOLD << "Выберите способ задания выходного файла:\n" << Color::RESET;
        std::cout << "  " << Color::CYAN << "1" << Color::RESET << ". Название по умолчанию (имя входного файла + _results_ + время)\n";
        std::cout << "  " << Color::CYAN << "2" << Color::RESET << ". Кастомное название\n\n";

        std::string choice;
        std::cout << Color::BOLD << "Ваш выбор (1 или 2): " << Color::RESET;
        std::getline(std::cin, choice);

        // Удаление пробелов
        choice.erase(0, choice.find_first_not_of(" \t"));
        choice.erase(choice.find_last_not_of(" \t") + 1);

        if (choice == "1") {
            // Название по умолчанию
            std::string inputStem = inputPath.stem().string();
            std::string timeStr = getCurrentTimeString();
            std::filesystem::path outputPath = inputPath.parent_path() / (inputStem + "_results_" + timeStr + ".csv");
            return outputPath;
        }
        else if (choice == "2") {
            // Кастомное название
            std::string customName;
            std::cout << Color::BOLD << "Введите название выходного файла (можно с путем, расширение .csv добавится автоматически): " << Color::RESET;
            std::getline(std::cin, customName);

            // Удаление пробелов
            customName.erase(0, customName.find_first_not_of(" \t"));
            customName.erase(customName.find_last_not_of(" \t") + 1);

            if (customName.empty()) {
                throw std::runtime_error("Пустое название файла");
            }

            std::filesystem::path customPath(customName);

            // Если путь абсолютный, используем его как есть
            if (customPath.is_absolute()) {
                // Добавляем .csv если его нет
                if (customPath.extension() != ".csv") {
                    customPath.replace_extension(".csv");
                }
                return customPath;
            }

            // Если путь относительный, используем директорию входного файла как базовую
            std::filesystem::path outputPath = inputPath.parent_path() / customPath;

            // Добавляем .csv если его нет
            if (outputPath.extension() != ".csv") {
                outputPath.replace_extension(".csv");
            }

            return outputPath;
        }
        else {
            throw std::runtime_error("Некорректный выбор. Используйте 1 или 2");
        }
    }

    // Интерактивный ввод количества потоков
    std::size_t selectThreadCount() {
        std::size_t defaultThreads = std::thread::hardware_concurrency();
        if (defaultThreads == 0) {
            defaultThreads = 2; // Резервное значение
        }

        std::cout << Color::BOLD << "Введите количество потоков" << Color::RESET
            << " (по умолчанию: " << Color::CYAN << defaultThreads << Color::RESET << "): ";

        std::string input;
        std::getline(std::cin, input);

        // Удаление пробелов
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);

        if (input.empty()) {
            return defaultThreads;
        }

        return parseNumber(input);
    }

    // Запрос продолжения работы с другим файлом
    bool askContinue() {
        std::cout << Color::BOLD << "Обработать еще один файл? (y/n): " << Color::RESET;
        std::string input;
        std::getline(std::cin, input);

        // Удаление пробелов и приведение к нижнему регистру
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);
        std::transform(input.begin(), input.end(), input.begin(), ::tolower);

        return (input == "y" || input == "yes" || input == "д" || input == "да");
    }

    // Интерактивный ввод количества выражений для генерации
    std::size_t askExpressionCount() {
        std::cout << Color::BOLD << "Введите количество выражений для генерации: " << Color::RESET;
        std::string input;
        std::getline(std::cin, input);

        // Удаление пробелов
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);

        if (input.empty()) {
            throw std::runtime_error("Пустой ввод");
        }

        return parseNumber(input);
    }

    // Интерактивный выбор имени файла для генерации
    std::filesystem::path selectGeneratedFileName(std::size_t expressionCount) {
        std::cout << Color::BOLD << "Выберите способ задания имени файла:\n" << Color::RESET;
        std::cout << "  " << Color::CYAN << "1" << Color::RESET << ". Автоматическое название (generate_" << expressionCount << ".txt)\n";
        std::cout << "  " << Color::CYAN << "2" << Color::RESET << ". Кастомное название\n\n";

        std::string choice;
        std::cout << Color::BOLD << "Ваш выбор (1 или 2): " << Color::RESET;
        std::getline(std::cin, choice);

        // Удаление пробелов
        choice.erase(0, choice.find_first_not_of(" \t"));
        choice.erase(choice.find_last_not_of(" \t") + 1);

        if (choice == "1") {
            // Автоматическое название
            return std::filesystem::path("generate_" + std::to_string(expressionCount) + ".txt");
        }
        else if (choice == "2") {
            // Кастомное название
            std::string customName;
            std::cout << Color::BOLD << "Введите название файла (расширение .txt добавится автоматически): " << Color::RESET;
            std::getline(std::cin, customName);

            // Удаление пробелов
            customName.erase(0, customName.find_first_not_of(" \t"));
            customName.erase(customName.find_last_not_of(" \t") + 1);

            if (customName.empty()) {
                throw std::runtime_error("Пустое название файла");
            }

            std::filesystem::path customPath(customName);

            // Добавляем .txt если его нет
            if (customPath.extension() != ".txt") {
                customPath.replace_extension(".txt");
            }

            return customPath;
        }
        else {
            throw std::runtime_error("Некорректный выбор. Используйте 1 или 2");
        }
    }

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
            auto startGen = std::chrono::high_resolution_clock::now();

            ExpressionGenerator generator;
            std::ofstream output(outputPath);
            if (!output.is_open()) {
                throw std::runtime_error("Не удалось создать файл: " + outputPath.string());
            }

            for (std::size_t i = 0; i < expressionCount; ++i) {
                // Генерируем выражения с глубиной от 4 до 8 для более длинных выражений
                int depth = 4 + (i % 5);
                output << generator.generate(depth) << "\n";

                // Показываем прогресс для больших файлов
                if ((i + 1) % 10000 == 0) {
                    std::cout << "\r  " << Color::CYAN << (i + 1) << "/" << expressionCount
                        << " выражений сгенерировано..." << Color::RESET << std::flush;
                }
            }

            output.close();

            auto endGen = std::chrono::high_resolution_clock::now();
            auto genDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endGen - startGen);

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

} // namespace

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
            auto startCount = std::chrono::high_resolution_clock::now();
            std::size_t totalLines = countLinesInFile(inputPath);
            auto endCount = std::chrono::high_resolution_clock::now();
            auto countDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endCount - startCount);
            std::cout << " " << Color::GREEN << "✓" << Color::RESET << " ("
                << totalLines << " строк, " << countDuration.count() << " мс)\n\n";

            // 1. Потоковое чтение и обработка файла по частям
            std::cout << Color::BOLD << "Обработка выражений:\n" << Color::RESET;
            auto startProcess = std::chrono::high_resolution_clock::now();

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
            auto processBatch = [&](const std::vector<expr::EvaluationRecord>& batch) {
                // Добавляем результаты в буфер и записываем последовательные результаты
                std::lock_guard<std::mutex> lock(bufferMutex);
                for (const auto& record : batch) {
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
                for (const auto& [lineNum, record] : resultBuffer) {
                    writer.writeRecord(record);
                }
            }

            // Ожидание завершения потока прогресса
            progressThread.join();

            auto endProcess = std::chrono::high_resolution_clock::now();
            auto processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endProcess - startProcess);

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
