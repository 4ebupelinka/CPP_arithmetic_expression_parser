#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace expr {

// Структура для хранения результата вычисления одной строки
struct EvaluationRecord {
    std::size_t lineNumber;       // Номер строки в исходном файле
    std::string expression;       // Исходный текст выражения
    std::optional<double> value;  // Результат (если вычисление успешно)
    std::string status;           // Статус (success или error)
    std::string message;          // Сообщение об ошибке (если есть)
};

// Класс для записи результатов в формате CSV (Comma-Separated Values)
// Обеспечивает корректное экранирование специальных символов.
class CsvWriter {
public:
    // Конструктор открывает файл для записи (перезаписывая его)
    explicit CsvWriter(std::filesystem::path targetPath);

    // Записывает пакет результатов в файл
    void write(const std::vector<EvaluationRecord>& records) const;
    
    // Инициализирует файл (записывает заголовок)
    void initialize() const;
    
    // Записывает один результат в файл (для потоковой записи)
    void writeRecord(const EvaluationRecord& record) const;

private:
    std::filesystem::path path; // Путь к выходному файлу
    mutable bool headerWritten = false; // Флаг записи заголовка
};

} // namespace expr
ц