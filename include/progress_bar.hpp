#pragma once

#include <atomic>
#include <cstddef>

// Функция отображения прогресс-бара.
// Запускается в отдельном потоке.
void displayProgress(std::atomic<std::size_t>& completed, std::size_t total);

