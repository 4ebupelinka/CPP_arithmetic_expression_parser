#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace expr {

// Пул потоков для параллельной обработки задач.
// Позволяет избежать накладных расходов на создание потоков для каждой задачи.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount);
    ~ThreadPool();

    // Добавляет новую задачу в очередь.
    // Возвращает std::future для получения результата выполнения.
    template <class Func, class... Args>
    auto enqueue(Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>>;

private:
    std::vector<std::thread> workers;          // Рабочие потоки
    std::queue<std::function<void()>> tasks;   // Очередь задач
    
    std::mutex mutex;                          // Мьютекс для синхронизации доступа к очереди
    std::condition_variable condition;         // Условная переменная для уведомления потоков
    bool stop = false;                         // Флаг остановки пула

    // Основной цикл рабочего потока
    void workerLoop();
};

// Реализация шаблона enqueue
template <class Func, class... Args>
inline auto ThreadPool::enqueue(Func&& func, Args&&... args)
    -> std::future<std::invoke_result_t<Func, Args...>> {
    using Return = std::invoke_result_t<Func, Args...>;

    // Упаковываем задачу в packaged_task для сохранения результата в future
    auto task = std::make_shared<std::packaged_task<Return()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    std::future<Return> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stop) {
            throw std::runtime_error("Пул потоков уже остановлен");
        }
        // Добавляем задачу в очередь, оборачивая в void() лямбду
        tasks.emplace([task]() { (*task)(); });
    }

    condition.notify_one(); // Будим один из спящих потоков
    return res;
}

} 
