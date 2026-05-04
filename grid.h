#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <complex>
#include <vector>

struct GridMesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;

    int rows = 0;
    int cols = 0;
    float size = 0.0f;

    glm::vec2 windDirection = glm::normalize(glm::vec2(1.0f, 0.18f));
    float windSpeed = 18.0f;

    // FFT grid resolution. Best results come from powers of two.
    int spectralResolution = 32;
    float phillipsA = 0.0009f;
    float smallWaveDamping = 0.08f;
    float heightScale = 1.85f;
    float waveActivity = 1.0f;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<glm::vec2> baseXZ;
    std::vector<glm::vec2> uvs;

    std::vector<glm::vec2> spectralK;
    std::vector<float> spectralOmega;
    std::vector<std::complex<float>> h0;

    std::vector<std::complex<float>> heightSpectrum;
    std::vector<std::complex<float>> slopeXSpectrum;
    std::vector<std::complex<float>> slopeZSpectrum;

    std::vector<float> heightField;
    std::vector<float> slopeXField;
    std::vector<float> slopeZField;

    void initialize(int rowsIn, int colsIn, float sizeIn);
    void update(float time);
    void draw() const;
    void destroy();

    void setWaveActivity(float value);
    void adjustWaveActivity(float delta);

private:
    void generateSpectralModes();
    void uploadVertexData();
};
