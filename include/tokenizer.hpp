#pragma once

#include <string>
#include <vector>

#include "token.hpp"

namespace expr {

// Класс лексического анализатора (лексера)
// Преобразует входную строку с математическим выражением в последовательность токенов.
// Игнорирует пробельные символы.
class Tokenizer {
public:
    // Конструктор принимает исходную строку выражения
    explicit Tokenizer(std::string sourceText);

    // Основной метод запуска токенизации
    // Возвращает вектор токенов, заканчивающийся токеном End
    // Выбрасывает std::runtime_error при обнаружении неизвестных символов
    std::vector<Token> tokenize();

private:
    const std::string source; // Исходная строка
    std::size_t index = 0;    // Текущая позиция чтения

    // Проверка достижения конца строки
    bool isAtEnd() const;
    
    // Возвращает текущий символ без продвижения вперед
    char peek() const;
    
    // Возвращает текущий символ и сдвигает указатель вперед
    char advance();
    
    // Пропускает пробелы, табуляции и переводы строк
    void skipWhitespace();

    // Считывает число (целое или с плавающей точкой)
    Token makeNumber();
    
    // Считывает идентификатор (имя переменной или функции)
    Token makeIdentifier();
};

} // namespace expr
