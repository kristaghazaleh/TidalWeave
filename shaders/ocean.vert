#version 330 core

uniform mat4 uModel;
uniform mat4 uViewProjection;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_uv;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vUv;

void main()
{
    vec3 worldPosition = vec3(uModel * vec4(in_position, 1.0));
    vec3 worldNormal = normalize(mat3(uModel) * in_normal);

    vPosition = worldPosition;
    vNormal = worldNormal;
    vUv = in_uv;

    gl_Position = uViewProjection * vec4(worldPosition, 1.0);
}
