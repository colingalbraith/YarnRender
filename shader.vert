#version 330 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 fiberCol;

uniform mat4 mvp, mv;
uniform mat3 normalMat;
uniform mat4 shadowMatrix;
uniform mat4 domLightVM;  // lightView * model
uniform mat4 domLightMVP; // lightProj * lightView * model

out vec3 vNormal, vPosition, vTangent, vFiberColor;
out vec4 ShadowCoord;
out float vDomDepth;
out vec4  vDomClip;

void main()
{
    vPosition    = vec3(mv * vec4(pos, 1.0));
    vNormal      = normalMat * normal;
    vTangent     = normalMat * tangent;
    vFiberColor  = fiberCol;
    ShadowCoord = shadowMatrix * vec4(pos, 1.0);
    vDomDepth   = -(domLightVM * vec4(pos, 1.0)).z;
    vDomClip    = domLightMVP * vec4(pos, 1.0);
    gl_Position = mvp * vec4(pos, 1.0);
}
