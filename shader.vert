#version 330 core
in vec3 pos, normal, tangent;

uniform mat4 mvp, mv;
uniform mat3 normalMat;
uniform mat4 shadowMatrix;

out vec3 vNormal, vPosition, vTangent;
out vec4 ShadowCoord;

void main()
{
    vPosition   = vec3(mv * vec4(pos, 1.0));
    vNormal     = normalMat * normal;
    vTangent    = normalMat * tangent;
    ShadowCoord = shadowMatrix * vec4(pos, 1.0);
    gl_Position = mvp * vec4(pos, 1.0);
}
