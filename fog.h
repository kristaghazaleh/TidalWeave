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
    glm::vec3 position = glm::vec3(0.0f, -0.04f, -2.0f);   // front-bottom-center
    glm::vec3 size = glm::vec3(15.0f, 0.90f, 9.5f);        // x width, y height, z depth(backward)
    glm::vec3 sunsetColor = glm::vec3(0.92f, 0.74f, 0.68f);
    glm::vec3 nightColor = glm::vec3(0.10f, 0.13f, 0.21f);
    float sunsetDensity = 0.95f;
    float nightDensity = 0.62f;
    float nearDistance = 1.7f;
    float farDistance = 9.5f;
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

    inline void render(const glm::mat4& viewProjection, const glm::vec3& camPos, int environmentMode, const FogSettings& settings) const
    {
        if (program_ == 0 || vao_ == 0)
        {
            return;
        }

        const glm::vec3 fogColor = (environmentMode == 0) ? settings.sunsetColor : settings.nightColor;
        const float fogDensity = (environmentMode == 0) ? settings.sunsetDensity : settings.nightDensity;

        glm::mat4 model(1.0f);
        model = glm::translate(model, settings.position);
        model = glm::scale(model, settings.size);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glUseProgram(program_);
        glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
        glUniform3fv(glGetUniformLocation(program_, "uCamPos"), 1, glm::value_ptr(camPos));
        glUniform3fv(glGetUniformLocation(program_, "uFogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(program_, "uFogDensity"), fogDensity);
        glUniform1f(glGetUniformLocation(program_, "uNearDistance"), settings.nearDistance);
        glUniform1f(glGetUniformLocation(program_, "uFarDistance"), settings.farDistance);

        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
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
