#version 330 core

in vec3 vWorldPos;
in vec3 vLocalPos;

uniform vec3 uCamPos;
uniform vec3 uFogColor;
uniform int uEnvironmentMode;
uniform float uFogDensity;
uniform float uNearDistance;
uniform float uFarDistance;
uniform float uTime;
uniform float uNoiseScale;
uniform float uAnimationSpeed;
uniform vec2 uDriftDir;
uniform float uAlphaCap;
uniform int uLayerIndex;

out vec4 out_color;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise2D(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        value += amplitude * valueNoise2D(p);
        p = p * 2.02 + vec2(19.7, 7.3);
        amplitude *= 0.5;
    }
    return value;
}

void main()
{
    float localDepth = clamp(-vLocalPos.z, 0.0, 1.0);
    float localHeight = clamp(vLocalPos.y, 0.0, 1.0);
    float localX = clamp(abs(vLocalPos.x) * 2.0, 0.0, 1.0);

    float distanceToCamera = length(vWorldPos - uCamPos);
    float distanceFade = smoothstep(uNearDistance, uFarDistance, distanceToCamera);
    float frontFade = mix(0.42, 1.0, smoothstep(0.02, 0.90, localDepth));
    float sideFade = 1.0 - smoothstep(0.58, 0.98, localX);

    vec2 driftDir = normalize(uDriftDir);
    vec2 drift = driftDir * (uTime * (0.18 + 0.80 * uAnimationSpeed));
    vec2 p = vWorldPos.xz * uNoiseScale + drift;

    vec2 warp = vec2(
        fbm(p * 0.40 + vec2(3.2, -5.4)),
        fbm(p * 0.40 + vec2(-4.7, 2.8))
    );
    p += (warp - 0.5) * 2.6;

    float broadShape = fbm(p * 0.52 + vec2(1.7, 3.4));
    float mediumShape = fbm(p * 1.05 + vec2(-8.1, 6.5));
    float fineShape = fbm(p * 2.10 + vec2(4.2, -11.6));
    float shape = 0.52 * broadShape + 0.33 * mediumShape + 0.15 * fineShape;

    float rolling = 0.5 + 0.5 * sin(
        vWorldPos.x * (0.05 + 0.018 * float(uLayerIndex)) +
        vWorldPos.z * (0.08 + 0.020 * float(uLayerIndex)) +
        uTime * (0.50 + 0.18 * float(uLayerIndex))
    );

    float topCeiling = mix(0.12, 0.62, shape);
    topCeiling += (rolling - 0.5) * 0.10;
    float topFeather = 1.0 - smoothstep(topCeiling - 0.14, topCeiling + 0.10, localHeight);

    float lowMist = exp(-5.5 * localHeight);
    float waterlineFeather = 1.0 - smoothstep(0.00, 0.20, localHeight);
    float verticalShape = max(lowMist * topFeather, waterlineFeather * 0.28 * topFeather);

    float patchiness = smoothstep(0.36, 0.78, shape);
    float foregroundLift = (uLayerIndex == 0) ? mix(1.28, 1.0, smoothstep(0.0, 0.55, localDepth)) : 1.0;
    float density = uFogDensity * distanceFade * frontFade * sideFade * verticalShape * patchiness * mix(0.86, 1.14, rolling) * foregroundLift;
    if (uEnvironmentMode == 1)
    {
        density *= 0.58;
    }

    float alpha = 1.0 - exp(-density);
    alpha = clamp(alpha, 0.0, uAlphaCap);

    vec3 lowColor;
    vec3 highColor;
    vec3 horizonColor;
    vec3 ambientSky;
    vec3 waterBounce;
    vec3 lightColor;
    vec3 lightDir;
    float phaseExponent;

    if (uEnvironmentMode == 0)
    {
        lowColor = mix(uFogColor, vec3(0.72, 0.61, 0.58), 0.30);
        highColor = mix(uFogColor, vec3(0.96, 0.86, 0.80), 0.45);
        horizonColor = vec3(0.98, 0.82, 0.66);
        ambientSky = vec3(0.28, 0.22, 0.20);
        waterBounce = vec3(0.06, 0.05, 0.06);
        lightColor = vec3(0.55, 0.42, 0.26);
        lightDir = normalize(vec3(-0.30, 0.90, -0.16));
        phaseExponent = 8.0;
    }
    else
    {
        lowColor = mix(uFogColor, vec3(0.04, 0.07, 0.14), 0.42);
        highColor = mix(uFogColor, vec3(0.12, 0.20, 0.34), 0.36);
        horizonColor = vec3(0.20, 0.30, 0.48);
        ambientSky = vec3(0.06, 0.10, 0.18);
        waterBounce = vec3(0.025, 0.045, 0.080);
        lightColor = vec3(0.42, 0.58, 0.96);
        lightDir = normalize(vec3(-0.10, 0.98, 0.20));
        phaseExponent = 10.0;
    }

    float heightLerp = smoothstep(0.04, 0.58, localHeight);
    float depthLerp = smoothstep(0.18, 1.0, localDepth);

    vec3 fogColor = mix(lowColor, highColor, heightLerp);
    fogColor = mix(fogColor, horizonColor, depthLerp * 0.65);

    vec3 viewDir = normalize(vWorldPos - uCamPos);
    float forwardScatter = pow(max(dot(-viewDir, lightDir), 0.0), phaseExponent);
    float phase = mix(0.18, 1.0, forwardScatter);
    vec3 inScatter = lightColor * phase * mix(0.18, 0.62, depthLerp) * mix(0.40, 1.05, patchiness);

    vec3 bounce = ambientSky * mix(0.35, 1.00, depthLerp) + waterBounce * (1.0 - heightLerp);

    fogColor += bounce + inScatter * alpha;

    if (uEnvironmentMode == 1)
    {
        fogColor *= vec3(0.86, 0.96, 1.22);
    }

    out_color = vec4(fogColor, alpha);
}
