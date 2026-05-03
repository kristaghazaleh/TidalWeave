#version 330 core

layout(location = 0) in vec3 in_position;

uniform mat4 uModel;
uniform mat4 uViewProjection;

out vec3 vWorldPos;
out vec3 vLocalPos;

void main()
{
    vec4 world = uModel * vec4(in_position, 1.0);
    vWorldPos = world.xyz;
    vLocalPos = in_position;
    gl_Position = uViewProjection * world;
}
