layout(location = 0) in vec2 fragUV;

layout(location = 1) in vec3 fragNormal;

layout(location = 2) in vec3 fragPosition;


// Cor final produzida pelo Fragment Shader.
layout(location = 0) out vec4 outColor;


layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;
    mat4 view;
    mat4 projection;

    float time;

    vec2 uvVelocity;
    vec2 uvScale;

} params;

layout(set = 0, binding = 1) uniform texture2D textureImage;

layout(set = 0, binding = 2) uniform sampler textureSampler;

float proceduralPattern(vec2 uv, float time)
{

    float wave =
        sin(
            uv.x * 10.0
            + uv.y * 10.0
            + time * 2.0
        );

    float normalizedWave =
        (wave + 1.0) * 0.5;


    return normalizedWave;
}


void main()
{

    vec2 uv = fragUV;


    vec2 sampledUV =
        fract(uv);

    vec4 textureColor =
        texture(
            sampler2D(textureImage, textureSampler),
            sampledUV
        );

    float proceduralValue =
        proceduralPattern(
            sampledUV,
            params.time
        );


    vec3 normal =
        normalize(fragNormal);


    vec3 lightDirection =
        normalize(vec3(0.4, 0.8, 0.6));

    float lightFactor =
        max(
            dot(normal, lightDirection),
            0.0
        );

    vec3 proceduralColor =
        vec3(
            proceduralValue,
            proceduralValue,
            proceduralValue
        );

    vec3 finalColor =
        mix(
            textureColor.rgb,
            proceduralColor,
            0.5
        );


    finalColor =
        finalColor * lightFactor;


    finalColor =
        clamp(
            finalColor,
            0.0,
            1.0
        );

    outColor =
        vec4(
            finalColor,
            textureColor.a
        );
}