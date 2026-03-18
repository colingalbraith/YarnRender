// Blinn-Phong with wrap diffuse

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
