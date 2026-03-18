#version 330 core

in vec3 vNormal, vPosition, vWorldPos;
in vec4 ShadowCoord;

uniform vec3 color, lightPos;
uniform float lightIntensity;
uniform sampler2DShadow shadowMap;
uniform int   checkerEnabled;
uniform float exposure;
uniform int   gammaEnabled;

out vec4 fragColor;

vec3 acesToneMap(vec3 x)
{
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(-vPosition);
    vec3 lightDir = normalize(lightPos - vPosition);
    vec3 halfVector = normalize(lightDir + viewDir);

    // Checker pattern
    vec3 surfColor = color;
    if (checkerEnabled > 0) {
        float scale = 0.5;
        float checker = mod(floor(vWorldPos.x * scale) + floor(vWorldPos.z * scale), 2.0);
        surfColor = mix(color * 0.7, color * 1.0, checker);
    }

    vec3 ambient = 0.2 * surfColor;
    float ndotl = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = 0.8 * surfColor * ndotl;
    float ndoth = max(dot(normal, halfVector), 0.0);
    vec3 specular = vec3(0.3) * pow(ndoth, 128.0) * float(ndotl > 0.0);

    vec3 projCoords = ShadowCoord.xyz / ShadowCoord.w;
    float visibility = 1.0;
    if(projCoords.x >= 0.0 && projCoords.x <= 1.0 && projCoords.y >= 0.0 && projCoords.y <= 1.0 && projCoords.z <= 1.0) {
        float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
        projCoords.z -= bias;
        vec2 ts = 0.5 / vec2(textureSize(shadowMap, 0));
        visibility = (texture(shadowMap, projCoords + vec3(-ts.x, -ts.y, 0))
                    + texture(shadowMap, projCoords + vec3( ts.x, -ts.y, 0))
                    + texture(shadowMap, projCoords + vec3(-ts.x,  ts.y, 0))
                    + texture(shadowMap, projCoords + vec3( ts.x,  ts.y, 0))) * 0.25;
    }

    vec3 finalColor = ambient + ((diffuse + specular) * lightIntensity) * visibility;

    finalColor *= exposure;
    if (gammaEnabled > 0) {
        finalColor = acesToneMap(finalColor);
        finalColor = pow(finalColor, vec3(1.0/2.2));
    }

    fragColor = vec4(finalColor, 1.0);
}
