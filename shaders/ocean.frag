#version 330 core

uniform vec3 uCamPos;
uniform float uTime;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vUv;

out vec4 out_color;

vec3 proceduralSky(vec3 dir)
{
    float t = clamp(0.5 * (dir.y + 1.0), 0.0, 1.0);
    vec3 horizon = vec3(0.60, 0.73, 0.88);
    vec3 zenith  = vec3(0.06, 0.16, 0.34);
    return mix(horizon, zenith, pow(t, 0.65));
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    vec3 waterNormal = normalize(vNormal);
    vec3 viewDir = normalize(uCamPos - vPosition);

    vec3 lightDir = normalize(vec3(0.25, 1.0, 0.35));
    vec3 halfVec = normalize(lightDir + viewDir);

    float NdotL = max(dot(waterNormal, lightDir), 0.0);
    float NdotH = max(dot(waterNormal, halfVec), 0.0);
    float NdotV = max(dot(waterNormal, viewDir), 0.0);

    float fresnel = fresnelSchlick(NdotV, 0.02);
    vec3 reflectedSky = proceduralSky(reflect(-viewDir, waterNormal));

    float tint = 0.5 + 0.5 * sin(8.0 * vUv.x + 5.0 * vUv.y + 0.25 * uTime);
    vec3 deepWater = vec3(0.02, 0.10, 0.22);
    vec3 shallowWater = vec3(0.10, 0.36, 0.62);
    vec3 waterBody = mix(deepWater, shallowWater, tint);

    vec3 ambient = 0.14 * waterBody;
    vec3 diffuse = 0.60 * waterBody * NdotL;
    vec3 specular = 0.95 * vec3(1.0) * pow(NdotH, 120.0);

    vec3 color =
        ambient +
        (1.0 - fresnel) * diffuse +
        fresnel * reflectedSky +
        specular;

    out_color = vec4(color, 1.0);
}