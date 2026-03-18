#version 330 core

in vec2 vUV;

uniform vec3 bgTop;
uniform vec3 bgBot;
uniform int  gammaEnabled;

out vec4 fragColor;

void main()
{
    vec3 color = mix(bgBot, bgTop, vUV.y);
    if (gammaEnabled > 0)
        color = pow(color, vec3(1.0/2.2));
    fragColor = vec4(color, 1.0);
}
