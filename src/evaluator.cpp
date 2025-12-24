#include "evaluator.hpp"

#include "parser.hpp"
#include "tokenizer.hpp"

namespace expr {

// Полный цикл обработки выражения:
// 1. Токенизация (Tokenizer)
// 2. Парсинг (Parser) -> построение AST
// 3. Вычисление (evaluate) -> получение числового результата
double ExpressionEvaluator::evaluate(const std::string& expression) const {
    // Этап 1: Лексический анализ
    Tokenizer tokenizer(expression);
    auto tokens = tokenizer.tokenize();

    // Этап 2: Синтаксический анализ
    Parser parser(std::move(tokens));
    auto ast = parser.parse();

    // Этап 3: Вычисление
    return ast->evaluate();
}

} // namespace expr
