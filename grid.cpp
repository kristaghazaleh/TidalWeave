#include "grid.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

namespace
{
    inline int wrappedIndex(int i, int n)
    {
        int r = i % n;
        return (r < 0) ? (r + n) : r;
    }
}

void GridMesh::generateSpectralModes()
{
    spectralK.clear();
    spectralOmega.clear();
    h0.clear();

    const float g = 9.81f;
    const glm::vec2 windDir = glm::normalize(windDirection);
    const float L = (windSpeed * windSpeed) / g;
    const float dk = glm::two_pi<float>() / size;
    const int N = spectralResolution;

    spectralK.reserve(static_cast<std::size_t>(N * N));
    spectralOmega.reserve(static_cast<std::size_t>(N * N));
    h0.reserve(static_cast<std::size_t>(N * N));

    std::mt19937 rng(1337u);
    std::normal_distribution<float> gaussian(0.0f, 1.0f);

    for (int iz = 0; iz < N; ++iz)
    {
        for (int ix = 0; ix < N; ++ix)
        {
            const int mx = ix - N / 2;
            const int mz = iz - N / 2;

            const glm::vec2 kVec = dk * glm::vec2(static_cast<float>(mx), static_cast<float>(mz));
            const float k = glm::length(kVec);

            spectralK.push_back(kVec);

            if (k < 1e-6f)
            {
                spectralOmega.push_back(0.0f);
                h0.push_back(std::complex<float>(0.0f, 0.0f));
                continue;
            }

            const glm::vec2 kHat = kVec / k;
            const float alignmentRaw = glm::dot(kHat, windDir);
            const float alignment = std::abs(alignmentRaw);

            float phillips =
                phillipsA *
                std::exp(-1.0f / ((k * L) * (k * L))) /
                std::pow(k, 4.0f) *
                (alignment * alignment);

            phillips *= std::exp(-(k * k) * (smallWaveDamping * smallWaveDamping));

            if (alignmentRaw < 0.0f)
            {
                phillips *= 0.25f;
            }

            if (phillips < 0.0f)
            {
                phillips = 0.0f;
            }

            const float xiR = gaussian(rng);
            const float xiI = gaussian(rng);
            const float scale = std::sqrt(0.5f * phillips);

            h0.push_back(scale * std::complex<float>(xiR, xiI));
            spectralOmega.push_back(std::sqrt(g * k));
        }
    }
}

void GridMesh::initialize(int rowsIn, int colsIn, float sizeIn)
{
    rows = rowsIn;
    cols = colsIn;
    size = sizeIn;

    windDirection = glm::normalize(windDirection);

    vertices.clear();
    indices.clear();
    baseXZ.clear();
    uvs.clear();

    vertices.reserve((rows + 1) * (cols + 1) * 11);
    indices.reserve(rows * cols * 6);
    baseXZ.reserve((rows + 1) * (cols + 1));
    uvs.reserve((rows + 1) * (cols + 1));

    const float halfSize = size * 0.5f;

    for (int r = 0; r <= rows; ++r)
    {
        const float v = static_cast<float>(r) / static_cast<float>(rows);
        const float z = -halfSize + size * v;

        for (int c = 0; c <= cols; ++c)
        {
            const float u = static_cast<float>(c) / static_cast<float>(cols);
            const float x = -halfSize + size * u;

            baseXZ.push_back(glm::vec2(x, z));
            uvs.push_back(glm::vec2(u, v));

            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(z);

            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);

            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);

            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const unsigned int topLeft = r * (cols + 1) + c;
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomLeft = (r + 1) * (cols + 1) + c;
            const unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    indexCount = static_cast<GLsizei>(indices.size());

    generateSpectralModes();

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

    const GLsizei stride = 11 * static_cast<GLsizei>(sizeof(float));

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
    const int N = spectralResolution;
    const std::size_t modeCount = spectralK.size();

    std::vector<std::complex<float>> hkt(modeCount, std::complex<float>(0.0f, 0.0f));

    for (int iz = 0; iz < N; ++iz)
    {
        for (int ix = 0; ix < N; ++ix)
        {
            const int idx = iz * N + ix;
            const int oppX = wrappedIndex(N - ix, N);
            const int oppZ = wrappedIndex(N - iz, N);
            const int oppIdx = oppZ * N + oppX;

            const float omegaT = spectralOmega[idx] * time;
            const std::complex<float> expPos(std::cos(omegaT), std::sin(omegaT));
            const std::complex<float> expNeg(std::cos(omegaT), -std::sin(omegaT));

            hkt[idx] = h0[idx] * expPos + std::conj(h0[oppIdx]) * expNeg;
        }
    }

    const std::size_t vertexCount = baseXZ.size();
    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        const float x = baseXZ[i].x;
        const float z = baseXZ[i].y;

        std::complex<float> h(0.0f, 0.0f);
        std::complex<float> dhdx(0.0f, 0.0f);
        std::complex<float> dhdz(0.0f, 0.0f);

        for (std::size_t m = 0; m < modeCount; ++m)
        {
            const glm::vec2& kVec = spectralK[m];
            if (glm::dot(kVec, kVec) < 1e-12f)
            {
                continue;
            }

            const float phase = kVec.x * x + kVec.y * z;
            const std::complex<float> expIKX(std::cos(phase), std::sin(phase));
            const std::complex<float> contribution = hkt[m] * expIKX;

            h += contribution;
            dhdx += std::complex<float>(0.0f, kVec.x) * contribution;
            dhdz += std::complex<float>(0.0f, kVec.y) * contribution;
        }

        const float height = heightScale * h.real();
        const float dHdX = heightScale * dhdx.real();
        const float dHdZ = heightScale * dhdz.real();

        const glm::vec3 normal = glm::normalize(glm::vec3(-dHdX, 1.0f, -dHdZ));
        const glm::vec3 tangent = glm::normalize(glm::vec3(1.0f, dHdX, 0.0f));

        const std::size_t base = i * 11;

        vertices[base + 0] = x;
        vertices[base + 1] = height;
        vertices[base + 2] = z;

        vertices[base + 3] = normal.x;
        vertices[base + 4] = normal.y;
        vertices[base + 5] = normal.z;

        vertices[base + 6] = tangent.x;
        vertices[base + 7] = tangent.y;
        vertices[base + 8] = tangent.z;

        vertices[base + 9]  = uvs[i].x;
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
