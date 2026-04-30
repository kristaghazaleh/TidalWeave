#pragma once

enum class SkyMode { Default = 0, Daytime = 1, Nighttime = 2 };

struct GuiState
{
    SkyMode mode = SkyMode::Default;

    void init();
    void update(struct GLFWwindow* window, int& prev1, int& prev2, int& prev3);
    void handleClick(double mouseX, double mouseY, int winW, int winH);
    void drawSky(float uTime) const;
    void draw() const;
    void destroy();

    int  skyModeInt() const { return static_cast<int>(mode); }
    void getClearColor(float& r, float& g, float& b) const;

private:
    unsigned int _prog    = 0;
    unsigned int _skyProg = 0;
    unsigned int _vao     = 0;
    unsigned int _vbo     = 0;
};
