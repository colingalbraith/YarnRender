#version 330 core

in vec3 vNormal, vPosition, vTangent, vFiberColor;
in float vFiberType;
in float vTubeU, vTubeV;
in vec4 ShadowCoord;
in float vDomDepth;
in vec4  vDomClip;

uniform vec3 lightPos;
uniform float lightIntensity;
uniform sampler2DShadow shadowMap;
uniform int shadingModel; // 0=Blinn-Phong, 1=Kajiya-Kay, 2=Marschner, 3=Yarn
uniform vec3 baseColor;
uniform float colorVariation;
uniform float exposure;
uniform int   gammaEnabled;

// ── Deep Opacity Maps ──
uniform int       domEnabled;
uniform int       domDebug;
uniform sampler2D domDepthMap;
uniform sampler2D domOpacityMap;
uniform vec3      domLayers;

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
uniform float m_alphaR;
uniform float m_betaR;
uniform float m_R_strength;
uniform float m_TT_strength;
uniform float m_TRT_strength;
uniform float m_normalInfluence;

// ── Yarn shading uniforms ──
uniform float y_ambient;
uniform float y_diffuse;
uniform float y_specular;
uniform float y_fuzz;
uniform float y_wrap;
uniform float y_tangentBlend;
uniform float y_shininess;
uniform float y_fuzzWidth;
uniform float y_specShift1;
uniform float y_specShift2;
uniform float y_spec2Tint;
uniform float y_shininess2;
uniform float y_fresnel;
uniform float y_transmission;
uniform float y_transPower;
uniform float y_tangentNoise;

// ── Fiber surface effects ──
uniform int   fiberStripes;
uniform float fiberTwistRate;
uniform float fiberGrooveDepth;
uniform float noiseStrength;
uniform float noiseScale;
uniform float rimStrength;
uniform float rimPower;
uniform float sssStrength;
uniform float sssPower;
uniform float plyAlpha;
uniform float flyawayAlpha;

out vec4 fragColor;

const float PI = 3.14159265;

// ── Utility functions ──

float gaussian(float x, float sigma)
{
    return exp(-0.5 * x * x / (sigma * sigma));
}

vec3 acesToneMap(vec3 x)
{
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// Cheap hash noise (no sin, no gradient — just value noise via integer hashing)
float hashNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n = i.x + i.y * 157.0 + i.z * 113.0;
    float a = fract(n * 0.1031);
    float b = fract((n + 1.0) * 0.1031);
    float c = fract((n + 157.0) * 0.1031);
    float d = fract((n + 158.0) * 0.1031);
    float e = fract((n + 113.0) * 0.1031);
    float g = fract((n + 114.0) * 0.1031);
    float h = fract((n + 270.0) * 0.1031);
    float k = fract((n + 271.0) * 0.1031);
    return mix(mix(mix(a, b, f.x), mix(c, d, f.x), f.y),
               mix(mix(e, g, f.x), mix(h, k, f.x), f.y), f.z) * 2.0 - 1.0;
}

// Keep old name for compatibility with yarn shader include
float valueNoise(vec3 p) { return hashNoise(p); }

float fiberFBM(vec3 pos, vec3 T)
{
    vec3 p = pos - T * dot(pos, T) * 0.7;
    return 0.55 * hashNoise(p) + 0.3 * hashNoise(p * 2.2);
}

// ── Shading models ──
#include "shading_blinnphong.glsl"
#include "shading_kajiyakay.glsl"
#include "shading_marschner.glsl"
#include "shading_yarn.glsl"

// ── main ──

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vPosition);
    vec3 L = normalize(lightPos - vPosition);
    vec3 T = normalize(vTangent);

    if (dot(N, V) < 0.0) N = -N;

    // ── Twisted fiber stripe pattern with per-fiber variation ──
    float stripeShade = 1.0;
    vec3 stripeTint = vec3(1.0);
    if (fiberStripes > 0) {
        float twistedV = vTubeV * float(fiberStripes) + vTubeU * fiberTwistRate;

        float stripeIdx = floor(twistedV);
        float withinStripe = fract(twistedV);

        float sn = stripeIdx + 0.5;
        float h1 = fract(sn * 0.1031);
        float h2 = fract(sn * 0.1030);
        float h3 = fract(sn * 0.0973);
        float h4 = fract(sn * 0.0742);
        float h5 = fract(sn * 0.0567);

        float wobbleFreq = 3.0 + h4 * 5.0;
        float posWobble = (h1 - 0.5) * 0.15 + 0.06 * sin(vTubeU * wobbleFreq * 2.0 * PI + h2 * 6.28);
        float widthWobble = 1.0 + 0.15 * sin(vTubeU * (wobbleFreq * 0.7) * 2.0 * PI + h3 * 6.28);
        float adjusted = withinStripe + posWobble;

        float edgeNoise = 0.04 * sin(vTubeU * 80.0 + stripeIdx * 13.7)
                        + 0.03 * sin(vTubeU * 190.0 + stripeIdx * 31.1);
        adjusted += edgeNoise;

        float width = (0.85 + h2 * 0.3) * widthWobble;
        float stripe = sin((adjusted - 0.5) * PI * width);
        stripeShade = 1.0 - fiberGrooveDepth * (0.5 - 0.5 * stripe);

        float shimmer = 1.0 + 0.04 * sin(vTubeU * 25.0 + h5 * 6.28);
        float brightness = (0.88 + h3 * 0.24) * shimmer;
        stripeTint = vec3(
            brightness * (0.95 + 0.1 * fract(h1 * 7.3)),
            brightness * (0.95 + 0.1 * fract(h2 * 5.7)),
            brightness * (0.95 + 0.1 * fract(h3 * 3.1))
        );

        float tiltAngle = (h4 - 0.5) * 0.15 * fiberGrooveDepth;
        vec3 B = cross(T, N);
        N = normalize(N + B * tiltAngle);
    }

    // Procedural micro-noise on top of stripes
    float fiberNoise = 0.0;
    if (noiseStrength > 0.0) {
        vec3 noiseVal = vec3(valueNoise(vPosition * noiseScale),
                             valueNoise(vPosition * noiseScale + vec3(31.7)),
                             valueNoise(vPosition * noiseScale + vec3(67.3)));
        N = normalize(N + noiseStrength * 2.0 * noiseVal);
        fiberNoise = fiberFBM(vPosition * noiseScale * 0.5, T);
    }

    // Per-fiber color variation + stripe shading + micro-noise
    vec3 bc = baseColor * mix(vec3(1.0), vFiberColor, colorVariation);
    bc *= stripeShade * stripeTint;
    bc *= 1.0 + noiseStrength * 3.0 * fiberNoise;

    vec3 ambient;
    vec3 color;

    if      (shadingModel == 1) color = kajiyaKay(T, N, L, V, bc, ambient);
    else if (shadingModel == 2) color = marschner(T, N, L, V, bc, ambient);
    else if (shadingModel == 3) color = yarnShader(T, N, L, V, bc, ambient);
    else                        color = blinnPhong(N, L, V, bc, ambient);

    // Rim / Fresnel edge glow for fuzzy appearance
    if (rimStrength > 0.0) {
        float rim = pow(1.0 - max(dot(N, V), 0.0), rimPower);
        color += rimStrength * rim * bc * lightIntensity * 0.5;
    }

    // Subsurface scattering — light passing through thin fibers
    if (sssStrength > 0.0) {
        float scatter = pow(max(dot(-L, V), 0.0), sssPower);
        color += sssStrength * scatter * bc * lightIntensity * 0.4;
    }

    vec3  projCoords = ShadowCoord.xyz / ShadowCoord.w;
    float visibility = 1.0;

    if (domEnabled > 0) {
        vec3 domNDC = vDomClip.xyz / vDomClip.w;
        vec2 domUV  = domNDC.xy * 0.5 + 0.5;

        if (domUV.x >= 0.0 && domUV.x <= 1.0 &&
            domUV.y >= 0.0 && domUV.y <= 1.0)
        {
            float z0    = texture(domDepthMap, domUV).r;
            float delta = vDomDepth - z0;

            if (delta < 0.0) {
                visibility = 1.0;
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
        if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
            projCoords.y >= 0.0 && projCoords.y <= 1.0)
        {
            float bias = max(0.008 * (1.0 - dot(N, L)), 0.001);
            projCoords.z -= bias;
            // 4-tap PCF soft shadows
            vec2 ts = 0.5 / vec2(textureSize(shadowMap, 0));
            visibility = (texture(shadowMap, projCoords + vec3(-ts.x, -ts.y, 0))
                        + texture(shadowMap, projCoords + vec3( ts.x, -ts.y, 0))
                        + texture(shadowMap, projCoords + vec3(-ts.x,  ts.y, 0))
                        + texture(shadowMap, projCoords + vec3( ts.x,  ts.y, 0))) * 0.25;
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
            float z = texture(domDepthMap, domUV).r;
            float viz = z / 100.0;
            fragColor = vec4(viz, viz, viz, 1.0);
            return;
        } else if (domDebug == 2) {
            vec3 op = texture(domOpacityMap, domUV).rgb;
            fragColor = vec4(op, 1.0);
            return;
        } else if (domDebug == 3) {
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

    // Alpha: separate ply vs flyaway opacity + Fresnel edge fade
    float baseAlpha = mix(plyAlpha, flyawayAlpha, vFiberType);
    float edgeFade = pow(1.0 - max(dot(normalize(vNormal), V), 0.0), 2.0);
    float alpha = mix(baseAlpha, baseAlpha * 0.3, edgeFade * (1.0 - baseAlpha));

    fragColor = vec4(color, alpha);
}
