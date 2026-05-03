#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "grid.h"
#include "fog.h"

namespace
{
    enum class EnvironmentMode
    {
        Sunset = 0,
        Night = 1
    };

    struct HdrImage
    {
        int width = 0;
        int height = 0;
        std::vector<float> pixels;
    };
}

static const char* kSkyVertexShader = R"GLSL(
#version 330 core
out vec2 vNdc;
void main()
{
    vec2 p;
    if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) p = vec2( 3.0, -1.0);
    else p = vec2(-1.0,  3.0);
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

static const char* kSkyFragmentShader = R"GLSL(
#version 330 core
uniform sampler2D uEnvironmentMap;
uniform int uEnvironmentMode;
uniform mat4 uInvProjection;
uniform mat4 uInvView;
uniform float uEnvRotation;
uniform float uEnvPitch;
uniform float uEnvExposure;
in vec2 vNdc;
out vec4 out_color;
const float INV_PI = 0.31830988618;
const float INV_TWO_PI = 0.15915494309;

vec3 rotateAroundX(vec3 v, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(
        v.x,
        c * v.y - s * v.z,
        s * v.y + c * v.z
    );
}

vec2 directionToEquirectUV(vec3 dir)
{
    dir = rotateAroundX(normalize(dir), uEnvPitch);
    float u = atan(-dir.z, dir.x) * INV_TWO_PI + 0.5 + uEnvRotation;
    u = fract(u);
    float v = acos(clamp(dir.y, -1.0, 1.0)) * INV_PI;
    return vec2(u, v);
}

vec3 toneMap(vec3 c)
{
    c *= uEnvExposure;
    c = c / (vec3(1.0) + c);
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 proceduralSky(vec3 dir)
{
    float t = clamp(0.5 * (dir.y + 1.0), 0.0, 1.0);
    if (uEnvironmentMode == 0)
    {
        vec3 horizon = vec3(0.92, 0.67, 0.60);
        vec3 zenith = vec3(0.30, 0.18, 0.28);
        return mix(horizon, zenith, pow(t, 0.70));
    }
    else
    {
        vec3 horizon = vec3(0.06, 0.08, 0.14);
        vec3 zenith = vec3(0.00, 0.02, 0.06);
        return mix(horizon, zenith, pow(t, 0.80));
    }
}

void main()
{
    vec4 clip = vec4(vNdc, 1.0, 1.0);
    vec4 viewPos = uInvProjection * clip;
    vec3 viewDir = normalize(viewPos.xyz / viewPos.w);
    vec3 worldDir = normalize((uInvView * vec4(viewDir, 0.0)).xyz);

    vec3 env = toneMap(texture(uEnvironmentMap, directionToEquirectUV(worldDir)).rgb);
    vec3 sky = proceduralSky(worldDir);

    float envMix = (uEnvironmentMode == 0) ? 0.94 : 0.995;
    vec3 color = mix(sky, env, envMix);
    out_color = vec4(color, 1.0);
}
)GLSL";

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

GLuint createShaderProgramFromSource(const char* vertexSource, const char* fragmentSource)
{
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
    return createShaderProgramFromSource(vertexSource, fragmentSource);
}

bool decodeRGBE(const unsigned char rgbe[4], float& r, float& g, float& b)
{
    if (rgbe[3] == 0)
    {
        r = g = b = 0.0f;
        return true;
    }

    const float scale = std::ldexp(1.0f, static_cast<int>(rgbe[3]) - (128 + 8));
    r = static_cast<float>(rgbe[0]) * scale;
    g = static_cast<float>(rgbe[1]) * scale;
    b = static_cast<float>(rgbe[2]) * scale;
    return true;
}

bool loadRadianceHDR(const std::string& path, HdrImage& image)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open HDR file: " << path << '\n';
        return false;
    }

    std::string line;
    bool formatFound = false;

    while (std::getline(file, line))
    {
        if (line.empty())
        {
            break;
        }

        if (line.rfind("FORMAT=32-bit_rle_rgbe", 0) == 0)
        {
            formatFound = true;
        }
    }

    if (!formatFound)
    {
        std::cerr << "Unsupported HDR format in: " << path << '\n';
        return false;
    }

    std::string resolutionLine;
    std::getline(file, resolutionLine);

    int width = 0;
    int height = 0;
    if (sscanf_s(resolutionLine.c_str(), "-Y %d +X %d", &height, &width) != 2)
    {
        std::cerr << "Failed to parse HDR resolution in: " << path << '\n';
        return false;
    }

    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width) * height * 3);

    std::vector<unsigned char> scanline(static_cast<std::size_t>(width) * 4);

    for (int y = 0; y < height; ++y)
    {
        unsigned char header[4];
        file.read(reinterpret_cast<char*>(header), 4);
        if (!file)
        {
            std::cerr << "Unexpected EOF in HDR scanline header: " << path << '\n';
            return false;
        }

        if (header[0] != 2 || header[1] != 2 || (header[2] & 0x80) != 0 || (((int)header[2] << 8) | header[3]) != width)
        {
            std::cerr << "Unsupported non-RLE HDR layout in: " << path << '\n';
            return false;
        }

        for (int channel = 0; channel < 4; ++channel)
        {
            int x = 0;
            while (x < width)
            {
                unsigned char count = 0;
                file.read(reinterpret_cast<char*>(&count), 1);
                if (!file)
                {
                    std::cerr << "Unexpected EOF in HDR RLE data: " << path << '\n';
                    return false;
                }

                if (count > 128)
                {
                    const int runLength = static_cast<int>(count) - 128;
                    unsigned char value = 0;
                    file.read(reinterpret_cast<char*>(&value), 1);
                    if (!file)
                    {
                        std::cerr << "Unexpected EOF in HDR RLE run: " << path << '\n';
                        return false;
                    }

                    for (int i = 0; i < runLength; ++i)
                    {
                        scanline[static_cast<std::size_t>(x + i) * 4 + channel] = value;
                    }
                    x += runLength;
                }
                else
                {
                    const int runLength = static_cast<int>(count);
                    for (int i = 0; i < runLength; ++i)
                    {
                        unsigned char value = 0;
                        file.read(reinterpret_cast<char*>(&value), 1);
                        if (!file)
                        {
                            std::cerr << "Unexpected EOF in HDR literal run: " << path << '\n';
                            return false;
                        }
                        scanline[static_cast<std::size_t>(x + i) * 4 + channel] = value;
                    }
                    x += runLength;
                }
            }
        }

        for (int x = 0; x < width; ++x)
        {
            const unsigned char* rgbe = &scanline[static_cast<std::size_t>(x) * 4];
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            decodeRGBE(rgbe, r, g, b);

            const std::size_t dst = (static_cast<std::size_t>(y) * width + x) * 3;
            image.pixels[dst + 0] = r;
            image.pixels[dst + 1] = g;
            image.pixels[dst + 2] = b;
        }
    }

    return true;
}

GLuint createHDRTexture(const std::string& path)
{
    HdrImage image;
    if (!loadRadianceHDR(path, image))
    {
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, image.width, image.height, 0, GL_RGB, GL_FLOAT, image.pixels.data());

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
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

    GLuint oceanProgram = createShaderProgramFromFiles("shaders/ocean.vert", "shaders/ocean.frag");
    GLuint skyProgram = createShaderProgramFromSource(kSkyVertexShader, kSkyFragmentShader);
    if (oceanProgram == 0 || skyProgram == 0)
    {
        glDeleteProgram(oceanProgram);
        glDeleteProgram(skyProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const GLuint sunsetTexture = createHDRTexture("hdr/belfast_sunset_puresky_4k.hdr");
    const GLuint nightTexture = createHDRTexture("hdr/qwantani_night_puresky_4k.hdr");

    if (sunsetTexture == 0 || nightTexture == 0)
    {
        std::cerr << "Failed to load one or more HDR environment maps.\n";
        glDeleteProgram(oceanProgram);
        glDeleteProgram(skyProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint skyVAO = 0;
    glGenVertexArrays(1, &skyVAO);

    EnvironmentMode envMode = EnvironmentMode::Sunset;

    GridMesh grid;
    grid.initialize(480, 480, 6.0f);

    FogRenderer fog;
    if (!fog.initialize("shaders/fog.vert", "shaders/fog.frag"))
    {
        glDeleteVertexArrays(1, &skyVAO);
        glDeleteTextures(1, &sunsetTexture);
        glDeleteTextures(1, &nightTexture);
        glDeleteProgram(oceanProgram);
        glDeleteProgram(skyProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    FogSettings fogSettings;
    fogSettings.position = glm::vec3(0.0f, -0.04f, -(grid.size * 0.35f));
    fogSettings.size = glm::vec3(grid.size * 2.6f, 0.92f, grid.size * 1.65f);
    fogSettings.nearDistance = 1.7f;
    fogSettings.farDistance = 9.5f;
    fogSettings.sunsetDensity = 0.95f;
    fogSettings.nightDensity = 0.62f;
    fogSettings.sunsetColor = glm::vec3(0.92f, 0.74f, 0.68f);
    fogSettings.nightColor = glm::vec3(0.10f, 0.13f, 0.21f);

    float sunsetRotation = 0.02f;
    float sunsetPitch = 0.00f;
    float sunsetExposure = 1.55f;

    float nightRotation = 0.00f;
    float nightPitch = 0.00f;
    float nightExposure = 7.50f;

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window))
    {
        const float now = static_cast<float>(glfwGetTime());
        const float deltaTime = now - lastTime;
        lastTime = now;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        {
            envMode = EnvironmentMode::Sunset;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        {
            envMode = EnvironmentMode::Night;
        }

        float* activeRotation = (envMode == EnvironmentMode::Sunset) ? &sunsetRotation : &nightRotation;
        float* activePitch = (envMode == EnvironmentMode::Sunset) ? &sunsetPitch : &nightPitch;
        float* activeExposure = (envMode == EnvironmentMode::Sunset) ? &sunsetExposure : &nightExposure;

        const float rotationSpeed = 0.25f;
        const float pitchSpeed = 0.60f;
        const float exposureSpeed = 2.50f;

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  { *activeRotation -= rotationSpeed * deltaTime; }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { *activeRotation += rotationSpeed * deltaTime; }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    { *activePitch += pitchSpeed * deltaTime; }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  { *activePitch -= pitchSpeed * deltaTime; }
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)   { *activeExposure += exposureSpeed * deltaTime; }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) { *activeExposure -= exposureSpeed * deltaTime; }

        *activePitch = glm::clamp(*activePitch, -1.2f, 1.2f);
        *activeExposure = glm::clamp(*activeExposure, 0.05f, 20.0f);

        const float timeValue = now;
        grid.update(timeValue);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        const float aspect = (framebufferHeight > 0)
            ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight)
            : 16.0f / 9.0f;

        glm::vec3 camPos(0.0f, 0.58f, 2.42f);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(
            camPos,
            glm::vec3(0.0f, 0.10f, -0.55f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        glm::mat4 projection = glm::perspective(
            glm::radians(42.0f),
            aspect,
            0.1f,
            100.0f
        );
        glm::mat4 viewProjection = projection * view;
        glm::mat4 invProjection = glm::inverse(projection);
        glm::mat4 invView = glm::inverse(view);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLuint envTexture = (envMode == EnvironmentMode::Sunset) ? sunsetTexture : nightTexture;
        float envRotation = (envMode == EnvironmentMode::Sunset) ? sunsetRotation : nightRotation;
        float envPitch = (envMode == EnvironmentMode::Sunset) ? sunsetPitch : nightPitch;
        float envExposure = (envMode == EnvironmentMode::Sunset) ? sunsetExposure : nightExposure;

        glDepthMask(GL_FALSE);
        glUseProgram(skyProgram);
        glUniformMatrix4fv(glGetUniformLocation(skyProgram, "uInvProjection"), 1, GL_FALSE, glm::value_ptr(invProjection));
        glUniformMatrix4fv(glGetUniformLocation(skyProgram, "uInvView"), 1, GL_FALSE, glm::value_ptr(invView));
        glUniform1i(glGetUniformLocation(skyProgram, "uEnvironmentMode"), static_cast<int>(envMode));
        glUniform1f(glGetUniformLocation(skyProgram, "uEnvRotation"), envRotation);
        glUniform1f(glGetUniformLocation(skyProgram, "uEnvPitch"), envPitch);
        glUniform1f(glGetUniformLocation(skyProgram, "uEnvExposure"), envExposure);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, envTexture);
        glUniform1i(glGetUniformLocation(skyProgram, "uEnvironmentMap"), 0);
        glBindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);

        glUseProgram(oceanProgram);
        glUniform1f(glGetUniformLocation(oceanProgram, "uTime"), timeValue);
        glUniform3fv(glGetUniformLocation(oceanProgram, "uCamPos"), 1, glm::value_ptr(camPos));
        glUniform1i(glGetUniformLocation(oceanProgram, "uEnvironmentMode"), static_cast<int>(envMode));
        glUniform1f(glGetUniformLocation(oceanProgram, "uEnvRotation"), envRotation);
        glUniform1f(glGetUniformLocation(oceanProgram, "uEnvPitch"), envPitch);
        glUniform1f(glGetUniformLocation(oceanProgram, "uEnvExposure"), envExposure);
        glUniformMatrix4fv(glGetUniformLocation(oceanProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(oceanProgram, "uViewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, envTexture);
        glUniform1i(glGetUniformLocation(oceanProgram, "uEnvironmentMap"), 0);
        grid.draw();

        fog.render(viewProjection, camPos, static_cast<int>(envMode), fogSettings);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    fog.destroy();
    grid.destroy();
    glDeleteVertexArrays(1, &skyVAO);
    glDeleteTextures(1, &sunsetTexture);
    glDeleteTextures(1, &nightTexture);
    glDeleteProgram(oceanProgram);
    glDeleteProgram(skyProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
