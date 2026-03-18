// Marschner simplified three-lobe hair model

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
