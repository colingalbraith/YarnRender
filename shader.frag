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

// ── Yarn shading uniforms ──
uniform float y_ambient;
uniform float y_diffuse;
uniform float y_specular;
uniform float y_fuzz;
uniform float y_wrap;
uniform float y_tangentBlend;
uniform float y_shininess;
uniform float y_fuzzWidth;

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

float gaussian(float x, float sigma)
{
    return exp(-0.5 * x * x / (sigma * sigma));
}

vec3 acesToneMap(vec3 x)
{
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// Hash-based 3D noise (no texture needed)
vec3 hash33(vec3 p)
{
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return fract(sin(p) * 43758.5453) * 2.0 - 1.0;
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(dot(hash33(i + vec3(0,0,0)), f - vec3(0,0,0)),
                       dot(hash33(i + vec3(1,0,0)), f - vec3(1,0,0)), f.x),
                   mix(dot(hash33(i + vec3(0,1,0)), f - vec3(0,1,0)),
                       dot(hash33(i + vec3(1,1,0)), f - vec3(1,1,0)), f.x), f.y),
               mix(mix(dot(hash33(i + vec3(0,0,1)), f - vec3(0,0,1)),
                       dot(hash33(i + vec3(1,0,1)), f - vec3(1,0,1)), f.x),
                   mix(dot(hash33(i + vec3(0,1,1)), f - vec3(0,1,1)),
                       dot(hash33(i + vec3(1,1,1)), f - vec3(1,1,1)), f.x), f.y), f.z);
}

// 2-octave FBM with anisotropic stretching along tangent
float fiberFBM(vec3 pos, vec3 T)
{
    vec3 p = pos - T * dot(pos, T) * 0.7;
    return 0.55 * valueNoise(p) + 0.3 * valueNoise(p * 2.2);
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

// ── 4. Yarn shader (hybrid tangent-normal model for twisted plied yarn) ──
//
// Designed for yarn rather than hair: combines tangent-based anisotropy
// with normal-based cross-section shading in a single unified model.
// Key differences from hair models:
//   - Wrap diffuse blends tangent and normal contributions (yarn is fuzzier)
//   - "Fuzz" lobe: broad scattered highlight from inter-fiber bounces
//   - Cross-section gradient: smooth darkening toward yarn interior
//   - All parameters tuned for the matte, soft look of twisted fiber bundles

vec3 yarnShader(vec3 T, vec3 N, vec3 L, vec3 V, vec3 bc, out vec3 ambient)
{
    ambient = y_ambient * bc;

    // ── Hybrid diffuse: blend tangent-based and normal-based ──
    // Tangent diffuse (Kajiya-Kay style)
    float TdotL = dot(T, L);
    float sinTL = sqrt(max(1.0 - TdotL * TdotL, 0.0));
    // Normal diffuse with wrap
    float NdotL = (dot(N, L) + y_wrap) / (1.0 + y_wrap);
    NdotL = max(NdotL, 0.0);
    // Blend: high tangentBlend = more anisotropic, low = more isotropic
    float diff = mix(NdotL, sinTL, y_tangentBlend);
    vec3 diffuse = y_diffuse * bc * diff;

    // ── Primary specular: anisotropic along fiber ──
    float TdotV = dot(T, V);
    float sinTV = sqrt(max(1.0 - TdotV * TdotV, 0.0));
    float cosHalf = TdotL * TdotV + sinTL * sinTV;
    vec3 spec = vec3(y_specular) * pow(max(cosHalf, 0.0), y_shininess);

    // ── Fuzz lobe: broad scattered highlight from inter-fiber bouncing ──
    // Uses the normal-space half vector for a wide, soft highlight
    // that wraps around the yarn cross-section
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 fuzz = y_fuzz * bc * pow(NdotH, y_fuzzWidth);

    // ── Cross-section shading: darken fibers facing away ──
    float crossSection = mix(0.6, 1.0, max(dot(N, L), 0.0));

    return ambient + (diffuse + spec + fuzz) * crossSection * lightIntensity;
}

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

        // Identify which stripe/fiber we're in
        float stripeIdx = floor(twistedV);
        float withinStripe = fract(twistedV);

        // Per-stripe hash for variation
        float h1 = fract(sin(stripeIdx * 127.1) * 43758.5);
        float h2 = fract(sin(stripeIdx * 269.5) * 18397.3);
        float h3 = fract(sin(stripeIdx * 419.2) * 29517.7);

        // Width wobble: shift the stripe center slightly
        float wobble = (h1 - 0.5) * 0.15;
        float adjusted = withinStripe + wobble;

        // Groove shape with per-fiber width variation
        float width = 0.85 + h2 * 0.3; // fiber width varies 0.85-1.15
        float stripe = sin((adjusted - 0.5) * PI * width);
        stripeShade = 1.0 - fiberGrooveDepth * (0.5 - 0.5 * stripe);

        // Per-fiber color/brightness variation
        float brightness = 0.88 + h3 * 0.24; // 0.88 - 1.12
        stripeTint = vec3(
            brightness * (0.95 + 0.1 * fract(h1 * 7.3)),
            brightness * (0.95 + 0.1 * fract(h2 * 5.7)),
            brightness * (0.95 + 0.1 * fract(h3 * 3.1))
        );
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

    // Alpha: separate ply vs flyaway opacity + Fresnel edge fade
    float baseAlpha = mix(plyAlpha, flyawayAlpha, vFiberType);
    float edgeFade = pow(1.0 - max(dot(normalize(vNormal), V), 0.0), 2.0);
    float alpha = mix(baseAlpha, baseAlpha * 0.3, edgeFade * (1.0 - baseAlpha));

    fragColor = vec4(color, alpha);
}
