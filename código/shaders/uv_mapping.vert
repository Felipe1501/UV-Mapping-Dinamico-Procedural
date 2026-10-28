#version 450

layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;
    mat4 view;
    mat4 projection;
    float time;
    int mode;
} params;

// Atributos lidos do modelo glTF (ver ModelLoader.cpp)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragNormal;

void main()
{
    gl_Position =
        params.projection *
        params.view *
        params.model *
        vec4(inPosition, 1.0);

    // A UV de cada vértice vem do arquivo do modelo e é interpolada
    // pelo rasterizador: cada pixel da superfície recebe a sua coordenada
    // na textura. Isso é o mapeamento UV.
    fragUV = inUV;

    // A matriz model é só rotação, então serve também para as normais.
    fragNormal = mat3(params.model) * inNormal;
}
