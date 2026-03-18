#version 330 core

out vec2 vUV;

void main()
{
    // Fullscreen triangle using gl_VertexID
    float x = float((gl_VertexID & 1) << 2) - 1.0;
    float y = float((gl_VertexID & 2) << 1) - 1.0;
    vUV = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.999, 1.0);
}
