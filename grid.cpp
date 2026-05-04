#include "grid.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <complex>
#include <random>
#include <vector>

namespace
{
    inline int wrapIndex(int i, int n)
    {
        const int r = i % n;
        return (r < 0) ? (r + n) : r;
    }

    bool isPowerOfTwo(int n)
    {
        return n > 0 && (n & (n - 1)) == 0;
    }

    int greatestPowerOfTwoLE(int n)
    {
        int p = 1;
        while ((p << 1) <= n)
        {
            p <<= 1;
        }
        return p;
    }

    void fft1D(std::vector<std::complex<float>>& a, bool inverse)
    {
        const int n = static_cast<int>(a.size());

        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1)
            {
                j ^= bit;
            }
            j ^= bit;

            if (i < j)
            {
                std::swap(a[i], a[j]);
            }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const float angle = 2.0f * glm::pi<float>() / static_cast<float>(len) * (inverse ? -1.0f : 1.0f);
            const std::complex<float> wLen(std::cos(angle), std::sin(angle));

            for (int i = 0; i < n; i += len)
            {
                std::complex<float> w(1.0f, 0.0f);
                for (int j = 0; j < len / 2; ++j)
                {
                    const std::complex<float> u = a[i + j];
                    const std::complex<float> v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wLen;
                }
            }
        }

        if (inverse)
        {
            const float invN = 1.0f / static_cast<float>(n);
            for (int i = 0; i < n; ++i)
            {
                a[i] *= invN;
            }
        }
    }

    void inverseFFT2D(const std::vector<std::complex<float>>& input, int N, std::vector<float>& output)
    {
        std::vector<std::complex<float>> data = input;
        std::vector<std::complex<float>> scratch(static_cast<std::size_t>(N));

        for (int z = 0; z < N; ++z)
        {
            for (int x = 0; x < N; ++x)
            {
                scratch[x] = data[static_cast<std::size_t>(z) * N + x];
            }
            fft1D(scratch, true);
            for (int x = 0; x < N; ++x)
            {
                data[static_cast<std::size_t>(z) * N + x] = scratch[x];
            }
        }

        for (int x = 0; x < N; ++x)
        {
            for (int z = 0; z < N; ++z)
            {
                scratch[z] = data[static_cast<std::size_t>(z) * N + x];
            }
            fft1D(scratch, true);
            for (int z = 0; z < N; ++z)
            {
                data[static_cast<std::size_t>(z) * N + x] = scratch[z];
            }
        }

        output.resize(static_cast<std::size_t>(N) * N);
        for (int z = 0; z < N; ++z)
        {
            for (int x = 0; x < N; ++x)
            {
                output[static_cast<std::size_t>(z) * N + x] = data[static_cast<std::size_t>(z) * N + x].real();
            }
        }
    }

    float samplePeriodicField(const std::vector<float>& field, int N, float x, float z, float patchSize)
    {
        float u = (x / patchSize) + 0.5f;
        float v = (z / patchSize) + 0.5f;

        u -= std::floor(u);
        v -= std::floor(v);

        const float gx = u * static_cast<float>(N);
        const float gz = v * static_cast<float>(N);

        const int x0 = static_cast<int>(std::floor(gx)) % N;
        const int z0 = static_cast<int>(std::floor(gz)) % N;
        const int x1 = (x0 + 1) % N;
        const int z1 = (z0 + 1) % N;

        const float tx = gx - std::floor(gx);
        const float tz = gz - std::floor(gz);

        const float f00 = field[static_cast<std::size_t>(z0) * N + x0];
        const float f10 = field[static_cast<std::size_t>(z0) * N + x1];
        const float f01 = field[static_cast<std::size_t>(z1) * N + x0];
        const float f11 = field[static_cast<std::size_t>(z1) * N + x1];

        const float a = (1.0f - tx) * f00 + tx * f10;
        const float b = (1.0f - tx) * f01 + tx * f11;
        return (1.0f - tz) * a + tz * b;
    }
}

void GridMesh::setWaveActivity(float value)
{
    waveActivity = glm::clamp(value, 0.10f, 1.45f);
}

void GridMesh::adjustWaveActivity(float delta)
{
    setWaveActivity(waveActivity + delta);
}

void GridMesh::generateSpectralModes()
{
    spectralK.clear();
    spectralOmega.clear();
    h0.clear();

    if (!isPowerOfTwo(spectralResolution))
    {
        spectralResolution = greatestPowerOfTwoLE(spectralResolution);
    }

    const float g = 9.81f;
    const glm::vec2 windDir = glm::normalize(windDirection);
    const float L = (windSpeed * windSpeed) / g;
    const float dk = glm::two_pi<float>() / size;
    const int N = spectralResolution;

    const std::size_t modeCount = static_cast<std::size_t>(N) * N;
    spectralK.reserve(modeCount);
    spectralOmega.reserve(modeCount);
    h0.reserve(modeCount);

    std::mt19937 rng(1337u);
    std::normal_distribution<float> gaussian(0.0f, 1.0f);

    for (int iz = 0; iz < N; ++iz)
    {
        for (int ix = 0; ix < N; ++ix)
        {
            const int mx = (ix < N / 2) ? ix : ix - N;
            const int mz = (iz < N / 2) ? iz : iz - N;

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
            const float alignment = std::max(alignmentRaw, 0.0f);

            float phillips =
                phillipsA *
                std::exp(-1.0f / ((k * L) * (k * L))) /
                std::pow(k, 4.0f) *
                (alignment * alignment);

            phillips *= std::exp(-(k * k) * (smallWaveDamping * smallWaveDamping));

            if (alignmentRaw < 0.0f)
            {
                phillips *= 0.07f;
            }

            phillips = std::max(phillips, 0.0f);

            const float xiR = gaussian(rng);
            const float xiI = gaussian(rng);
            const float scale = std::sqrt(0.5f * phillips);

            h0.push_back(scale * std::complex<float>(xiR, xiI));
            spectralOmega.push_back(std::sqrt(g * k));
        }
    }

    heightSpectrum.resize(modeCount);
    slopeXSpectrum.resize(modeCount);
    slopeZSpectrum.resize(modeCount);
    heightField.resize(modeCount);
    slopeXField.resize(modeCount);
    slopeZField.resize(modeCount);
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

    for (int iz = 0; iz < N; ++iz)
    {
        for (int ix = 0; ix < N; ++ix)
        {
            const int idx = iz * N + ix;
            const int oppX = wrapIndex(-ix, N);
            const int oppZ = wrapIndex(-iz, N);
            const int oppIdx = oppZ * N + oppX;

            const float omegaT = spectralOmega[idx] * time;
            const std::complex<float> expPos(std::cos(omegaT), std::sin(omegaT));
            const std::complex<float> expNeg(std::cos(omegaT), -std::sin(omegaT));

            const std::complex<float> hkt = h0[idx] * expPos + std::conj(h0[oppIdx]) * expNeg;
            heightSpectrum[static_cast<std::size_t>(idx)] = hkt;

            const glm::vec2& kVec = spectralK[static_cast<std::size_t>(idx)];
            slopeXSpectrum[static_cast<std::size_t>(idx)] = std::complex<float>(0.0f, kVec.x) * hkt;
            slopeZSpectrum[static_cast<std::size_t>(idx)] = std::complex<float>(0.0f, kVec.y) * hkt;
        }
    }

    inverseFFT2D(heightSpectrum, N, heightField);
    inverseFFT2D(slopeXSpectrum, N, slopeXField);
    inverseFFT2D(slopeZSpectrum, N, slopeZField);

    const float fieldScale = heightScale * waveActivity * static_cast<float>(N * N);

    const std::size_t vertexCount = baseXZ.size();
    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        const float x = baseXZ[i].x;
        const float z = baseXZ[i].y;

        const float height = fieldScale * samplePeriodicField(heightField, N, x, z, size);
        const float dHdX = fieldScale * samplePeriodicField(slopeXField, N, x, z, size);
        const float dHdZ = fieldScale * samplePeriodicField(slopeZField, N, x, z, size);

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
