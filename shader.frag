#version 330 core

in vec3 vNormal, vPosition, vTangent;
in vec4 ShadowCoord;

uniform vec3 lightPos;
uniform float lightIntensity;
uniform sampler2DShadow shadowMap;
uniform int shadingModel; // 0=Blinn-Phong, 1=Kajiya-Kay, 2=Marschner
uniform vec3 baseColor;

// ── Blinn-Phong uniforms ──
uniform float bp_ambient;
uniform float bp_diffuse;
uniform float bp_specular;
uniform float bp_shininess;
uniform float bp_wrap;

// ── Kajiya-Kay uniforms ──
uniform float kk_ambient;
uniform float kk_diffuse;
uniform float kk_specPrimary;
uniform float kk_specSecondary;
uniform float kk_shinyPrimary;
uniform float kk_shinySecondary;

// ── Marschner uniforms ──
uniform float m_ambient;
uniform float m_alphaR;      // cuticle tilt
uniform float m_betaR;       // roughness
uniform float m_R_strength;  // R lobe
uniform float m_TT_strength; // TT lobe
uniform float m_TRT_strength;// TRT lobe

out vec4 fragColor;

const float PI = 3.14159265;

float gaussian(float x, float sigma)
{
    return exp(-0.5 * x * x / (sigma * sigma));
}

// ── 1. Blinn-Phong with wrap diffuse ──

vec3 blinnPhong(vec3 N, vec3 L, vec3 V, vec3 bc, out vec3 ambient)
{
    vec3 H  = normalize(L + V);
    ambient = bp_ambient * bc;

    float NdotL = (dot(N, L) + bp_wrap) / (1.0 + bp_wrap);
    NdotL       = max(NdotL, 0.0);
    vec3  diff  = bp_diffuse * bc * NdotL;

    float NdotH = max(dot(N, H), 0.0);
    vec3  spec  = vec3(bp_specular) * pow(NdotH, bp_shininess) * float(dot(N, L) > 0.0);

    return ambient + (diff + spec) * lightIntensity;
}

// ── 2. Kajiya-Kay ──

vec3 kajiyaKay(vec3 T, vec3 N, vec3 L, vec3 V, vec3 bc, out vec3 ambient)
{
    ambient = kk_ambient * bc;

    float TdotL = dot(T, L);
    float TdotV = dot(T, V);
    float sinTL = sqrt(max(1.0 - TdotL * TdotL, 0.0));
    float sinTV = sqrt(max(1.0 - TdotV * TdotV, 0.0));

    vec3 diff = kk_diffuse * bc * sinTL;

    float cosHalf   = TdotL * TdotV + sinTL * sinTV;
    float primary   = pow(max(cosHalf, 0.0), kk_shinyPrimary);
    float secondary = pow(max(cosHalf, 0.0), kk_shinySecondary);

    vec3 spec = vec3(kk_specPrimary) * primary + kk_specSecondary * bc * secondary;

    return ambient + (diff + spec) * lightIntensity;
}

// ── 3. Marschner (simplified three-lobe) ──

vec3 marschner(vec3 T, vec3 N, vec3 L, vec3 V, vec3 bc, out vec3 ambient)
{
    ambient = m_ambient * bc;

    float sinThetaI = dot(T, L);
    float sinThetaO = dot(T, V);
    float cosThetaI = sqrt(max(1.0 - sinThetaI * sinThetaI, 0.0));

    float thetaI = asin(clamp(sinThetaI, -1.0, 1.0));
    float thetaO = asin(clamp(sinThetaO, -1.0, 1.0));
    float thetaH = (thetaI + thetaO) * 0.5;

    vec3 Lperp = L - sinThetaI * T;
    vec3 Vperp = V - sinThetaO * T;
    float lenLp = length(Lperp);
    float lenVp = length(Vperp);
    float cosPhi = (lenLp > 0.001 && lenVp > 0.001)
                   ? clamp(dot(Lperp / lenLp, Vperp / lenVp), -1.0, 1.0)
                   : 1.0;
    float phi = acos(cosPhi);

    float alphaTT  =  m_alphaR * 0.5;
    float alphaTRT = -m_alphaR * 1.5;
    float betaTT   = m_betaR * 0.5;
    float betaTRT  = m_betaR * 2.0;

    // R lobe
    float MR = gaussian(thetaH - m_alphaR, m_betaR);
    float NR = pow(max(cos(phi * 0.5), 0.0), 16.0);
    vec3  R  = vec3(m_R_strength) * MR * NR;

    // TT lobe
    float MTT       = gaussian(thetaH - alphaTT, betaTT);
    float fresnelTT = pow(1.0 - abs(cosPhi), 4.0);
    float NTT       = (1.0 - fresnelTT) * 0.6;
    vec3  absorb    = pow(bc, vec3(1.2));
    vec3  TT        = m_TT_strength * absorb * MTT * NTT;

    // TRT lobe
    float MTRT = gaussian(thetaH - alphaTRT, betaTRT);
    float NTRT = pow(max(cos(phi * 0.5 - 0.3), 0.0), 6.0);
    vec3  TRT  = m_TRT_strength * bc * MTRT * NTRT;

    float cosWeight = max(cosThetaI, 0.0);
    return ambient + (R + TT + TRT) * cosWeight * lightIntensity;
}

// ── main ──

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vPosition);
    vec3 L = normalize(lightPos - vPosition);
    vec3 T = normalize(vTangent);

    if (dot(N, V) < 0.0) N = -N;

    vec3 ambient;
    vec3 color;

    if      (shadingModel == 1) color = kajiyaKay(T, N, L, V, baseColor, ambient);
    else if (shadingModel == 2) color = marschner(T, N, L, V, baseColor, ambient);
    else                        color = blinnPhong(N, L, V, baseColor, ambient);

    vec3  projCoords = ShadowCoord.xyz / ShadowCoord.w;
    float visibility = 1.0;
    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
        projCoords.y >= 0.0 && projCoords.y <= 1.0)
    {
        float bias = max(0.008 * (1.0 - dot(N, L)), 0.001);
        projCoords.z -= bias;
        visibility = texture(shadowMap, projCoords);
    }
    else { visibility = 0.0; }

    visibility = max(visibility, 0.3);

    vec3 direct = color - ambient;
    color = ambient + direct * visibility;

    fragColor = vec4(color, 1.0);
}
