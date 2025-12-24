#include "csv_writer.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace expr {

CsvWriter::CsvWriter(std::filesystem::path targetPath) : path(std::move(targetPath)) {
    initialize();
}

// Инициализация файла (запись заголовка)
void CsvWriter::initialize() const {
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Не удалось открыть файл для записи CSV");
    }
    stream << "line,expression,status,result,message\n";
    headerWritten = true;
}

// Запись одного результата в файл (для потоковой записи)
void CsvWriter::writeRecord(const EvaluationRecord& record) const {
    std::ofstream stream(path, std::ios::app);
    if (!stream.is_open()) {
        throw std::runtime_error("Не удалось открыть файл для записи CSV");
    }
    
    // Настройка формата вывода чисел
    stream.setf(std::ios::fixed);
    stream << std::setprecision(10);

    stream << record.lineNumber << ',';

    // Экранирование выражения (замена двойных кавычек на одинарные)
    // и оборачивание в кавычки
    std::string sanitized = record.expression;
    for (char& ch : sanitized) {
        if (ch == '"') {
            ch = '\'';
        }
    }
    stream << '"' << sanitized << '"' << ',';

    stream << record.status << ',';
    
    // Запись числового значения, если оно есть
    if (record.value.has_value()) {
        stream << record.value.value();
    }
    stream << ',';

    // Экранирование сообщения об ошибке
    std::string message = record.message;
    for (char& ch : message) {
        if (ch == '"') {
            ch = '\'';
        }
    }
    stream << '"' << message << '"' << '\n';
}

// Запись результатов в CSV файл
// Формат: line,expression,status,result,message
void CsvWriter::write(const std::vector<EvaluationRecord>& records) const {
    if (!headerWritten) {
        initialize();
    }
    
    std::ofstream stream(path, std::ios::app);
    if (!stream.is_open()) {
        throw std::runtime_error("Не удалось открыть файл для записи CSV");
    }
    
    // Настройка формата вывода чисел
    stream.setf(std::ios::fixed);
    stream << std::setprecision(10);

    for (const auto& record : records) {
        stream << record.lineNumber << ',';

        // Экранирование выражения (замена двойных кавычек на одинарные)
        // и оборачивание в кавычки
        std::string sanitized = record.expression;
        for (char& ch : sanitized) {
            if (ch == '"') {
                ch = '\'';
            }
        }
        stream << '"' << sanitized << '"' << ',';

        stream << record.status << ',';
        
        // Запись числового значения, если оно есть
        if (record.value.has_value()) {
            stream << record.value.value();
        }
        stream << ',';

        // Экранирование сообщения об ошибке
        std::string message = record.message;
        for (char& ch : message) {
            if (ch == '"') {
                ch = '\'';
            }
        }
        stream << '"' << message << '"' << '\n';
    }
}

} // namespace expr
