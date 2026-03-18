#version 330 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;

uniform mat4 mvp, mv;
uniform mat3 normalMat;
uniform mat4 shadowMatrix;
uniform mat4 domLightVM;  // lightView * model
uniform mat4 domLightMVP; // lightProj * lightView * model

out vec3 vNormal, vPosition, vTangent;
out vec4 ShadowCoord;
out float vDomDepth;   // linear depth in light space
out vec4  vDomClip;    // clip-space position from light for DOM UV lookup

void main()
{
    vPosition   = vec3(mv * vec4(pos, 1.0));
    vNormal     = normalMat * normal;
    vTangent    = normalMat * tangent;
    ShadowCoord = shadowMatrix * vec4(pos, 1.0);
    vDomDepth   = -(domLightVM * vec4(pos, 1.0)).z;
    vDomClip    = domLightMVP * vec4(pos, 1.0);
    gl_Position = mvp * vec4(pos, 1.0);
}
