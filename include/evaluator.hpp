#pragma once

#include <string>

namespace expr {

// Класс-фасад для вычисления математических выражений.
// Объединяет этапы токенизации, парсинга и вычисления AST.
class ExpressionEvaluator {
public:
    ExpressionEvaluator() = default;

    // Вычисляет значение математического выражения, заданного строкой.
    // Пример: "2 + 2 * 2" -> 6.0
    // Выбрасывает исключения в случае ошибок синтаксиса или вычисления.
    double evaluate(const std::string& expression) const;
};

} // namespace expr
