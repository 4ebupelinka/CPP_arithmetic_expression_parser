// Генератор математических выражений для тестирования.
// Поддерживает скобки и тригонометрические функции (sin, cos, tan, arcsin, arccos).
// Генерирует длинные выражения с редкими ошибками (деление на 0, незакрытые скобки, лишние символы).
//

#pragma once

#include <random>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>

// Список доступных унарных функций
const std::vector<std::string> kFunctions = {
    "sin", "cos", "tan", "arcsin", "arccos"
};

// Вероятность генерации ошибки (5%)
const double ERROR_PROBABILITY = 0.05;

// Простой рекурсивный генератор выражений
class ExpressionGenerator {
public:
    ExpressionGenerator() : gen(rd()), 
        num_dist(-10.0, 10.0), // Небольшие числа для тригонометрии
        op_dist(0, 3), 
        func_dist(0, kFunctions.size() - 1),
        bool_dist(0, 1),
        error_dist(0.0, 1.0),
        error_type_dist(0, 2),
        char_dist(0, 255),
        pos_dist(0, 100) {}

    std::string generate(int depth) {
        // Базовый случай: при нулевой или отрицательной глубине всегда возвращаем число
        if (depth <= 0) {
            return generateNumber();
        }

        // При глубине 1 можем генерировать только функции от чисел или простые числа
        // чтобы избежать бесконечной рекурсии
        if (depth == 1) {
            int type_roll = std::uniform_int_distribution<>(0, 9)(gen);
            if (type_roll < 3) { // 30% - функция от числа
                std::string func = kFunctions[func_dist(gen)];
                std::string arg = generateNumber();
                
                // Для arcsin/arccos аргумент должен быть в [-1, 1]
                if (func == "arcsin" || func == "arccos") {
                    arg = generateNumberInUnitRange();
                }

                std::string result = func + "(" + arg + ")";
                return introduceError(result);
            } else { // 70% - просто число
                return generateNumber();
            }
        }

        // При глубине >= 2 генерируем сложные выражения
        // Изменяем вероятности: 0-15 бинарная операция (80%), 16-18 функция (15%), 19 просто число (5%)
        int type_roll = std::uniform_int_distribution<>(0, 19)(gen);
        
        if (type_roll < 16) { // Бинарная операция: A op B (80%)
            char op = operations[op_dist(gen)];
            std::string left = generate(depth - 1);
            std::string right = generate(depth - 1);
            
            // С маленькой вероятностью делаем деление на ноль (ошибка)
            if (op == '/' && error_dist(gen) < ERROR_PROBABILITY * 0.3) {
                right = "0";
            } else if (op == '/') {
                // Обычная защита от деления на ноль
                right = generateNumber(true);
            }

            std::string result = "(" + left + " " + op + " " + right + ")";
            return introduceError(result);

        } else if (type_roll < 19) { // Функция: func(A) (15%)
            std::string func = kFunctions[func_dist(gen)];
            std::string arg = generate(depth - 1);
            
            // Для arcsin/arccos аргумент должен быть в [-1, 1]
            if (func == "arcsin" || func == "arccos") {
                arg = generateNumberInUnitRange();
            }

            std::string result = func + "(" + arg + ")";
            return introduceError(result);
        } else { // Просто число (5% - редко)
            return generateNumber();
        }
    }

private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> num_dist;
    std::uniform_int_distribution<> op_dist;
    std::uniform_int_distribution<> func_dist;
    std::uniform_int_distribution<> bool_dist;
    std::uniform_real_distribution<> error_dist;
    std::uniform_int_distribution<> error_type_dist;
    std::uniform_int_distribution<> char_dist;
    std::uniform_int_distribution<> pos_dist;

    std::vector<char> operations = {'+', '-', '*', '/'};

    std::string generateNumber(bool avoidZero = false) {
        double num = num_dist(gen);
        if (avoidZero && std::abs(num) < 0.1) {
            num = 1.0;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << num;
        return ss.str();
    }

    std::string generateNumberInUnitRange() {
        std::uniform_real_distribution<> unit_dist(-0.99, 0.99);
        double num = unit_dist(gen);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << num;
        return ss.str();
    }

    // Вносит ошибки в выражение с малой вероятностью
    std::string introduceError(const std::string& expr) {
        if (error_dist(gen) >= ERROR_PROBABILITY) {
            return expr; // Без ошибки
        }

        int errorType = error_type_dist(gen);
        
        switch (errorType) {
            case 0: // Незакрытая скобка - убираем последнюю закрывающую скобку
                {
                    std::string result = expr;
                    size_t lastBracket = result.find_last_of(')');
                    if (lastBracket != std::string::npos) {
                        result.erase(lastBracket, 1);
                    }
                    return result;
                }
            
            case 1: // Лишний символ по середине выражения
                {
                    std::string result = expr;
                    if (result.length() > 2) {
                        // Вставляем случайный символ в середину
                        size_t pos = result.length() / 2;
                        char randomChar = static_cast<char>(char_dist(gen));
                        // Используем только печатные символы
                        if (randomChar < 32 || randomChar > 126) {
                            randomChar = '@';
                        }
                        result.insert(pos, 1, randomChar);
                    }
                    return result;
                }
            
            case 2: // Дополнительная открывающая скобка без закрывающей
                {
                    std::string result = expr;
                    // Вставляем открывающую скобку в случайное место
                    if (result.length() > 1) {
                        size_t pos = std::uniform_int_distribution<>(0, result.length() - 1)(gen);
                        result.insert(pos, "(");
                    }
                    return result;
                }
            
            default:
                return expr;
        }
    }
};

