#version 330 core

in vec3 vWorldPos;
in vec3 vLocalPos;

uniform vec3 uCamPos;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uNearDistance;
uniform float uFarDistance;

out vec4 out_color;

void main()
{
    float localDepth = clamp(-vLocalPos.z, 0.0, 1.0);       // 0 front, 1 back
    float localHeight = clamp(vLocalPos.y, 0.0, 1.0);       // 0 bottom, 1 top
    float lateral = clamp(abs(vLocalPos.x) * 2.0, 0.0, 1.0); // 0 center, 1 edge

    float distanceToCamera = length(vWorldPos - uCamPos);
    float distanceFade = smoothstep(uNearDistance, uFarDistance, distanceToCamera);
    float frontFade = smoothstep(0.08, 0.45, localDepth);
    float topFade = 1.0 - smoothstep(0.08, 1.0, localHeight);
    float edgeFade = 1.0 - smoothstep(0.78, 1.0, lateral);
    float backBoost = mix(0.35, 1.0, localDepth);

    float density = uFogDensity * distanceFade * frontFade * topFade * edgeFade * backBoost;
    float alpha = 1.0 - exp(-density);
    alpha = clamp(alpha, 0.0, 0.82);

    out_color = vec4(uFogColor, alpha);
}
