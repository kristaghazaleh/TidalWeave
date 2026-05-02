#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "grid.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

std::string readFileToString(const std::string& path)
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

bool checkShaderCompile(GLuint shader, const char* label)
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

bool checkProgramLink(GLuint program)
{
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << '\n';
        return false;
    }

    return true;
}

GLuint createShaderProgramFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
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

    if (!checkShaderCompile(vertexShader, "Vertex"))
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    if (!checkShaderCompile(fragmentShader, "Fragment"))
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!checkProgramLink(shaderProgram))
    {
        glDeleteProgram(shaderProgram);
        return 0;
    }

    return shaderProgram;
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "TidalWeave", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << '\n';

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GLuint shaderProgram = createShaderProgramFromFiles("shaders/ocean.vert", "shaders/ocean.frag");
    if (shaderProgram == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GridMesh grid;
    grid.initialize(180, 180, 7.0f);

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        const float timeValue = static_cast<float>(glfwGetTime());
        grid.update(timeValue);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        const float aspect = (framebufferHeight > 0)
            ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight)
            : 16.0f / 9.0f;

        glm::vec3 camPos(0.0f, 0.80f, 2.65f);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(
            camPos,
            glm::vec3(0.0f, 0.10f, -0.65f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        glm::mat4 projection = glm::perspective(
            glm::radians(42.0f),
            aspect,
            0.1f,
            100.0f
        );
        glm::mat4 viewProjection = projection * view;

        glClearColor(0.02f, 0.04f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glUniform1f(glGetUniformLocation(shaderProgram, "uTime"), timeValue);
        glUniform3fv(glGetUniformLocation(shaderProgram, "uCamPos"), 1, glm::value_ptr(camPos));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uViewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));

        grid.draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    grid.destroy();
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
