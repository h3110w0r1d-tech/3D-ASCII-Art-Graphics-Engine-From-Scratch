/*
        clang++ -O3 -o cube cube.cpp
        g++ -O3 -o cube cube.cpp
*/


#include <iostream>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <windows.h>  // For GetAsyncKeyState
#include <tuple>
#include <atomic>


const int gridWidth = 124;
const int gridHeight = 70;
const int refreshRate = 100;


// The grid which is the amount of characters taking up terminal for the height and width
char grid[gridHeight][gridWidth];
std::string frontBuffer; // Buffer currently being displayed
std::string backBuffer;  // Buffer being written to

double zBuffer[gridHeight][gridWidth];

// Camera Varibles, matrices and vectors
glm::mat4 transform = glm::mat4(1.0f);
glm::vec3 cameraPos = glm::vec3(2.0f, 0.0f, 2.0f);
glm::vec3 lookAt = cameraPos + glm::vec3(-1.0f, 0.0f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);


float cameraSpeed = 0.00050f;
float cameraSpeedRight = 0.0003f;


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
glm::vec3 cubePosition(0.0f, 0.0f, 0.0f);


// Lighting varibles and vectors
glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -1.0f, -1.0f));
glm::vec3 lightPosition = glm::vec3(1.0f, -1.0f, 0.0f);


// Vertices for a pyramid (position only)
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


// Copy of vertices for transformations
float transformedVertices[24];


int mapToGrid(float coord, int maxIndex) {
    // Map NDC (-1,1) range to (0, maxIndex)
    return static_cast<int>((coord + 1.0f) * 0.5f * maxIndex);
}


// Clears screen every frame
void clearScreen() {
    std::cout << "\033[H";
}

void applyTransform(glm::mat4& transform) {
    for (int i = 0; i < sizeof(transformedVertices) / sizeof(transformedVertices[0]); i += 3) {
        glm::vec4 vertex = glm::vec4(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);
        vertex = transform * vertex;


        // Perform perspective divide to get NDC
        if (std::abs(vertex.w) > 1e-6f)
        {
            vertex.x /= vertex.w;
            vertex.y /= vertex.w;
            vertex.z /= vertex.w;

            // Clip to the near and far planes
            if (vertex.z < nearPlane)
            {
                continue; // Skip this vertex if it's too close
            }
            if (vertex.z > farPlane)
            {
                continue; // Skip this vertex if it's too far
            }
            if (vertex.z == nearPlane)
            {
                continue; // Skip this vertex if it is equal to the nearPlane or in other words too close
            }
            if (vertex.z == farPlane)
            {
                continue; // Skip this vertex if it is equal to the farPlane or in other words too far
            }
        }
        else
        {
            continue;
        }

        // Skip vertices that are outside the NDC range [-1, 1] for the z-axis (clipping range)
        if (vertex.z < -1.0f || vertex.z > 1.0f)
        {
            // Set to out-of-bounds values or skip the update
            transformedVertices[i] = transformedVertices[i + 1] = transformedVertices[i + 2] = 2.0f; // Out-of-bounds
            continue;
        }

        // Assigns the respective vertice to its transformed value
        transformedVertices[i]     = vertex.x;
        transformedVertices[i + 1] = vertex.y;
        transformedVertices[i + 2] = vertex.z;
    }
}


glm::vec3 calculateNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& cameraPos) {
    glm::vec3 normal = glm::normalize(glm::cross(p2 - p1, p3 - p1)); // Calculates the normal of the triangle
    glm::vec3 viewDir = glm::normalize(cameraPos - p1); // Finds the view direction of the camera
    if (glm::dot(normal, viewDir) > 0)
    {
        normal = -normal; // Flip the normal if it's pointing towards the camera
    }
    return normal;
}


void fillTriangle(const std::tuple<int, int, float>& p1, const std::tuple<int, int, float>& p2, const std::tuple<int, int, float>& p3) {
    int x1 = std::get<0>(p1), y1 = std::get<1>(p1); // Assigns x1 and y1 into p1
    int x2 = std::get<0>(p2), y2 = std::get<1>(p2); // Assigns x2 and y2 into p2
    int x3 = std::get<0>(p3), y3 = std::get<1>(p3); // Assigns x3 and y3 into p3


    float z1 = std::get<2>(p1), z2 = std::get<2>(p2), z3 = std::get<2>(p3); // Assigns z1 into p1, z2 into p2 and z3 into p3


    // Sort vertices by y-coordinate (y1 <= y2 <= y3)
    if (y2 < y1)
    {
        std::swap(y1, y2); std::swap(x1, x2); std::swap(z1, z2);
    }


    if (y3 < y1)
    {
        std::swap(y1, y3); std::swap(x1, x3); std::swap(z1, z3);
    }
   
    if (y3 < y2)
    {
        std::swap(y2, y3); std::swap(x2, x3); std::swap(z2, z3);
    }


    glm::vec3 point1 = glm::vec3(static_cast<float>(x1), static_cast<float>(y1), z1); // Establishes point1
    glm::vec3 point2 = glm::vec3(static_cast<float>(x2), static_cast<float>(y2), z2); // Establishes point2
    glm::vec3 point3 = glm::vec3(static_cast<float>(x3), static_cast<float>(y3), z3); // Establishes point3
    glm::vec3 normal = calculateNormal(point1, point2, point3, cameraPos); // Calculates the normal for the triangle


    // Interpolation helper
    auto interpolate = [](int y, int y0, int y1, int x0, int x1, float z0, float z1) -> std::tuple<int, float> {
        if (y1 == y0) return {x0, z0}; // Checks if y1 = y0 and if it is, we already know that x1 will be equal to x0 and z1 will be equal to z0
        float t = static_cast<float>(y - y0) / (y1 - y0); // Finds the interpolation factor which changes y
        int x = static_cast<int>(x0 + t * (x1 - x0)); // Uses t to interpolate x
        float z = z0 + t * (z1 - z0); // Uses t to interpolate y
        return {x, z};
    };


    float angleIntensity = glm::dot(normal, -lightDirection); // Calculates the lighting of the triangle by using the normal and comparing it to see how it is pointing at the light source
    angleIntensity = std::pow(glm::clamp(angleIntensity, 0.0f, 1.0f), 1.5f);


    // Iterates over every y cooridinate from y1 to y2 which is the upper segment of the triangle
    for (int y = y1; y <= y2; y++) {
        auto [xa, za] = interpolate(y, y1, y3, x1, x3, z1, z3); // Finds the x and z cooridinates on the left side of the trinagle by using the interpolated value based on the y cooridinate
        auto [xb, zb] = interpolate(y, y1, y2, x1, x2, z1, z2); // Finds the x and z cooridinates on the right side of the trinagle by using the interpolated value based on the y cooridinate


        if (xa > xb)
        {
            std::swap(xa, xb); std::swap(za, zb);
        }


        // Iterates over x cooridinate between the x cooridinate on the left side to the x cooridinate on the right side
        for (int x = xa; x <= xb; x++) {
            float z = za + (zb - za) * (static_cast<float>(x - xa) / (xb - xa + 1e-6f)); // Calculates z by interpolating the difference between the left to the right side of the triangle based on the x cooridinates
            if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight && z < zBuffer[y][x])  // Checks if the cooridnates are inbounds
            {
                zBuffer[y][x] = z; // Sets the new zBuffer
               
                glm::vec3 Pos = glm::vec3(x, y, z); // Setting the position as a vector so we can normalize it
                glm::vec3 normPos = glm::normalize(Pos); // Normalize for lighting calculations only


                float dx = normPos.x - lightPosition.x; // Calculates the difference between the x position of the pixel and x position of the lightposition
                float dy = normPos.y - lightPosition.y; // Calculates the difference between the y position of the pixel and y position of the lightposition
                float dz = normPos.z - lightPosition.z; // Calculates the difference between the z position of the pixel and z position of the lightposition


                float distance = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2)); // Uses the pythagorous thereom to calculate the distance bewteen the lightposition and the pixel position
                float maxDistance = 50.0f;
                float clampedDistance = glm::clamp(distance, 1.0f, maxDistance); // Clamps the distance between 1 and 50


                // Smoother falloff for distance attenuation (inverse-square law approximation)
                float distanceIntensity = 1.0f / (clampedDistance * clampedDistance);


                // Combine the two factors
                float intensity = angleIntensity * distanceIntensity;
               
               
                // Choose character based on intensity
                char shadingChar = (intensity > 0.13f ) ? '@' :
                                   (intensity > 0.125f && intensity < 0.13f) ? '#' :
                                   (intensity > 0.5f && intensity < 0.125f) ? '*' :
                                   (intensity <= 0.5f) ? '.' : '.';
                grid[y][x] = shadingChar; // Sets the pixels in that position to the assigned colour
            }
        }
    }


    // Lower part of triangle
    for (int y = y2; y <= y3; y++) {
        auto [xa, za] = interpolate(y, y1, y3, x1, x3, z1, z3); // Finds the x and z cooridinates on the left side of the trinagle by using the interpolated value based on the y cooridinate
        auto [xb, zb] = interpolate(y, y2, y3, x2, x3, z2, z3); // Finds the x and z cooridinates on the right side of the trinagle by using the interpolated value based on the y cooridinate
       
        if (xa > xb)
        {
            std::swap(xa, xb); std::swap(za, zb);
        }


        // Iterates over x cooridinate between the x cooridinate on the left side to the x cooridinate on the right side
        for (int x = xa; x <= xb; x++) {
            float z = za + (zb - za) * (static_cast<float>(x - xa) / (xb - xa + 1e-6f));
            if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight && z < zBuffer[y][x])
            {
                zBuffer[y][x] = z;


                glm::vec3 Pos = glm::vec3(x, y, z); // Setting the position as a vector so we can normalize it
                glm::vec3 normPos = glm::normalize(Pos); // Normalize for lighting calculations only


                float dx = normPos.x - lightPosition.x; // Calculates the difference between the x position of the pixel and x position of the lightposition
                float dy = normPos.y - lightPosition.y; // Calculates the difference between the y position of the pixel and y position of the lightposition
                float dz = normPos.z - lightPosition.z; // Calculates the difference between the z position of the pixel and z position of the lightposition


                float distance = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2)); // Uses the pythagorous thereom to calculate the distance bewteen the lightposition and the pixel position
                float maxDistance = 50.0f;
                float clampedDistance = glm::clamp(distance, 1.0f, maxDistance); // Clamps the distance between 1 and 50


                // Smoother falloff for distance attenuation (inverse-square law approximation)
                float distanceIntensity = 1.0f / (clampedDistance * clampedDistance);


                // Combine the two factors
                float intensity = angleIntensity * distanceIntensity;
               
               
                // Choose character based on intensity
                char shadingChar = (intensity > 0.13f ) ? '@' :
                                   (intensity > 0.125f && intensity < 0.13f) ? '#' :
                                   (intensity > 0.5f && intensity < 0.125f) ? '*' :
                                   (intensity <= 0.5f) ? '.' : '.';
                grid[y][x] = shadingChar; // Sets the pixels in that position to the assigned colour
            }
        }
    }
}




void drawLine(const std::tuple<int, int, float>& p1, const std::tuple<int, int, float>& p2) {
    int x1 = std::get<0>(p1), y1 = std::get<1>(p1); // Assigns x1 and y1 to p1
    int x2 = std::get<0>(p2), y2 = std::get<1>(p2); // Assigns x2 and y2 to p2
    float z1 = std::get<2>(p1), z2 = std::get<2>(p2); // Assigns z1 to p1 and z2 to p2


    int dx = x2 - x1; // Finds the difference between the first x cooridiante and the second x coordidinate
    int dy = y2 - y1; // Finds the difference between the first y cooridiante and the second y coordidinate
   
    double hypotenuse = sqrt(pow(dx, 2) + pow(dy, 2)); // Finds the hypotenuse of the legs which gives us the distance between them


    int steps = static_cast<int>(hypotenuse);


    for (int i = 0; i <= steps; i++) {
        int x = static_cast<int>(x1 + (dx * i) / steps); // Calculates the x cooridiante
        int y = static_cast<int>(y1 + (dy * i) / steps); // Calculates the y cooridiante
        float z = z1 + (z2 - z1) * (static_cast<float>(i) / steps) + 1e-6f;
 // Calculates the z cooridinate


        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) // Checks if x and y is on the grid
        {
            // Depth test
            if (z < zBuffer[y][x])
            {
                zBuffer[y][x] = z;  // Update the Z-buffer
                grid[y][x] = '*';   // Draw pixel if closer than what's in the Z-buffer
            }
            if (z == zBuffer[y][x])
            {
                continue;
            }
        }
    }
}


void render(const std::vector<std::tuple<int, int, float>>& triangles) {
    // Resize the back buffer to the grid dimensions and clear it
    backBuffer.clear();
    backBuffer.reserve(gridHeight * (gridWidth + 1)); // Preallocate space for performance


    // Iterates over every position in the grid and assigns default values
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[y][x] = ' ';
            zBuffer[y][x] = 1.0f; // Use a large value
        }
    }


    // Process each triangle
    for (size_t i = 0; i < triangles.size(); i += 3) {
        glm::vec3 v0(transformedVertices[i * 3], transformedVertices[i * 3 + 1], transformedVertices[i * 3 + 2]);
        glm::vec3 v1(transformedVertices[(i + 1) * 3], transformedVertices[(i + 1) * 3 + 1], transformedVertices[(i + 1) * 3 + 2]);
        glm::vec3 v2(transformedVertices[(i + 2) * 3], transformedVertices[(i + 2) * 3 + 1], transformedVertices[(i + 2) * 3 + 2]);


        fillTriangle(triangles[i], triangles[i + 1], triangles[i + 2]);
    }


    // Build the back buffer from the grid
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            backBuffer += grid[y][x];
        }
        backBuffer += '\n';
    }


    // Swap buffers
    std::swap(frontBuffer, backBuffer);


    // Display the front buffer
    std::cout << frontBuffer;
}


std::vector<std::tuple<int, int, float>> triangulateWithIndices(const float* vertices, int numVertices, const unsigned int* indices, int numIndices) {
    std::vector<std::tuple<int, int, float>> triangles; // Creates a vector with the cooridinates of each vertex


    // Iterates over every 3 indices
    for (int i = 0; i < numIndices; i += 3) {
        int index1 = indices[i] * 3; // Makes index equal to the x cooridinate of the respective point which is i
        int index2 = indices[i + 1] * 3; // Makes index equal to the y cooridinate of the respective point which is i
        int index3 = indices[i + 2] * 3; // Makes index equal to the z cooridinate of the respective point which is i


        if (index1 < 0 || index1 >= numVertices || index2 < 0 || index2 >= numVertices || index3 < 0 || index3 >= numVertices)
        {
            continue; // Skip invalid indices.
        }


        int x1 = mapToGrid(vertices[index1], gridWidth); // index1 in this example represents the x cooridinate of the first point of the triangle. The vertices array picks the correct float off the list by the number calculated for index1
        int y1 = mapToGrid(vertices[index1 + 1], gridHeight); // index1 in this example represents the y cooridinate of the first point of the triangle
        float z1 = vertices[index1 + 2]; // index1 in this example represents the z cooridinate of the first point of the triangle


        int x2 = mapToGrid(vertices[index2], gridWidth); // index2 in this example represents the x cooridinate of the second point of the triangle. The vertices array picks the correct float off the list by the number calculated for index2
        int y2 = mapToGrid(vertices[index2 + 1], gridHeight); // index2 in this example represents the y cooridinate of the second point of the triangle
        float z2 = vertices[index2 + 2]; // index2 in this example represents the z cooridinate of the second point of the triangle


        int x3 = mapToGrid(vertices[index3], gridWidth); // index3 in this example represents the x cooridinate of the third point of the triangle. The vertices array picks the correct float off the list by the number calculated for index3
        int y3 = mapToGrid(vertices[index3 + 1], gridHeight); // index3 in this example represents the y cooridinate of the third point of the triangle
        float z3 = vertices[index3 + 2]; // index3 in this example represents the z cooridinate of the third point of the triangle


        // Skip vertices that are off the screen (outside grid bounds)
        if ((x1 < 0 || x1 >= gridWidth || y1 < 0 || y1 >= gridHeight) ||
            (x2 < 0 || x2 >= gridWidth || y2 < 0 || y2 >= gridHeight) ||
            (x3 < 0 || x3 >= gridWidth || y3 < 0 || y3 >= gridHeight))
        {
            continue;
        }


        triangles.emplace_back(x1, y1, z1); // Places the cooridinates for the first point in the triangle into triangles
        triangles.emplace_back(x2, y2, z2); // Places the cooridinates for the second point in the triangle into triangles
        triangles.emplace_back(x3, y3, z3); // Places the cooridinates for the third point in the triangle into triangles
    }


    return triangles;
}


bool debounceKey(int keyCode) {
    return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
}


void calculateCamRot()
{
    // Update camera rotation matrix based on current rotation angles
    cameraRotation = glm::rotate(glm::mat4(1.0f), cameraRotationY, glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis rotation
    cameraRotation = glm::rotate(cameraRotation, cameraRotationX, glm::vec3(1.0f, 0.0f, 0.0f)); // X-axis rotation


    // Update the forward and right directions based on the new rotation
    forward = glm::normalize(glm::vec3(cameraRotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))); // Forward direction
    right = glm::normalize(glm::cross(forward, cameraUp)); // Right direction
}


int main() {
    auto previousTime = std::chrono::steady_clock::now();


    try
    {
        while (true) {
            clearScreen();


            // Flag to track if the rotation matrix needs updating
            bool cameraRotationUpdated = false;


            // Update camera rotation values based on user input
            if (debounceKey(VK_LEFT))
            {
                cameraRotationY += 0.0005f;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_RIGHT))
            {
                cameraRotationY -= 0.0005f;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_UP))
            {
                cameraRotationX -= 0.0003f;
                cameraRotationUpdated = true;
            }
            if (debounceKey(VK_DOWN))
            {
                cameraRotationX += 0.0003f;
                cameraRotationUpdated = true;
            }


            // Only update the camera rotation matrix if a rotation key was pressed
            if (cameraRotationUpdated)
            {
                calculateCamRot();
            }


            // Camera movement input using cached forward and right directions
            if (debounceKey('A'))
            {
                cameraPos -= right * cameraSpeedRight; // Move left
            }
            if (debounceKey('D'))
            {
                cameraPos += right * cameraSpeedRight; // Move right
            }
            if (debounceKey('W'))
            {
                cameraPos += forward * cameraSpeed; // Move forward
            }
            if (debounceKey('S'))
            {
                cameraPos -= forward * cameraSpeed; // Move backward
            }


            // Calculates deltaTime
            auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsedTime = currentTime - previousTime;
            float deltaTime = elapsedTime.count(); // Use the actual elapsed time without capping
            previousTime = currentTime;


            // Rotate the cube slightly after each frame
            angle += 1.0f * deltaTime; // Adjust the constant to control rotation speed

            // Update the lookAt position to always face forward relative to the camera's movement
            lookAt = cameraPos + forward;


            // Create the view matrix
            glm::mat4 view = glm::lookAt(cameraPos, lookAt, cameraUp);


            // Calculate the transformation matrices
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
            cubeRotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 1.0f, 1.0f));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cubePosition) * cubeRotation;
            glm::mat4 MVP = projection * view * model;


            // Apply transformations and draw the updated cube
            applyTransform(MVP);
            int numVertices = sizeof(transformedVertices) / sizeof(transformedVertices[0]);
            std::vector<std::tuple<int, int, float>> triangles = triangulateWithIndices(transformedVertices, numVertices, indices, sizeof(indices) / sizeof(indices[0]));


            // Prints the final product
            render(triangles);


            std::cout << deltaTime;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }


    return 0;
}



