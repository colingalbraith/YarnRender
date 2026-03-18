// Yarn shader: hybrid tangent-normal model for twisted plied yarn
//
// Dual shifted specular (primary white + secondary colored), Fresnel,
// fuzz lobe for inter-fiber scatter, transmission backlight,
// cross-section shading with separate diffuse/specular treatment,
// and tangent perturbation for natural highlight breakup.

vec3 yarnShader(vec3 T, vec3 N, vec3 L, vec3 V, vec3 bc, out vec3 ambient)
{
    ambient = y_ambient * bc;

    // ── tangent perturbation for natural highlight variation ──
    // break up the perfectly uniform specular band
    vec3 B = cross(T, N);
    float pNoise = valueNoise(vPosition * y_tangentNoise * 3.0);
    vec3 Tp = normalize(T + B * pNoise * 0.12 * y_tangentNoise);

    // ── hybrid diffuse: blend tangent-based and normal-based ──
    float TdotL = dot(Tp, L);
    float sinTL = sqrt(max(1.0 - TdotL * TdotL, 0.0));
    float NdotL_raw = dot(N, L);
    float NdotL = (NdotL_raw + y_wrap) / (1.0 + y_wrap);
    NdotL = max(NdotL, 0.0);
    float diff = mix(NdotL, sinTL, y_tangentBlend);
    vec3 diffuse = y_diffuse * bc * diff;

    // ── dual shifted specular (Kajiya-Kay style) ──
    float TdotV = dot(Tp, V);
    float sinTV = sqrt(max(1.0 - TdotV * TdotV, 0.0));

    // Fresnel: specular intensifies at grazing angles
    float fresnel = mix(1.0, pow(1.0 - sinTV, 5.0), y_fresnel);

    // primary specular: sharp, near-white, shifted toward root
    float TdotL1 = dot(Tp, L) - y_specShift1;
    float sinTL1 = sqrt(max(1.0 - TdotL1 * TdotL1, 0.0));
    float cosHalf1 = TdotL1 * TdotV + sinTL1 * sinTV;
    vec3 spec1 = vec3(y_specular) * pow(max(cosHalf1, 0.0), y_shininess) * fresnel;

    // secondary specular: broader, color-tinted, shifted toward tip
    float TdotL2 = dot(Tp, L) + y_specShift2;
    float sinTL2 = sqrt(max(1.0 - TdotL2 * TdotL2, 0.0));
    float cosHalf2 = TdotL2 * TdotV + sinTL2 * sinTV;
    vec3 spec2 = y_spec2Tint * bc * pow(max(cosHalf2, 0.0), y_shininess2) * fresnel;

    vec3 spec = spec1 + spec2;

    // ── fuzz lobe: broad scattered highlight from inter-fiber bouncing ──
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 fuzz = y_fuzz * bc * pow(NdotH, y_fuzzWidth);

    // ── transmission / backscatter: light through thin fibers ──
    float scatter = pow(max(dot(-L, V), 0.0), y_transPower);
    vec3 trans = y_transmission * bc * scatter;

    // ── cross-section shading (separate for diffuse vs specular) ──
    float csRaw = max(NdotL_raw, 0.0);
    float csDiffuse  = mix(0.55, 1.0, csRaw);   // stronger darkening on diffuse
    float csSpecular = mix(0.75, 1.0, csRaw);   // gentler on specular (don't kill highlights)

    return ambient
         + (diffuse * csDiffuse + (spec + fuzz) * csSpecular + trans) * lightIntensity;
}
