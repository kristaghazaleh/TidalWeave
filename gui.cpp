#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "gui.h"
#include <iostream>

// ── Button layout (NDC) ───────────────────────────────────────────────────────
struct Rect { float x0, y0, x1, y1; };
static const Rect kButtons[3] = {
    { 0.73f, 0.84f, 0.97f, 0.96f },   // 0 Default
    { 0.73f, 0.68f, 0.97f, 0.80f },   // 1 Day
    { 0.73f, 0.52f, 0.97f, 0.64f },   // 2 Night
};
static const char* kBtnLabel[3] = { "Default", "Day", "Night" };

// ── Flat-colour shader ────────────────────────────────────────────────────────
static const char* kVert = R"(#version 330 core
layout(location=0) in vec2 aPos;
void main(){ gl_Position=vec4(aPos,0,1); })";

static const char* kFrag = R"(#version 330 core
uniform vec4 uColor; out vec4 o; void main(){ o=uColor; })";

// ── Sky background shader ─────────────────────────────────────────────────────
static const char* kSkyVert = R"(#version 330 core
out vec2 vUv;
void main(){
    vec2 p=vec2((gl_VertexID&1)*2-1,(gl_VertexID>>1)*2-1);
    vUv=p*0.5+0.5; gl_Position=vec4(p,0.999,1);
})";

static const char* kSkyFrag = R"(#version 330 core
uniform int uMode; uniform float uTime;
in vec2 vUv; out vec4 o;
void main(){
    float y=vUv.y;
    vec3 col;
    if(uMode==1){
        col=mix(vec3(0.72,0.88,1.0),vec3(0.20,0.52,0.92),y);
        // sun
        float d=length(vUv-vec2(0.78,0.80));
        col=mix(vec3(1.0,0.95,0.55),col,smoothstep(0.0,0.055,d));
        col+=vec3(1.0,0.85,0.3)*max(0.0,0.10-d)*5.0;   // glow
    } else if(uMode==2){
        // deep navy gradient
        col=mix(vec3(0.04,0.05,0.12),vec3(0.00,0.00,0.04),pow(y,0.6));
        // moon crescent
        vec2 mp=vec2(0.20,0.82);
        float d1=length(vUv-mp), d2=length(vUv-(mp+vec2(0.030,0.010)));
        float moon=smoothstep(0.046,0.042,d1)-smoothstep(0.040,0.036,d2);
        col=mix(col,vec3(0.96,0.97,0.90),moon);
        // moon glow
        col+=vec3(0.55,0.60,0.75)*max(0.0,0.12-length(vUv-mp))*1.8;
        // stars – three density layers
        for(int lyr=0;lyr<3;lyr++){
            float sc=40.0+float(lyr)*55.0;
            vec2 cell=floor(vUv*sc);
            float rnd=fract(sin(dot(cell,vec2(127.1+float(lyr)*31.0,311.7+float(lyr)*17.3)))*43758.5);
            if(rnd>0.88){
                float brightness=pow((rnd-0.88)/0.12,1.5);
                vec2 uv2=fract(vUv*sc)-0.5;
                float disc=smoothstep(0.18,0.0,length(uv2));
                // subtle colour tint
                vec3 tint=mix(vec3(0.7,0.8,1.0),vec3(1.0,0.95,0.8),rnd);
                col+=tint*disc*brightness*(lyr==0?1.0:0.55);
            }
        }
    } else {
        col=mix(vec3(0.52,0.66,0.82),vec3(0.06,0.16,0.34),y);
    }
    o=vec4(col,1);
})";

static GLuint compile(GLenum t, const char* src){
    GLuint s=glCreateShader(t); glShaderSource(s,1,&src,nullptr); glCompileShader(s); return s;
}
static GLuint link(const char* vs, const char* fs){
    GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f);
    glLinkProgram(p); glDeleteShader(v); glDeleteShader(f); return p;
}

void GuiState::init()
{
    _prog    = link(kVert, kFrag);
    _skyProg = link(kSkyVert, kSkyFrag);

    glGenVertexArrays(1,&_vao); glGenBuffers(1,&_vbo);
    glBindVertexArray(_vao); glBindBuffer(GL_ARRAY_BUFFER,_vbo);
    glBufferData(GL_ARRAY_BUFFER,12*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0);
    glEnableVertexAttribArray(0); glBindVertexArray(0);
}

static void quad(GLuint vbo, const Rect& r, GLint loc, float R,float G,float B,float A){
    float v[12]={r.x0,r.y0, r.x1,r.y0, r.x1,r.y1, r.x0,r.y0, r.x1,r.y1, r.x0,r.y1};
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(v),v);
    glUniform4f(loc,R,G,B,A); glDrawArrays(GL_TRIANGLES,0,6);
}

void GuiState::drawSky(float uTime) const
{
    if (skyModeInt() == 0) return;  // Default: just use plain clear color
    glDisable(GL_DEPTH_TEST);
    glUseProgram(_skyProg);
    glUniform1i(glGetUniformLocation(_skyProg,"uMode"), skyModeInt());
    glUniform1f(glGetUniformLocation(_skyProg,"uTime"), uTime);
    // Draw without a VBO – vertex positions generated in vertex shader via gl_VertexID
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glEnable(GL_DEPTH_TEST);
}

void GuiState::draw() const
{
    // buttons removed – mode switched via keys 1/2/3
}

void GuiState::destroy(){
    glDeleteBuffers(1,&_vbo); glDeleteVertexArrays(1,&_vao);
    glDeleteProgram(_prog); glDeleteProgram(_skyProg);
}

void GuiState::handleClick(double mouseX,double mouseY,int winW,int winH){
    float nx=float(mouseX/winW*2.0-1.0), ny=float(1.0-mouseY/winH*2.0);
    for(int i=0;i<3;++i){
        const Rect& r=kButtons[i];
        if(nx>=r.x0&&nx<=r.x1&&ny>=r.y0&&ny<=r.y1){
            mode=static_cast<SkyMode>(i);
            std::cout<<"[GUI] "<<kBtnLabel[i]<<'\n'; return;
        }
    }
}

void GuiState::getClearColor(float& r,float& g,float& b) const {
    switch(mode){
        case SkyMode::Daytime:   r=0.40f;g=0.65f;b=0.90f; break;
        case SkyMode::Nighttime: r=0.01f;g=0.01f;b=0.04f; break;
        default:                 r=0.02f;g=0.04f;b=0.08f; break;
    }
}

void GuiState::update(GLFWwindow* window,int& p1,int& p2,int& p3){
    int k1=glfwGetKey(window,GLFW_KEY_1),k2=glfwGetKey(window,GLFW_KEY_2),k3=glfwGetKey(window,GLFW_KEY_3);
    if     (k1==GLFW_PRESS&&p1==GLFW_RELEASE){mode=SkyMode::Default;   std::cout<<"[GUI] Default\n";}
    else if(k2==GLFW_PRESS&&p2==GLFW_RELEASE){mode=SkyMode::Daytime;   std::cout<<"[GUI] Day\n";}
    else if(k3==GLFW_PRESS&&p3==GLFW_RELEASE){mode=SkyMode::Nighttime; std::cout<<"[GUI] Night\n";}
    p1=k1; p2=k2; p3=k3;
}


