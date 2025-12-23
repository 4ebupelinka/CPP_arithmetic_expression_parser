#pragma once

#include <memory>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

namespace expr {

// Класс синтаксического анализатора (парсера)
// Строит Абстрактное Синтаксическое Дерево (AST) из списка токенов.
// Реализует алгоритм рекурсивного спуска.
class Parser {
public:
    // Конструктор принимает список токенов от лексера
    explicit Parser(std::vector<Token> tokens);

    // Основной метод запуска парсинга
    // Возвращает указатель на корневой узел AST
    // Выбрасывает std::runtime_error при синтаксических ошибках
    std::unique_ptr<AstNode> parse();

private:
    const std::vector<Token> tokens; // Список токенов
    std::size_t current = 0;         // Индекс текущего токена

    // Возвращает текущий токен без продвижения
    const Token& peek() const;
    
    // Проверяет, соответствует ли текущий токен ожидаемому типу.
    // Если да — сдвигает указатель и возвращает true.
    bool match(TokenType type);
    
    // Ожидает токен определенного типа.
    // Если тип совпадает — возвращает токен и сдвигает указатель.
    // Если нет — выбрасывает исключение с текстом errorMessage.
    const Token& consume(TokenType type, const std::string& errorMessage);
    
    // Проверка на конец списка токенов
    bool isAtEnd() const;

    // --- Методы рекурсивного спуска (от низкого приоритета к высокому) ---
    
    // Разбор выражения (сложение/вычитание)
    std::unique_ptr<AstNode> parseExpression();
    
    // Разбор слагаемого (умножение/деление)
    std::unique_ptr<AstNode> parseTerm();
    
    // Разбор фактора (унарные операции)
    std::unique_ptr<AstNode> parseFactor();
    
    // Разбор унарного оператора
    std::unique_ptr<AstNode> parseUnary();
    
    // Разбор первичного выражения (числа, скобки, вызовы функций)
    std::unique_ptr<AstNode> parsePrimary();
    
    // Разбор вызова функции
    std::unique_ptr<AstNode> parseFunctionCall(const std::string& name, std::size_t position);
};

} // namespace expr
