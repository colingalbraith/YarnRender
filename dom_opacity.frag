#version 330 core
// DOM Pass 2: accumulate per-layer opacity with additive blending
// R = cumulative opacity through layer 1
// G = cumulative opacity through layer 2
// B = cumulative opacity through layer 3

in float vLinearDepth;

uniform sampler2D domDepthMap;
uniform vec3 domLayers; // x=d1, y=d2, z=d3 in world units
uniform float domFragOpacity;

out vec4 fragOut;

void main()
{
    // look up z0 at this pixel (same viewport as depth pass)
    vec2 uv  = gl_FragCoord.xy / vec2(textureSize(domDepthMap, 0));
    float z0 = texture(domDepthMap, uv).r;

    float delta = vLinearDepth - z0;
    if (delta < -0.01) discard;
    delta = max(delta, 0.0);

    float op = domFragOpacity;
    fragOut  = vec4(0.0);

    // fragment in layer k contributes to layers k..3
    if      (delta <= domLayers.x) fragOut.rgb = vec3(op);
    else if (delta <= domLayers.y) fragOut.gb  = vec2(op);
    else                           fragOut.b   = op;
}
