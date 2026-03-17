#version 330 core
in vec3 pos;

uniform mat4 mvp, mv;
uniform mat3 normalMat;
uniform mat4 shadowMatrix;

out vec3 vNormal, vPosition;
out vec4 ShadowCoord;

void main()
{
    vPosition = vec3(mv * vec4(pos, 1.0));
    vNormal = normalMat * vec3(0.0, 1.0, 0.0);
    ShadowCoord = shadowMatrix * vec4(pos, 1.0);
    gl_Position = mvp * vec4(pos, 1.0);
}