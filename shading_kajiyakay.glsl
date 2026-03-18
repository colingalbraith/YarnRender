// Kajiya-Kay anisotropic hair/fiber model

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
