/*
            clang++ -o cube cube.cpp
            g++ -o cube cube.cpp
*/


#include <iostream>
#include <chrono>
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <thread>
#include <windows.h>

const int gridWidth = 124;
const int gridHeight = 70;
const int refreshRate = 20;


char grid[gridHeight][gridWidth];
std::string frontBuffer;
std::string backBuffer;

float zBuffer[gridHeight][gridWidth];

// Camera Varibles
glm::mat4 transform = glm::mat4(1.0f);
glm::vec3 cameraPos = glm::vec3(2.0f, 0.0f, 2.0f);
glm::vec3 lookAt = cameraPos + glm::vec3(-1.0f, 0.0f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);


float cameraSpeed = 5.0f;
float cameraSpeedRight = 3.0f;


float cameraRotationX = 0.0f;
float cameraRotationY = 0.0f;


float nearPlane = 0.1f;
float farPlane = 7.0f;


float fov = glm::radians(90.0f);
float aspectRatio = static_cast<float>(gridWidth) / static_cast<float>(gridHeight);
glm::mat4 cameraRotation = glm::rotate(glm::mat4(1.0f), cameraRotationY, glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis rotation
glm::vec3 forward = glm::normalize(glm::vec3(cameraRotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))); // Forward direction
glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp)); // Right direction


// Cube Varibles

float angle;
glm::mat4 cubeRotation = glm::mat4(1.0f);
glm::vec3 cubePosition(0.0f, 0.0f, 2.0f);

const float vertices[] = {
    // Front face 
    -0.5f, -0.5f,  0.5f,  // 0: Front-bottom-left
     0.5f, -0.5f,  0.5f,  // 1: Front-bottom-right
     0.5f,  0.5f,  0.5f,  // 2: Front-top-right
    -0.5f,  0.5f,  0.5f,  // 3: Front-top-left


    // Back face
    -0.5f, -0.5f, -0.5f,  // 4: Back-bottom-left
     0.5f, -0.5f, -0.5f,  // 5: Back-bottom-right
     0.5f,  0.5f, -0.5f,  // 6: Back-top-right
    -0.5f,  0.5f, -0.5f   // 7: Back-top-left
};


const unsigned int indices[] = {
    // Front face
    0, 1, 2,
    2, 3, 0,


    // Back face
    4, 5, 6,
    6, 7, 4,


    // Left face
    4, 0, 3,
    3, 7, 4,


    // Right face
    1, 5, 6,
    6, 2, 1,


    // Top face
    3, 2, 6,
    6, 7, 3,


    // Bottom face
    4, 5, 1,
    1, 0, 4
};

float transformedVertices[24];

const int numVertices = sizeof(vertices) / sizeof(vertices[0]);

int mapToGrid(float coord, int maxIndex) {
    return static_cast<int>((coord + 1.0f) * 0.5f * maxIndex);
}

void clearScreen() {
    std::cout << "\033[H";
}

std::vector<std::tuple<int, int, float>> triangulate(const float* vertices, int numVertices, const unsigned int* indices, int numIndices) {
    std::vector<std::tuple<int, int, float>> triangles;

    for (int i = 0; i < numIndices; i += 3) {
        int index1 = indices[i] * 3;
        int index2 = indices[i+1] * 3;
        int index3 = indices[i + 2] * 3;

        if (index1 < 0 || index1 >= numVertices || index2 < 0 || index2 >=numVertices || index3 < 0 || index3 >=  numVertices) {
            continue;
        }

        int x1 = mapToGrid(vertices[index1], gridWidth);
        int y1 = mapToGrid(vertices[index1 + 1], gridHeight);
        float z1 = vertices[index1 + 2];

        int x2 = mapToGrid(vertices[index2], gridWidth);
        int y2 = mapToGrid(vertices[index2 + 1], gridHeight);
        float z2 = vertices[index2 + 2];

        int x3 = mapToGrid(vertices[index3], gridWidth);
        int y3 = mapToGrid(vertices[index3 + 1], gridHeight);
        float z3 = vertices[index3 + 2];

        if ((x1 < 0 || x1 >= gridWidth || y1 < 0 || y1 >= gridHeight) ||
            (x2 < 0 || x2 >= gridWidth || y2 < 0 || y2 >= gridHeight) ||
            (x3 < 0 || x3 >= gridWidth || y3 < 0 || y3 >= gridHeight))
        {
            continue;
        }

        triangles.emplace_back(x1, y1, z1);
        triangles.emplace_back(x2, y2, z2);
        triangles.emplace_back(x3, y3, z3);
    }

    return triangles;
}

void drawLine(const std::tuple<int, int, float>& p1, const std::tuple<int, int, float>& p2) {
    int x1 = std::get<0>(p1), y1 = std::get<1>(p1);
    int x2 = std::get<0>(p2), y2 = std::get<1>(p2);
    float z1 = std::get<2>(p1), z2 = std::get<2>(p2);

    int dx = x2 - x1;
    int dy = y2 - y1;

    double hyptonuse = sqrt(pow(dx, 2) + pow(dy, 2));

    int steps = static_cast<int>(hyptonuse);

    for (int i = 0; i <= steps; i++) {
        int x = static_cast<int>(x1 + (dx * i) /steps);
        int y = static_cast<int>(y1 + (dy * i) /steps);
        int z = z1 + (z1 - z1) * (static_cast<float>(i) / steps) + 1e-6f;

        if (x >= 0 && x < gridWidth && y >= 0 && y < gridWidth) {
            if (z < zBuffer[y][x]) {
                zBuffer[y][x] = z;
                grid[y][x] = '*';
            }
        }
        else if (z == zBuffer[y][x]) {
            continue;
        }
    }
}

void applyTransform(glm::mat4& transform) {
    for (int i = 0; i < sizeof(transformedVertices) / sizeof(transformedVertices[0]); i += 3) {
        glm::vec4 vertex = glm::vec4(vertices[i], vertices[i+1], vertices[i+2], 1.0f);
        vertex = transform * vertex;

        if (std::abs(vertex.w) > 1e-6f) {
            vertex.x /= vertex.w;
            vertex.y /= vertex.w;
            vertex.z /= vertex.w;

            if (vertex.z < nearPlane && vertex.z == nearPlane) {
                continue;
            }

            if (vertex.z > farPlane && vertex.z == farPlane) {
                continue;
            }
        }
        else {
            continue;
        }

        if (vertex.z < -1.0f || vertex.z > 1.0f) {
            transformedVertices[i] = transformedVertices[i+1] = transformedVertices[i+2] = 2.0f;
            continue;
        }

        transformedVertices[i] = vertex.x;
        transformedVertices[i+1] = vertex.y;
        transformedVertices[i+2] = vertex.z;
    }
}

void render(const std::vector<std::tuple<int, int, float>> & triangles) {
    backBuffer.clear();
    backBuffer.reserve(gridHeight * (gridWidth + 1));

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[y][x] = ' ';
            zBuffer[y][x] = 1.0f;
        }
    }

    for (size_t i = 0; i < triangles.size(); i += 3) {
        drawLine(triangles[i], triangles[i+1]);
        drawLine(triangles[i+1], triangles[i+2]);
        drawLine(triangles[i+2], triangles[i]);
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

bool debounceKey(int keyCode) {
    return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
}

void calculateCamRot() {
    cameraRotation = glm::rotate(glm::mat4(1.0f), cameraRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    cameraRotation = glm::rotate(cameraRotation, cameraRotationX, glm::vec3(1.0f, 0.0f, 0.0f));

    forward = glm::normalize(glm::vec3(cameraRotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    right = glm::normalize(glm::cross(forward, cameraUp));
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

            bool cameraRotationUpdated = false;

            if (debounceKey(VK_LEFT)) {
                cameraRotationY += 5 * deltaTime;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_RIGHT)) {
                cameraRotationY -= 5 * deltaTime;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_UP)) {
                cameraRotationX -= 5 * deltaTime;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_DOWN)) {
                cameraRotationX += 5 * deltaTime;
                cameraRotationUpdated = true;
            }

            if (debounceKey('A')) {
                cameraPos -= right * cameraSpeedRight * deltaTime;
            }
            if (debounceKey('D')) {
                cameraPos += right * cameraSpeedRight * deltaTime;
            }
            if (debounceKey('W')) {
                cameraPos += forward * cameraSpeed * deltaTime;
            }
            if (debounceKey('S')) {
                cameraPos -= forward * cameraSpeed * deltaTime;
            }

            if (cameraRotationUpdated) {
                calculateCamRot();
            }

            lookAt = cameraPos + forward;

            angle += 1.0f * deltaTime;

            glm::mat4 view = glm::lookAt(cameraPos, lookAt, cameraUp);
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
            cubeRotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 1.0f, 1.0f));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cubePosition) * cubeRotation;
            glm::mat4 MVP = projection * view * model;

            applyTransform(MVP);
            int numVertices = sizeof(transformedVertices) / sizeof(transformedVertices[0]);
            int numIndices = sizeof(indices) / sizeof(indices[0]);
            std::vector<std::tuple<int, int, float>> triangles = triangulate(transformedVertices, numVertices, indices, numIndices);

            render(triangles);

            std::this_thread::sleep_for(std::chrono::milliseconds(refreshRate));
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
