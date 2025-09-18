#include <iostream>
#include <thread>
#include <chrono>

// Global health
int g_health = 2500;

// Exported function for hooking (use extern "C" to avoid mangling)
extern "C" __declspec(dllexport) void reduceHealth(int dmg) {
    g_health -= dmg;
    std::cout << "Health reduced! Current health: " << g_health << std::endl;
    if (g_health <= 0) {
        std::cout << "Game Over!" << std::endl;
    }
}

void gameLoop() {
    std::cout << "Game started. Initial health: " << g_health << std::endl;
    while (g_health > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        reduceHealth(10);  // Simulate a hit
    }
    std::cout << "Game ended." << std::endl;
}

int main() {
    gameLoop();
    return 0;
}