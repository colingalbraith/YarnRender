#version 330 core
// DOM Pass 1: write linear depth of nearest surface (z0)
in float vLinearDepth;

out vec4 fragOut;

void main()
{
    fragOut = vec4(vLinearDepth, 0.0, 0.0, 1.0);
}
