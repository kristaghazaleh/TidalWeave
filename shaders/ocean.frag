#version 330 core

uniform vec3 uCamPos;
uniform float uTime;
uniform sampler2D uEnvironmentMap;
uniform int uEnvironmentMode;
uniform float uEnvRotation;
uniform float uEnvPitch;
uniform float uEnvExposure;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vUv;

out vec4 out_color;

const float INV_PI = 0.31830988618;
const float INV_TWO_PI = 0.15915494309;

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

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
        vec3 horizon = vec3(0.92, 0.63, 0.58);
        vec3 zenith = vec3(0.25, 0.15, 0.26);
        return mix(horizon, zenith, pow(t, 0.75));
    }
    else
    {
        vec3 horizon = vec3(0.08, 0.10, 0.16);
        vec3 zenith = vec3(0.01, 0.03, 0.08);
        return mix(horizon, zenith, pow(t, 0.85));
    }
}

vec3 sampleEnvironmentSoft(vec3 dir, float roughness)
{
    dir = normalize(dir);
    dir.y = max(dir.y, 0.0);

    vec2 uv = directionToEquirectUV(dir);
    vec2 texel = 1.0 / vec2(textureSize(uEnvironmentMap, 0));
    float blurRadius = mix(2.0, 6.0, clamp(roughness, 0.0, 1.0));

    vec2 du = vec2(texel.x * blurRadius, 0.0);
    vec2 dv = vec2(0.0, texel.y * blurRadius);

    vec3 hdr = vec3(0.0);
    hdr += 4.0 * texture(uEnvironmentMap, uv).rgb;
    hdr += 2.0 * texture(uEnvironmentMap, uv + du).rgb;
    hdr += 2.0 * texture(uEnvironmentMap, uv - du).rgb;
    hdr += 2.0 * texture(uEnvironmentMap, uv + dv).rgb;
    hdr += 2.0 * texture(uEnvironmentMap, uv - dv).rgb;
    hdr += 1.0 * texture(uEnvironmentMap, uv + du + dv).rgb;
    hdr += 1.0 * texture(uEnvironmentMap, uv + du - dv).rgb;
    hdr += 1.0 * texture(uEnvironmentMap, uv - du + dv).rgb;
    hdr += 1.0 * texture(uEnvironmentMap, uv - du - dv).rgb;
    hdr /= 16.0;

    vec3 env = toneMap(hdr);
    vec3 sky = proceduralSky(dir);
    float envMix = (uEnvironmentMode == 0) ? 0.92 : 0.985;
    return mix(sky, env, envMix);
}

vec3 applyDetailNormals(vec3 baseNormal, vec2 worldXZ, float time)
{
    const float TWO_PI = 6.28318530718;

    vec2 uv1 = worldXZ * 0.75 + vec2(0.03, 0.02) * time;
    vec3 d1 = normalize(vec3(
        cos(uv1.x * TWO_PI) * sin(uv1.y * TWO_PI),
        5.0,
        sin(uv1.x * TWO_PI) * cos(uv1.y * TWO_PI)
    ));

    vec2 uv2 = worldXZ * 1.90 + vec2(-0.02, 0.035) * time;
    vec3 d2 = normalize(vec3(
        cos(uv2.x * TWO_PI) * sin(uv2.y * TWO_PI) * 0.6,
        7.0,
        sin(uv2.x * TWO_PI) * cos(uv2.y * TWO_PI) * 0.6
    ));

    return normalize(baseNormal + 0.10 * d1 + 0.05 * d2);
}

void main()
{
    vec3 waterNormal = normalize(vNormal);
    if (!gl_FrontFacing)
    {
        waterNormal = -waterNormal;
    }

    waterNormal = applyDetailNormals(waterNormal, vPosition.xz, uTime);

    vec3 viewDir = normalize(uCamPos - vPosition);
    vec3 reflectedDir = reflect(-viewDir, waterNormal);

    vec3 lightDir;
    vec3 deepWater;
    vec3 shallowWater;
    float ambientStrength;
    float diffuseStrength;
    float specularStrength;
    float envStrength;
    float microStrength;

    if (uEnvironmentMode == 0)
    {
        lightDir = normalize(vec3(-0.30, 0.88, -0.12));
        deepWater = vec3(0.03, 0.08, 0.16);
        shallowWater = vec3(0.16, 0.27, 0.40);
        ambientStrength = 0.12;
        diffuseStrength = 0.45;
        specularStrength = 0.80;
        envStrength = 0.92;
        microStrength = 0.26;
    }
    else
    {
        lightDir = normalize(vec3(-0.08, 0.97, 0.24));
        deepWater = vec3(0.01, 0.04, 0.08);
        shallowWater = vec3(0.04, 0.10, 0.17);
        ambientStrength = 0.05;
        diffuseStrength = 0.18;
        specularStrength = 0.92;
        envStrength = 0.72;
        microStrength = 0.14;
    }

    vec3 halfVec = normalize(lightDir + viewDir);

    float NdotL = max(dot(waterNormal, lightDir), 0.0);
    float NdotH = max(dot(waterNormal, halfVec), 0.0);
    float NdotV = max(dot(waterNormal, viewDir), 0.0);

    float fresnel = fresnelSchlick(NdotV, 0.02);
    float roughness = clamp(1.0 - NdotV, 0.0, 1.0);
    vec3 envColor = sampleEnvironmentSoft(reflectedDir, roughness);

    float band = 0.5 + 0.5 * sin(0.55 * vPosition.x + 0.35 * vPosition.z + 0.06 * uTime);
    vec3 waterBody = mix(deepWater, shallowWater, band);

    vec3 ambient = ambientStrength * waterBody;
    vec3 diffuse = diffuseStrength * waterBody * NdotL;
    vec3 specular = specularStrength * vec3(1.0) * pow(NdotH, 140.0);
    vec3 microSpecular = microStrength * vec3(1.0) * pow(NdotH, 480.0);

    vec3 color = ambient + (1.0 - fresnel) * diffuse + fresnel * envStrength * envColor + specular + microSpecular;
    out_color = vec4(color, 1.0);
}
