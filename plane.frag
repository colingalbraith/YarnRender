#version 330 core

in vec3 vNormal, vPosition;
in vec4 ShadowCoord;

uniform vec3 color, lightPos;
uniform float lightIntensity;
uniform sampler2DShadow shadowMap;

out vec4 fragColor;

void main()
{
    // vectors
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(-vPosition);
    vec3 lightDir = normalize(lightPos - vPosition);
    vec3 halfVector = normalize(lightDir + viewDir);

    // ambient
    vec3 ambient = 0.2 * color;

    // diffuse
    float ndotl = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = 0.8 * color * ndotl;

    // specular
    float shininess = 128.0;
    float ndoth = max(dot(normal, halfVector), 0.0);
    vec3 specular = vec3(1.0) * pow(ndoth, shininess) * float(ndotl > 0.0);

    vec3 projCoords = ShadowCoord.xyz / ShadowCoord.w;

    float visibility = 1.0;

    if(projCoords.x >= 0.0 && projCoords.x <= 1.0 && projCoords.y >= 0.0 && projCoords.y <= 1.0 && projCoords.z <= 1.0) {
        // bias
        float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
        projCoords.z -= bias;
        visibility = texture(shadowMap, projCoords);
    }else {visibility = 1.0;}

    // final color
    vec3 finalColor = ambient + ((diffuse + specular) * lightIntensity) * visibility;
    fragColor = vec4(finalColor, 1.0);
}