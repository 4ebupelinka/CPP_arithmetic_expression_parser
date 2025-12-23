#include "tokenizer.hpp"

#include <cctype>
#include <stdexcept>

namespace expr {

Tokenizer::Tokenizer(std::string sourceText) : source(std::move(sourceText)) {}

// Основной цикл разбора: проходит по строке и выделяет токены
std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) {
            break;
        }

        char ch = peek();
        switch (ch) {
        // Односимвольные токены
        case '+':
            tokens.push_back({TokenType::Plus, 0.0, "+", index});
            advance();
            break;
        case '-':
            tokens.push_back({TokenType::Minus, 0.0, "-", index});
            advance();
            break;
        case '*':
            tokens.push_back({TokenType::Star, 0.0, "*", index});
            advance();
            break;
        case '/':
            tokens.push_back({TokenType::Slash, 0.0, "/", index});
            advance();
            break;
        case '(':
            tokens.push_back({TokenType::LParen, 0.0, "(", index});
            advance();
            break;
        case ')':
            tokens.push_back({TokenType::RParen, 0.0, ")", index});
            advance();
            break;
        default:
            // Многосимвольные токены (числа и идентификаторы)
            if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
                tokens.push_back(makeNumber());
            } else if (std::isalpha(static_cast<unsigned char>(ch))) {
                tokens.push_back(makeIdentifier());
            } else {
                throw std::runtime_error("Недопустимый символ в позиции " + std::to_string(index));
            }
            break;
        }
    }

    tokens.push_back({TokenType::End, 0.0, "", index});
    return tokens;
}

bool Tokenizer::isAtEnd() const {
    return index >= source.size();
}

char Tokenizer::peek() const {
    return source[index];
}

char Tokenizer::advance() {
    return source[index++];
}

// Пропуск всех незначащих символов
void Tokenizer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

// Разбор числового литерала
// Поддерживает целые числа и числа с плавающей точкой
Token Tokenizer::makeNumber() {
    std::size_t start = index;
    bool hasDot = false;
    while (!isAtEnd()) {
        char ch = peek();
        if (ch == '.') {
            if (hasDot) {
                break; // Вторая точка — конец числа
            }
            hasDot = true;
            advance();
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            advance();
        } else {
            break;
        }
    }

    std::string text = source.substr(start, index - start);
    double value = std::stod(text);
    return {TokenType::Number, value, text, start};
}

// Разбор идентификатора (имя функции или переменной)
// Преобразует текст в нижний регистр для нечувствительности к регистру
Token Tokenizer::makeIdentifier() {
    std::size_t start = index;
    while (!isAtEnd() && std::isalpha(static_cast<unsigned char>(peek()))) {
        advance();
    }

    std::string identifier = source.substr(start, index - start);
    // Приведение к нижнему регистру
    for (char& ch : identifier) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return {TokenType::Identifier, 0.0, identifier, start};
}

} // namespace expr
