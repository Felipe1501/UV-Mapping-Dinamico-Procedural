// ===========================================================================
// PSEUDOCÓDIGO - VERTEX SHADER (uv_mapping.vert)
// Tema: UV Mapping Dinâmico e Procedural
//
// Objetivo: posicionar cada vértice do modelo na tela e entregar ao
//   rasterizador a coordenada UV e a normal desse vértice.
// Entra:    posição, normal e UV de um vértice (lidos do modelo glTF)
//           e as matrizes model, view e projection (Uniform Buffer).
// Sai:      gl_Position (posição na tela) e fragUV, fragNormal, que o
//           rasterizador interpola e entrega ao fragment shader.
// Roda:     uma vez por vértice, a cada quadro.
//
// Fluxo no pipeline:
//   Vertex Buffer -> [ESTE SHADER] -> rasterizador (interpola UV e normal)
//   -> fragment shader (uv_mapping.frag) -> cor do pixel
//
// Este shader NÃO muda o UV: ele só o repassa. Todo o efeito dinâmico e
// procedural é calculado por pixel, no fragment shader.
// Padrão dos comentários: Entra / Faz / Sai / Na tela.
// ===========================================================================

#version 450

// ---------------------------------------------------------------------------
// RECURSO: Uniform Buffer (set 0, binding 0), o mesmo nos dois shaders.
// A CPU reescreve este bloco a cada quadro (208 bytes, layout std140).
// ---------------------------------------------------------------------------
layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;       // mat4: gira o modelo (só rotação no eixo Y)
    mat4 view;        // mat4: posição e direção da câmera
    mat4 projection;  // mat4: projeção em perspectiva (com profundidade)
    float time;       // float: segundos desde o início (t nas equações);
                      //   não é usado aqui, só no fragment shader
    int mode;         // int: modo escolhido nas teclas 1 a 5;
                      //   não é usado aqui, só no fragment shader
} params;

// ---------------------------------------------------------------------------
// ENTRADAS: atributos de um vértice, lidos do Vertex Buffer
// (32 bytes por vértice: posição, normal e UV, todos em float).
// ---------------------------------------------------------------------------
layout(location = 0) in vec3 inPosition; // vec3: posição no espaço do modelo
layout(location = 1) in vec3 inNormal;   // vec3: normal, já unitária
                                         //   (o ModelLoader garante isso)
layout(location = 2) in vec2 inUV;       // vec2: coordenada (u, v) na
                                         //   textura, do TEXCOORD_0 do glTF

// ---------------------------------------------------------------------------
// SAÍDAS: vão para o rasterizador, que interpola cada uma dentro do
// triângulo, com correção de perspectiva (Eq. 3.4).
// ---------------------------------------------------------------------------
layout(location = 0) out vec2 fragUV;     // vec2: UV que o pixel vai usar
layout(location = 1) out vec3 fragNormal; // vec3: normal para a iluminação
// gl_Position (vec4) é a saída obrigatória do vertex shader.

void main()
{
    // -----------------------------------------------------------------------
    // 1. POSIÇÃO EM ESPAÇO DE RECORTE (Eq. 3.2, transformação da posição)
    // Entra:  inPosition (vec3) e as três matrizes.
    // Faz:    p_clip = projection * view * model * p.
    //         O vec4(..., 1.0) põe a posição em coordenadas homogêneas,
    //         para que a matriz possa também deslocar o ponto.
    // Sai:    gl_Position (vec4), em espaço de recorte.
    // Na tela: o modelo aparece girando, visto pela câmera.
    // Depois deste shader, o hardware divide gl_Position por w e aplica o
    //         viewport. O z/w (de 0 a 1) vira a profundidade do pixel e é
    //         comparado com o depth attachment (teste LESS: fica o ponto
    //         mais próximo). Este shader não escreve profundidade à mão.
    // -----------------------------------------------------------------------
    gl_Position =
        params.projection *
        params.view *
        params.model *
        vec4(inPosition, 1.0);

    // -----------------------------------------------------------------------
    // 2. COORDENADA UV (Eq. 3.1, coordenadas UV do vértice)
    // Entra:  inUV (vec2), entre 0 e 1.
    // Faz:    copia sem alterar. O rasterizador interpola o valor entre os
    //         três vértices do triângulo (Eq. 3.4).
    // Sai:    fragUV (vec2).
    // Na tela: cada pixel do modelo recebe a sua posição na textura.
    //         Isso é o mapeamento UV.
    // -----------------------------------------------------------------------
    fragUV = inUV;

    // -----------------------------------------------------------------------
    // 3. NORMAL (operação com matriz)
    // Entra:  inNormal (vec3) e a matriz model.
    // Faz:    gira a normal junto com o modelo. mat3(model) pega só a parte
    //         de rotação da matriz 4x4 (sem o deslocamento). Como model é
    //         só rotação, ela serve para normais sem precisar da inversa
    //         transposta, e a normal continua com comprimento 1.
    // Sai:    fragNormal (vec3).
    // Na tela: a luz fica parada enquanto o modelo gira; sem esta linha,
    //         a parte iluminada giraria junto com o pato.
    // -----------------------------------------------------------------------
    fragNormal = mat3(params.model) * inNormal;
}
