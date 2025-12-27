#include "user_input.hpp"
#include "console.hpp"
#include "file_utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

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

