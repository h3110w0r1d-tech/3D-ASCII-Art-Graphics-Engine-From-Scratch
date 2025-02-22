/*

            clang++ -o cube cube.cpp

*/


#include <iostream>
#include <chrono>
#include <vector>

const int gridWidth = 124;
const int gridHeight = 70;


char grid[gridHeight][gridWidth];
std::string frontBuffer;
std::string backBuffer;

const float vertices[] = {
    -0.5f, -0.5f,
    0.5f, -0.5f,
    0.5f, 0.5f,
    -0.5f, 0.5f
};

const int numVertices = sizeof(vertices);

int mapToGrid(float coord, int maxIndex) {
    return static_cast<int>((coord + 1.0f) * 0.5f * maxIndex);
}

void clearScreen() {
    std::cout << "\033[H";
}

void render() {
    backBuffer.clear();
    backBuffer.reserve(gridHeight * (gridWidth + 1));

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[y][x] = ' ';
        }
    }

    for (int i = 0; i < numVertices; i += 2) {
        int xi = mapToGrid(vertices[i], gridWidth);
        int yi = mapToGrid(vertices[i +1], gridHeight);
        grid[yi][xi] = '#';
    }

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            backBuffer += grid[y][x];
        }
        backBuffer += '\n';
    }

    std::swap(frontBuffer, backBuffer);

    std::cout << frontBuffer;
}

int main() {
    auto previousTime = std::chrono::steady_clock::now();

    try {
        while (true) {
            clearScreen();

            auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsedTime = currentTime - previousTime;
            float deltaTime = elapsedTime.count();
            previousTime = currentTime;

            render();

            std::cout << deltaTime;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}