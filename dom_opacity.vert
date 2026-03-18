#version 330 core
// DOM Pass 2: render yarn from light view for opacity accumulation
layout(location = 0) in vec3 pos;

uniform mat4 mvp;
uniform mat4 lightViewModel;

out float vLinearDepth;

void main()
{
    vLinearDepth = -(lightViewModel * vec4(pos, 1.0)).z;
    gl_Position  = mvp * vec4(pos, 1.0);
}
