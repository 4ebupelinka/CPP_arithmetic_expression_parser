#pragma once

#include "csv_writer.hpp"
#include "evaluator.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <vector>
#include <cstddef>
#include <functional>

// Структура для хранения исходной строки выражения с ее номером
struct ExpressionLine {
    std::size_t number;
    std::string text;
};

// Потоковое чтение и обработка файла по частям (chunks) для экономии памяти
// Читает файл порциями и сразу отправляет задачи в пул потоков
// Обрабатывает futures батчами и вызывает callback для записи результатов
// Это позволяет обрабатывать файлы любого размера без загрузки всего файла в память
template<typename ProcessCallback>
void processExpressionsStreaming(
    const std::filesystem::path& path,
    expr::ExpressionEvaluator& evaluator,
    expr::ThreadPool& pool,
    std::atomic<std::size_t>& completed,
    std::atomic<std::size_t>& totalLines,
    ProcessCallback&& processBatch,
    std::size_t chunkSize = 10000,  // Обрабатываем по 10000 строк за раз
    std::size_t batchSize = 1000) {  // Обрабатываем futures батчами по 1000

    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Не удалось открыть входной файл");
    }

    // Увеличиваем размер буфера для чтения (1 МБ) для ускорения
    // Важно: pubsetbuf должен быть вызван ДО первого чтения из потока
    constexpr std::size_t bufferSize = 1024 * 1024;
    char* fileBuffer = new char[bufferSize];
    input.rdbuf()->pubsetbuf(fileBuffer, bufferSize);

    std::vector<ExpressionLine> chunk;
    chunk.reserve(chunkSize);
    std::vector<std::future<expr::EvaluationRecord>> futures;
    futures.reserve(batchSize);

    std::string buffer;
    std::size_t lineNumber = 1;

    auto processFuturesBatch = [&]() {
        if (futures.empty()) return;

        // Собираем результаты батча
        std::vector<expr::EvaluationRecord> batch;
        batch.reserve(futures.size());
        for (auto& future : futures) {
            batch.push_back(future.get());
        }

        // Вызываем callback для обработки батча
        processBatch(batch);

        // Очищаем futures
        futures.clear();
        futures.reserve(batchSize);
    };

    while (std::getline(input, buffer)) {
        chunk.push_back({ lineNumber++, std::move(buffer) });
        // Не обновляем totalLines, так как оно уже известно из подсчета

        // Когда накопили достаточно строк, отправляем в обработку
        if (chunk.size() >= chunkSize) {
            // Отправляем весь chunk в пул потоков
            for (const auto& expressionLine : chunk) {
                futures.emplace_back(pool.enqueue(
                    [expressionLine, &evaluator, &completed]() -> expr::EvaluationRecord {
                        expr::EvaluationRecord record;
                        record.lineNumber = expressionLine.number;
                        record.expression = expressionLine.text;
                        try {
                            if (expressionLine.text.empty()) {
                                throw std::runtime_error("Пустая строка");
                            }
                            // Основная логика вычисления
                            double value = evaluator.evaluate(expressionLine.text);
                            record.value = value;
                            record.status = "success";
                            record.message = "";
                        }
                        catch (const std::exception& ex) {
                            record.value.reset();
                            record.status = "error";
                            record.message = ex.what();
                        }
                        completed.fetch_add(1); // Обновляем прогресс
                        return record;
                    }));

                // Когда накопили достаточно futures, обрабатываем батч
                if (futures.size() >= batchSize) {
                    processFuturesBatch();
                }
            }
            // Очищаем chunk для следующей порции
            chunk.clear();
            chunk.reserve(chunkSize);
        }
    }

    // Обрабатываем оставшиеся строки (если их меньше chunkSize)
    if (!chunk.empty()) {
        for (const auto& expressionLine : chunk) {
            futures.emplace_back(pool.enqueue(
                [expressionLine, &evaluator, &completed]() -> expr::EvaluationRecord {
                    expr::EvaluationRecord record;
                    record.lineNumber = expressionLine.number;
                    record.expression = expressionLine.text;
                    try {
                        if (expressionLine.text.empty()) {
                            throw std::runtime_error("Пустая строка");
                        }
                        // Основная логика вычисления
                        double value = evaluator.evaluate(expressionLine.text);
                        record.value = value;
                        record.status = "success";
                        record.message = "";
                    }
                    catch (const std::exception& ex) {
                        record.value.reset();
                        record.status = "error";
                        record.message = ex.what();
                    }
                    completed.fetch_add(1); // Обновляем прогресс
                    return record;
                }));

            // Когда накопили достаточно futures, обрабатываем батч
            if (futures.size() >= batchSize) {
                processFuturesBatch();
            }
        }
    }

    // Обрабатываем оставшиеся futures
    processFuturesBatch();

    delete[] fileBuffer;
}

