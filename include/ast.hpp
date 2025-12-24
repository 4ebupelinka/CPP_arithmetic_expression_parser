#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace expr {

// Базовый класс для узла абстрактного синтаксического дерева (AST).
// Все типы узлов (числа, операции, функции) наследуются от этого класса.
class AstNode {
public:
    virtual ~AstNode() = default;

    // Рекурсивно вычисляет значение поддерева
    virtual double evaluate() const = 0;
};

// Узел, представляющий числовую константу (лист дерева)
class NumberNode final : public AstNode {
public:
    explicit NumberNode(double value) : value(value) {}
    
    // Возвращает само число
    double evaluate() const override { return value; }

private:
    double value;
};

// Узел бинарной арифметической операции (+, -, *, /)
class BinaryNode final : public AstNode {
public:
    BinaryNode(char op, std::unique_ptr<AstNode> left, std::unique_ptr<AstNode> right)
        : op(op), left(std::move(left)), right(std::move(right)) {}

    double evaluate() const override;

private:
    char op;                        // Символ операции
    std::unique_ptr<AstNode> left;  // Левый операнд
    std::unique_ptr<AstNode> right; // Правый операнд
};

// Узел унарной операции (унарный минус или плюс)
class UnaryNode final : public AstNode {
public:
    UnaryNode(char op, std::unique_ptr<AstNode> child)
        : op(op), child(std::move(child)) {}

    double evaluate() const override;

private:
    char op;
    std::unique_ptr<AstNode> child;
};

// Узел вызова математической функции (sin, cos и т.д.)
class FunctionNode final : public AstNode {
public:
    FunctionNode(std::string name, std::unique_ptr<AstNode> argument)
        : name(std::move(name)), argument(std::move(argument)) {}

    double evaluate() const override;

private:
    std::string name;                // Имя функции
    std::unique_ptr<AstNode> argument; // Аргумент функции
};

} // namespace expr
