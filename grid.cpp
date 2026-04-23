#include "grid.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

void GridMesh::initialize(int rowsIn, int colsIn, float sizeIn)
{
    rows = rowsIn;
    cols = colsIn;
    size = sizeIn;

    vertices.clear();
    indices.clear();
    baseXZ.clear();
    uvs.clear();
    waves.clear();

    vertices.reserve((rows + 1) * (cols + 1) * 11);
    indices.reserve(rows * cols * 6);
    baseXZ.reserve((rows + 1) * (cols + 1));
    uvs.reserve((rows + 1) * (cols + 1));

    const float halfSize = size * 0.5f;

    for (int r = 0; r <= rows; ++r)
    {
        float v = static_cast<float>(r) / static_cast<float>(rows);
        float z = -halfSize + size * v;

        for (int c = 0; c <= cols; ++c)
        {
            float u = static_cast<float>(c) / static_cast<float>(cols);
            float x = -halfSize + size * u;

            baseXZ.emplace_back(x, z);
            uvs.emplace_back(u, v);

            // position
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(z);

            // normal
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);

            // tangent
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);

            // uv
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            unsigned int topLeft = r * (cols + 1) + c;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (r + 1) * (cols + 1) + c;
            unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    indexCount = static_cast<GLsizei>(indices.size());

    // A small layered swell set. This is the bridge stage before Phillips/FFT.
    waves.push_back({ 0.11f, 2.40f, 0.30f, glm::normalize(glm::vec2(1.00f,  0.18f)) });
    waves.push_back({ 0.07f, 1.35f, 1.20f, glm::normalize(glm::vec2(1.00f, -0.10f)) });
    waves.push_back({ 0.05f, 0.85f, 2.40f, glm::normalize(glm::vec2(0.92f,  0.28f)) });
    waves.push_back({ 0.03f, 0.48f, 0.70f, glm::normalize(glm::vec2(0.96f, -0.22f)) });
    waves.push_back({ 0.02f, 0.26f, 1.90f, glm::normalize(glm::vec2(1.00f,  0.05f)) });

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(),
        GL_STATIC_DRAW
    );

    constexpr GLsizei stride = 11 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    update(0.0f);
}

void GridMesh::uploadVertexData()
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data()
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GridMesh::update(float time)
{
    constexpr float g = 9.81f;

    const std::size_t vertexCount = baseXZ.size();

    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        const float x = baseXZ[i].x;
        const float z = baseXZ[i].y;

        float height = 0.0f;
        float dHdX = 0.0f;
        float dHdZ = 0.0f;

        for (const Wave& wave : waves)
        {
            const glm::vec2 dir = glm::normalize(wave.direction);
            const float k = glm::two_pi<float>() / wave.wavelength;
            const float omega = std::sqrt(g * k);

            const float theta = k * (dir.x * x + dir.y * z) - omega * time + wave.phase;

            height += wave.amplitude * std::sin(theta);

            const float deriv = wave.amplitude * k * std::cos(theta);
            dHdX += deriv * dir.x;
            dHdZ += deriv * dir.y;
        }

        glm::vec3 normal = glm::normalize(glm::vec3(-dHdX, 1.0f, -dHdZ));
        glm::vec3 tangent = glm::normalize(glm::vec3(1.0f, dHdX, 0.0f));

        const std::size_t base = i * 11;

        // position
        vertices[base + 0] = x;
        vertices[base + 1] = height;
        vertices[base + 2] = z;

        // normal
        vertices[base + 3] = normal.x;
        vertices[base + 4] = normal.y;
        vertices[base + 5] = normal.z;

        // tangent
        vertices[base + 6] = tangent.x;
        vertices[base + 7] = tangent.y;
        vertices[base + 8] = tangent.z;

        // uv stays unchanged
        vertices[base + 9] = uvs[i].x;
        vertices[base + 10] = uvs[i].y;
    }

    uploadVertexData();
}

void GridMesh::draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void GridMesh::destroy()
{
    if (EBO != 0)
    {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }

    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }

    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
}