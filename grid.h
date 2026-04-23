#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

struct Wave
{
    float amplitude = 0.0f;
    float wavelength = 1.0f;
    float phase = 0.0f;
    glm::vec2 direction = glm::vec2(1.0f, 0.0f);
};

struct GridMesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;

    int rows = 0;
    int cols = 0;
    float size = 0.0f;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<glm::vec2> baseXZ;
    std::vector<glm::vec2> uvs;
    std::vector<Wave> waves;

    void initialize(int rowsIn, int colsIn, float sizeIn);
    void update(float time);
    void draw() const;
    void destroy();

private:
    void uploadVertexData();
};