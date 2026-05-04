#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct FogSettings
{
    glm::vec3 position = glm::vec3(0.0f, -0.07f, -1.8f);
    glm::vec3 size = glm::vec3(18.0f, 0.40f, 13.5f);

    glm::vec3 sunsetColor = glm::vec3(0.84f, 0.72f, 0.67f);
    glm::vec3 nightColor  = glm::vec3(0.05f, 0.06f, 0.08f);

    float sunsetDensity = 0.78f;
    float nightDensity  = 0.28f;

    float nearDistance = 1.60f;
    float farDistance  = 17.0f;

    std::array<float, 3> layerHeightOffsets = { 0.00f, 0.025f, 0.060f };
    std::array<float, 3> layerDepthOffsets  = { -0.35f, -2.70f, -5.30f };

    std::array<float, 3> layerWidthScales  = { 0.95f, 1.08f, 1.24f };
    std::array<float, 3> layerHeightScales = { 0.17f, 0.12f, 0.09f };
    std::array<float, 3> layerDepthScales  = { 0.92f, 1.08f, 1.30f };

    std::array<float, 3> layerDensityScales   = { 0.95f, 0.64f, 0.42f };
    std::array<float, 3> layerNoiseScales     = { 0.22f, 0.31f, 0.41f };
    std::array<float, 3> layerAnimationSpeeds = { 0.24f, 0.18f, 0.13f };
    std::array<float, 3> layerAlphaCaps       = { 0.18f, 0.13f, 0.09f };

    std::array<glm::vec2, 3> layerDriftDirs = {
        glm::vec2( 1.00f,  0.16f),
        glm::vec2( 0.64f, -0.18f),
        glm::vec2(-0.28f,  0.08f)
    };
};

class FogRenderer
{
public:
    inline bool initialize(const std::string& vertexPath, const std::string& fragmentPath)
    {
        destroy();

        program_ = createProgramFromFiles(vertexPath, fragmentPath);
        if (program_ == 0)
        {
            return false;
        }

        // x in [-0.5, 0.5], y in [0, 1], z in [0, -1]
        const std::array<float, 8 * 3> vertices = {
            -0.5f, 0.0f,  0.0f, // 0 front bottom left
             0.5f, 0.0f,  0.0f, // 1 front bottom right
             0.5f, 1.0f,  0.0f, // 2 front top right
            -0.5f, 1.0f,  0.0f, // 3 front top left
            -0.5f, 0.0f, -1.0f, // 4 back bottom left
             0.5f, 0.0f, -1.0f, // 5 back bottom right
             0.5f, 1.0f, -1.0f, // 6 back top right
            -0.5f, 1.0f, -1.0f  // 7 back top left
        };

        const std::array<unsigned int, 36> indices = {
            0, 1, 2,  2, 3, 0, // front
            1, 5, 6,  6, 2, 1, // right
            5, 4, 7,  7, 6, 5, // back
            4, 0, 3,  3, 7, 4, // left
            3, 2, 6,  6, 7, 3, // top
            4, 5, 1,  1, 0, 4  // bottom
        };

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * static_cast<GLsizei>(sizeof(float)), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        return true;
    }

    inline void render(
        const glm::mat4& viewProjection,
        const glm::vec3& camPos,
        int environmentMode,
        const FogSettings& settings,
        float timeSeconds) const
    {
        if (program_ == 0 || vao_ == 0)
        {
            return;
        }

        const glm::vec3 baseFogColor =
            (environmentMode == 0) ? settings.sunsetColor : settings.nightColor;

        const float baseFogDensity =
            (environmentMode == 0) ? settings.sunsetDensity : settings.nightDensity;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glUseProgram(program_);

        glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
        glUniform3fv(glGetUniformLocation(program_, "uCamPos"), 1, glm::value_ptr(camPos));
        glUniform3fv(glGetUniformLocation(program_, "uFogColor"), 1, glm::value_ptr(baseFogColor));
        glUniform1f(glGetUniformLocation(program_, "uNearDistance"), settings.nearDistance);
        glUniform1f(glGetUniformLocation(program_, "uFarDistance"), settings.farDistance);
        glUniform1i(glGetUniformLocation(program_, "uEnvironmentMode"), environmentMode);
        glUniform1f(glGetUniformLocation(program_, "uTime"), timeSeconds);

        glBindVertexArray(vao_);

        for (int i = 2; i >= 0; --i)
        {
            glm::vec3 layerPos = settings.position;
            layerPos.y += settings.layerHeightOffsets[i];
            layerPos.z += settings.layerDepthOffsets[i];

            glm::vec3 layerSize = settings.size;
            layerSize.x *= settings.layerWidthScales[i];
            layerSize.y *= settings.layerHeightScales[i];
            layerSize.z *= settings.layerDepthScales[i];

            glm::mat4 model(1.0f);
            model = glm::translate(model, layerPos);
            model = glm::scale(model, layerSize);

            glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(glGetUniformLocation(program_, "uFogDensity"),
                        baseFogDensity * settings.layerDensityScales[i]);
            glUniform1f(glGetUniformLocation(program_, "uNoiseScale"),
                        settings.layerNoiseScales[i]);
            glUniform1f(glGetUniformLocation(program_, "uAnimationSpeed"),
                        settings.layerAnimationSpeeds[i]);
            glUniform2fv(glGetUniformLocation(program_, "uDriftDir"), 1,
                         glm::value_ptr(settings.layerDriftDirs[i]));
            glUniform1f(glGetUniformLocation(program_, "uAlphaCap"),
                        settings.layerAlphaCaps[i]);
            glUniform1i(glGetUniformLocation(program_, "uLayerIndex"), i);

            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }

        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    inline void destroy()
    {
        if (ebo_ != 0)
        {
            glDeleteBuffers(1, &ebo_);
            ebo_ = 0;
        }
        if (vbo_ != 0)
        {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (vao_ != 0)
        {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (program_ != 0)
        {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint program_ = 0;

    inline static std::string readFileToString(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            std::cerr << "Failed to open shader file: " << path << '\n';
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    inline static bool checkShaderCompile(GLuint shader, const char* label)
    {
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << label << " shader compilation failed:\n" << infoLog << '\n';
            return false;
        }

        return true;
    }

    inline static bool checkProgramLink(GLuint program)
    {
        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success)
        {
            char infoLog[1024];
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            std::cerr << "Fog program linking failed:\n" << infoLog << '\n';
            return false;
        }

        return true;
    }

    inline static GLuint createProgramFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
    {
        std::string vertexSourceString = readFileToString(vertexPath);
        std::string fragmentSourceString = readFileToString(fragmentPath);

        if (vertexSourceString.empty() || fragmentSourceString.empty())
        {
            return 0;
        }

        const char* vertexSource = vertexSourceString.c_str();
        const char* fragmentSource = fragmentSourceString.c_str();

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, nullptr);
        glCompileShader(vertexShader);
        if (!checkShaderCompile(vertexShader, "Fog vertex"))
        {
            glDeleteShader(vertexShader);
            return 0;
        }

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
        glCompileShader(fragmentShader);
        if (!checkShaderCompile(fragmentShader, "Fog fragment"))
        {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        if (!checkProgramLink(program))
        {
            glDeleteProgram(program);
            return 0;
        }

        return program;
    }
};
