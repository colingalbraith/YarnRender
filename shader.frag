#version 330 core

in vec3 vNormal, vPosition, vTangent, vFiberColor;
in vec4 ShadowCoord;
in float vDomDepth;
in vec4  vDomClip;

uniform vec3 lightPos;
uniform float lightIntensity;
uniform sampler2DShadow shadowMap;
uniform int shadingModel; // 0=Blinn-Phong, 1=Kajiya-Kay, 2=Marschner
uniform vec3 baseColor;
uniform float colorVariation;
uniform float exposure;
uniform int   gammaEnabled;

// ── Deep Opacity Maps ──
uniform int       domEnabled;
uniform int       domDebug;      // 0=off, 1=show depth, 2=show opacity, 3=show visibility
uniform sampler2D domDepthMap;   // unit 1: linear z0
uniform sampler2D domOpacityMap; // unit 2: accumulated opacity per layer
uniform vec3      domLayers;     // d1, d2, d3

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
uniform float kk_normalInfluence;

// ── Marschner uniforms ──
uniform float m_ambient;
uniform float m_alphaR;      // cuticle tilt
uniform float m_betaR;       // roughness
uniform float m_R_strength;  // R lobe
uniform float m_TT_strength; // TT lobe
uniform float m_TRT_strength;// TRT lobe
uniform float m_normalInfluence;

out vec4 fragColor;

const float PI = 3.14159265;

float gaussian(float x, float sigma)
{
    return exp(-0.5 * x * x / (sigma * sigma));
}

vec3 acesToneMap(vec3 x)
{
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
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

    // Normal modulation: use surface normal to differentiate fibers
    float normalShade = mix(1.0, max(dot(N, L), 0.0), kk_normalInfluence);

    return ambient + (diff + spec) * normalShade * lightIntensity;
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

    // Normal modulation: use surface normal to differentiate fibers
    float normalShade = mix(1.0, max(dot(N, L), 0.0), m_normalInfluence);

    float cosWeight = max(cosThetaI, 0.0);
    return ambient + (R + TT + TRT) * cosWeight * normalShade * lightIntensity;
}

// ── main ──

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vPosition);
    vec3 L = normalize(lightPos - vPosition);
    vec3 T = normalize(vTangent);

    if (dot(N, V) < 0.0) N = -N;

    // Per-fiber color variation
    vec3 bc = baseColor * mix(vec3(1.0), vFiberColor, colorVariation);

    vec3 ambient;
    vec3 color;

    if      (shadingModel == 1) color = kajiyaKay(T, N, L, V, bc, ambient);
    else if (shadingModel == 2) color = marschner(T, N, L, V, bc, ambient);
    else                        color = blinnPhong(N, L, V, bc, ambient);

    vec3  projCoords = ShadowCoord.xyz / ShadowCoord.w;
    float visibility = 1.0;

    if (domEnabled > 0) {
        // ── Deep Opacity Maps path ──
        // Convert light clip-space to [0,1] UV for DOM texture lookup
        vec3 domNDC = vDomClip.xyz / vDomClip.w;
        vec2 domUV  = domNDC.xy * 0.5 + 0.5;

        if (domUV.x >= 0.0 && domUV.x <= 1.0 &&
            domUV.y >= 0.0 && domUV.y <= 1.0)
        {
            float z0    = texture(domDepthMap, domUV).r;
            float delta = vDomDepth - z0;

            if (delta < 0.0) {
                visibility = 1.0; // in front of hair volume
            } else {
                vec3 layers = texture(domOpacityMap, domUV).rgb;
                float opacity;
                if (delta <= domLayers.x) {
                    opacity = mix(0.0, layers.r, delta / max(domLayers.x, 0.0001));
                } else if (delta <= domLayers.y) {
                    float t = (delta - domLayers.x) / max(domLayers.y - domLayers.x, 0.0001);
                    opacity = mix(layers.r, layers.g, t);
                } else if (delta <= domLayers.z) {
                    float t = (delta - domLayers.y) / max(domLayers.z - domLayers.y, 0.0001);
                    opacity = mix(layers.g, layers.b, t);
                } else {
                    opacity = layers.b;
                }
                visibility = exp(-opacity);
            }
        }
        visibility = max(visibility, 0.05);
    } else {
        // ── Classic shadow map path ──
        if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
            projCoords.y >= 0.0 && projCoords.y <= 1.0)
        {
            float bias = max(0.008 * (1.0 - dot(N, L)), 0.001);
            projCoords.z -= bias;
            visibility = texture(shadowMap, projCoords);
        }
        else { visibility = 0.0; }
        visibility = max(visibility, 0.3);
    }

    vec3 direct = color - ambient;
    color = ambient + direct * visibility;

    // DOM debug visualization
    if (domEnabled > 0 && domDebug > 0) {
        vec3 domNDC = vDomClip.xyz / vDomClip.w;
        vec2 domUV  = domNDC.xy * 0.5 + 0.5;
        if (domDebug == 1) {
            // Show DOM depth texture (normalized)
            float z = texture(domDepthMap, domUV).r;
            float viz = z / 100.0; // scale to visible range
            fragColor = vec4(viz, viz, viz, 1.0);
            return;
        } else if (domDebug == 2) {
            // Show DOM opacity texture (RGB = layer opacities)
            vec3 op = texture(domOpacityMap, domUV).rgb;
            fragColor = vec4(op, 1.0);
            return;
        } else if (domDebug == 3) {
            // Show visibility as grayscale
            fragColor = vec4(vec3(visibility), 1.0);
            return;
        }
    }

    // Tone mapping and gamma
    color *= exposure;
    if (gammaEnabled > 0) {
        color = acesToneMap(color);
        color = pow(color, vec3(1.0/2.2));
    }

    fragColor = vec4(color, 1.0);
}
