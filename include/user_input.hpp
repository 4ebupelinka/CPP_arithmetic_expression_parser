#pragma once

#include <filesystem>
#include <cstddef>
#include <string>

// Безопасный парсинг числа из строки
std::size_t parseNumber(const std::string& value);

// Интерактивный выбор входного файла
std::filesystem::path selectInputFile();

// Интерактивный выбор выходного файла
std::filesystem::path selectOutputFile(const std::filesystem::path& inputPath);

// Интерактивный ввод количества потоков
std::size_t selectThreadCount();

// Запрос продолжения работы с другим файлом
bool askContinue();

// Интерактивный ввод количества выражений для генерации
std::size_t askExpressionCount();

// Интерактивный выбор имени файла для генерации
std::filesystem::path selectGeneratedFileName(std::size_t expressionCount);

