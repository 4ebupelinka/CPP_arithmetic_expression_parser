#include "file_utils.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <ctime>
#else
#include <ctime>
#endif

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
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
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

