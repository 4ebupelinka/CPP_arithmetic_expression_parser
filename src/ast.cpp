#include "ast.hpp"

#include <limits>

namespace expr {

namespace {
// Точность сравнения вещественных чисел с нулём
constexpr double kEpsilon = 1e-12;
}

// Вычисление бинарной операции
double BinaryNode::evaluate() const {
    double leftValue = left->evaluate();
    double rightValue = right->evaluate();

    switch (op) {
    case '+':
        return leftValue + rightValue;
    case '-':
        return leftValue - rightValue;
    case '*':
        return leftValue * rightValue;
    case '/':
        // Проверка деления на ноль с учетом погрешности double
        if (std::abs(rightValue) < kEpsilon) {
            throw std::runtime_error("Деление на ноль");
        }
        return leftValue / rightValue;
    default:
        throw std::runtime_error("Неизвестная бинарная операция");
    }
}

// Вычисление унарной операции
double UnaryNode::evaluate() const {
    double childValue = child->evaluate();
    switch (op) {
    case '+':
        return childValue; // Унарный плюс ничего не меняет
    case '-':
        return -childValue; // Унарный минус инвертирует знак
    default:
        throw std::runtime_error("Неизвестная унарная операция");
    }
}

// Вычисление математических функций
double FunctionNode::evaluate() const {
    double arg = argument->evaluate();

    // Тригонометрические функции
    if (name == "sin") {
        return std::sin(arg);
    }
    if (name == "cos") {
        return std::cos(arg);
    }
    if (name == "tan") {
        double cosValue = std::cos(arg);
        if (std::abs(cosValue) < kEpsilon) {
            throw std::runtime_error("Тангенс не определён для данного аргумента");
        }
        return std::tan(arg);
    }
    if (name == "ctan") {
        double sinValue = std::sin(arg);
        if (std::abs(sinValue) < kEpsilon) {
            throw std::runtime_error("Котангенс не определён для данного аргумента");
        }
        return std::cos(arg) / sinValue;
    }
    
    // Обратные тригонометрические функции
    if (name == "arcsin") {
        if (arg < -1.0 || arg > 1.0) {
            throw std::runtime_error("arcsin определён только на [-1;1]");
        }
        return std::asin(arg);
    }
    if (name == "arccos") {
        if (arg < -1.0 || arg > 1.0) {
            throw std::runtime_error("arccos определён только на [-1;1]");
        }
        return std::acos(arg);
    }

    throw std::runtime_error("Неизвестная функция: " + name);
}

} // namespace expr
