#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Быстрый подсчет количества строк в файле
// Читает файл блоками и считает символы новой строки
std::size_t countLinesInFile(const std::filesystem::path& path);

// Поиск корневой директории проекта (ищет папку tests или файл CMakeLists.txt)
std::filesystem::path findProjectRoot();

// Поиск всех .txt файлов в директории проекта
std::vector<std::filesystem::path> findTxtFiles(const std::filesystem::path& directory);

// Получение текущего времени в формате для имени файла
std::string getCurrentTimeString();

