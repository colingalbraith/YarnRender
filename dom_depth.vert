#version 330 core
// DOM Pass 1: render yarn from light view, output linear depth
layout(location = 0) in vec3 pos;

uniform mat4 mvp;
uniform mat4 lightViewModel; // lightView * model

out float vLinearDepth;

void main()
{
    vLinearDepth = -(lightViewModel * vec4(pos, 1.0)).z;
    gl_Position  = mvp * vec4(pos, 1.0);
}
