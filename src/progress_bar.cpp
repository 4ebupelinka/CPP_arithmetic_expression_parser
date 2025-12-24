#include "progress_bar.hpp"
#include "console.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

// Функция отображения прогресс-бара.
void displayProgress(std::atomic<std::size_t>& completed, std::size_t total) {
    const int barWidth = 50;
    while (completed < total) {
        std::size_t current = completed.load();
        float progress = static_cast<float>(current) / total;
        int pos = static_cast<int>(barWidth * progress);

        std::cout << "\r  " << Color::CYAN << "[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "█";
            else if (i == pos) std::cout << "▒";
            else std::cout << "░";
        }
        std::cout << "] " << Color::BOLD << std::setw(3) << static_cast<int>(progress * 100.0f)
            << "%" << Color::RESET << " (" << current << "/" << total << ")";
        std::cout.flush();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // Финальное обновление до 100%
    std::cout << "\r  " << Color::GREEN << "[";
    for (int i = 0; i < barWidth; ++i) std::cout << "█";
    std::cout << "] " << Color::BOLD << "100%" << Color::RESET
        << " (" << total << "/" << total << ")\n";
}

