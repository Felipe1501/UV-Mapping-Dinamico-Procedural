layout(location = 0) in vec3 inPosition;

layout(location = 1) in vec3 inNormal;

// Coordenada UV associada ao vértice.
// U e V normalmente pertencem ao intervalo [0, 1].
layout(location = 2) in vec2 inUV;


layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;
    mat4 view;
    mat4 projection;

    float time;

    vec2 uvVelocity;

    vec2 uvScale;

} params;


layout(location = 0) out vec2 fragUV;

layout(location = 1) out vec3 fragNormal;

layout(location = 2) out vec3 fragPosition;


void main()
{

    vec4 worldPosition =
        params.model * vec4(inPosition, 1.0);

    vec4 viewPosition =
        params.view * worldPosition;

    gl_Position =
        params.projection * viewPosition;


    fragPosition = worldPosition.xyz;

    fragNormal = inNormal;

    vec2 scaledUV =
        inUV * params.uvScale;

    vec2 uvOffset =
        params.uvVelocity * params.time;

    fragUV =
        scaledUV + uvOffset;
}