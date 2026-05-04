#version 330 core

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform float uTime;
uniform vec3 uCamPos;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_uv;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vUv;

float detailDisplacement(vec2 xz, float time)
{
    float w1 = sin(xz.x * 3.8 + xz.y * 2.1 + time * 0.55) * 0.008;
    float w2 = sin(xz.x * 6.2 - xz.y * 4.7 + time * 0.38) * 0.004;
    return w1 + w2;
}

vec2 detailDisplacementGradient(vec2 xz, float time)
{
    float phase1 = xz.x * 3.8 + xz.y * 2.1 + time * 0.55;
    float phase2 = xz.x * 6.2 - xz.y * 4.7 + time * 0.38;

    float dHdX = cos(phase1) * 3.8 * 0.008 + cos(phase2) * 6.2 * 0.004;
    float dHdZ = cos(phase1) * 2.1 * 0.008 + cos(phase2) * -4.7 * 0.004;

    return vec2(dHdX, dHdZ);
}

float smoothBand(float startDist, float endDist, float dist)
{
    return smoothstep(startDist, endDist, dist);
}

float smoothBandDerivative(float startDist, float endDist, float dist)
{
    float span = endDist - startDist;
    float s = clamp((dist - startDist) / span, 0.0, 1.0);
    if (s <= 0.0 || s >= 1.0)
    {
        return 0.0;
    }
    return (6.0 * s * (1.0 - s)) / span;
}

void main()
{
    vec3 pos = in_position;
    pos.y += detailDisplacement(pos.xz, uTime);

    vec2 grad = detailDisplacementGradient(in_position.xz, uTime);

    vec3 preliminaryWorld = vec3(uModel * vec4(pos, 1.0));
    float aheadDist = max(0.0, uCamPos.z - preliminaryWorld.z);

    // Stretch the far half of the patch away from the camera so the water
    // reaches into the fog without adding extra tiled patches.
    float stretchT = smoothBand(4.5, 15.5, aheadDist);
    float zStretch = 4.8 * stretchT * stretchT;
    float stretchSlope = 4.8 * 2.0 * stretchT * smoothBandDerivative(4.5, 15.5, aheadDist);
    pos.z -= zStretch;

    // Very gentle overall downward tilt through the distance.
    float tiltT = smoothBand(3.5, 15.5, aheadDist);
    float tiltDrop = 0.11 * tiltT;
    float tiltSlope = 0.11 * smoothBandDerivative(3.5, 15.5, aheadDist);

    // Then a much later, softer curve only right near the end.
    float curveT = smoothBand(13.2, 19.0, aheadDist);
    float curveDrop = 0.20 * curveT * curveT;
    float curveSlope = 0.20 * 2.0 * curveT * smoothBandDerivative(13.2, 19.0, aheadDist);

    pos.y -= (tiltDrop + curveDrop);

    // Because we stretch the far geometry in z, reduce the apparent z-slope
    // a bit so the midground does not look folded over.
    float effectiveGradZ = (grad.y + tiltSlope + curveSlope) / (1.0 + stretchSlope);

    vec3 localNormal = normalize(vec3(
        in_normal.x - grad.x,
        in_normal.y,
        in_normal.z - effectiveGradZ
    ));

    vec3 worldPosition = vec3(uModel * vec4(pos, 1.0));
    vec3 worldNormal = normalize(mat3(uModel) * localNormal);

    vPosition = worldPosition;
    vNormal = worldNormal;
    vUv = in_uv;

    gl_Position = uViewProjection * vec4(worldPosition, 1.0);
}
