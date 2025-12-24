#include "thread_pool.hpp"

namespace expr {

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) {
        threadCount = 1;
    }
    
    // Запуск рабочих потоков
    workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true; // Устанавливаем флаг остановки
    }
    
    // Будим все спящие потоки, чтобы они могли завершиться
    condition.notify_all();
    
    // Ожидаем завершения всех потоков
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// Логика работы отдельного потока
void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex);
            
            // Ожидаем появления задачи или сигнала остановки
            condition.wait(lock, [this]() { return stop || !tasks.empty(); });
            
            // Если нужно остановиться и задач больше нет — выходим
            if (stop && tasks.empty()) {
                return;
            }
            
            // Забираем задачу из очереди
            task = std::move(tasks.front());
            tasks.pop();
        }
        
        // Выполняем задачу вне блокировки мьютекса
        task();
    }
}

} 
