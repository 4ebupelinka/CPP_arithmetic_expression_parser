#include "parser.hpp"

#include <stdexcept>
#include <unordered_set>

namespace expr {

namespace {
// Допустимые математические функции
const std::unordered_set<std::string> kFunctions = {
    "sin", "cos", "tan", "ctan", "arcsin", "arccos"
};
}

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

// Запуск процесса парсинга
// Ожидает, что всё выражение будет полностью разобрано
std::unique_ptr<AstNode> Parser::parse() {
    auto exprNode = parseExpression();
    if (!isAtEnd()) {
        throw std::runtime_error("Неожиданный хвост выражения возле позиции " +
                                 std::to_string(peek().position));
    }
    return exprNode;
}

const Token& Parser::peek() const {
    return tokens[current];
}

bool Parser::match(TokenType type) {
    if (!isAtEnd() && tokens[current].type == type) {
        ++current;
        return true;
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& errorMessage) {
    if (match(type)) {
        return tokens[current - 1];
    }
    throw std::runtime_error(errorMessage);
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::End;
}

// Грамматика: Expression -> Term { ("+" | "-") Term }
std::unique_ptr<AstNode> Parser::parseExpression() {
    auto node = parseTerm();
    while (true) {
        if (match(TokenType::Plus)) {
            auto right = parseTerm();
            node = std::make_unique<BinaryNode>('+', std::move(node), std::move(right));
        } else if (match(TokenType::Minus)) {
            auto right = parseTerm();
            node = std::make_unique<BinaryNode>('-', std::move(node), std::move(right));
        } else {
            break;
        }
    }
    return node;
}

// Грамматика: Term -> Factor { ("*" | "/") Factor }
std::unique_ptr<AstNode> Parser::parseTerm() {
    auto node = parseFactor();
    while (true) {
        if (match(TokenType::Star)) {
            auto right = parseFactor();
            node = std::make_unique<BinaryNode>('*', std::move(node), std::move(right));
        } else if (match(TokenType::Slash)) {
            auto right = parseFactor();
            node = std::make_unique<BinaryNode>('/', std::move(node), std::move(right));
        } else {
            break;
        }
    }
    return node;
}

// Грамматика: Factor -> Unary
std::unique_ptr<AstNode> Parser::parseFactor() {
    return parseUnary();
}

// Грамматика: Unary -> ("+" | "-") Unary | Primary
std::unique_ptr<AstNode> Parser::parseUnary() {
    if (match(TokenType::Plus)) {
        return std::make_unique<UnaryNode>('+', parseUnary());
    }
    if (match(TokenType::Minus)) {
        return std::make_unique<UnaryNode>('-', parseUnary());
    }
    return parsePrimary();
}

// Грамматика: Primary -> Number | Identifier "(" Expression ")" | "(" Expression ")"
std::unique_ptr<AstNode> Parser::parsePrimary() {
    // Число
    if (match(TokenType::Number)) {
        const auto& token = tokens[current - 1];
        return std::make_unique<NumberNode>(token.numericValue);
    }

    // Вызов функции (Identifier)
    if (match(TokenType::Identifier)) {
        const auto& token = tokens[current - 1];
        return parseFunctionCall(token.text, token.position);
    }

    // Группировка скобками
    if (match(TokenType::LParen)) {
        auto node = parseExpression();
        consume(TokenType::RParen, "Ожидалась закрывающая скобка");
        return node;
    }

    throw std::runtime_error("Неожиданный токен возле позиции " +
                             std::to_string(peek().position));
}

// Разбор вызова функции, например: sin(x)
std::unique_ptr<AstNode> Parser::parseFunctionCall(const std::string& name, std::size_t position) {
    if (!kFunctions.contains(name)) {
        throw std::runtime_error("Неизвестная функция '" + name + "' на позиции " +
                                 std::to_string(position));
    }
    consume(TokenType::LParen, "Ожидалась открывающая скобка после имени функции");
    auto argument = parseExpression();
    consume(TokenType::RParen, "Ожидалась закрывающая скобка после аргумента функции");
    return std::make_unique<FunctionNode>(name, std::move(argument));
}

} // namespace expr
