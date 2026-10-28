#version 450

layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;
    mat4 view;
    mat4 projection;
    float time;
    int mode;
} params;

// Textura do modelo, lida através das coordenadas UV
layout(set = 0, binding = 1) uniform sampler2D baseTexture;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

// Modos (mesmos valores de RenderMode em VulkanApp.hpp)
const int MODE_TEXTURE = 1;
const int MODE_DYNAMIC_UV = 2;
const int MODE_PROCEDURAL = 3;
const int MODE_BLEND = 4;
const int MODE_UV_GRID = 5;


// UV DINÂMICA
// As coordenadas UV do modelo são alteradas ao longo do tempo antes de
// ler a textura. A malha não se move: o que se move é o mapeamento.

vec2 dynamicUV(vec2 uv, float time)
{
    // Deslocamento contínuo: a textura desliza sobre a superfície
    // (o sampler usa REPEAT, então ela se repete ao passar de 1.0)
    uv.x += time * 0.10;

    // Ondulação: cada região recebe um deslocamento diferente
    uv.x += sin(uv.y * 12.0 + time * 2.0) * 0.015;
    uv.y += cos(uv.x * 12.0 + time * 1.5) * 0.015;

    return uv;
}


// PADRÃO PROCEDURAL
// Cor gerada por funções matemáticas a partir da UV, sem imagem.
// Retorna a cor em rgb e a "energia" da onda em a.

vec4 proceduralPattern(vec2 baseUV, float time)
{
    vec2 uv = baseUV;

    uv.x += time * 0.40;
    uv.y += sin(time * 1.2) * 0.10;


    // DISTORÇÃO PROCEDURAL

    float distortionX =
        sin(uv.y * 14.0 + time * 2.0) * 0.08;

    float distortionY =
        cos(uv.x * 12.0 - time * 1.5) * 0.08;

    uv.x += distortionX;
    uv.y += distortionY;


    // ONDA PRINCIPAL

    float wave1 =
        sin(
            uv.x * 20.0
            + uv.y * 8.0
            + time * 4.0
        );

    float wave2 =
        sin(
            uv.y * 24.0
            - uv.x * 6.0
            - time * 3.0
        );

    float wave3 =
        sin(
            (uv.x + uv.y) * 18.0
            + time * 2.0
        );


    // Combinação das ondas

    float energy =
        (wave1 + wave2 + wave3) / 3.0;

    energy =
        energy * 0.5 + 0.5;


    // PADRÃO DE GRADE

    vec2 gridUV =
        fract(uv * 8.0);

    float gridX =
        smoothstep(
            0.46,
            0.50,
            abs(gridUV.x - 0.5)
        );

    float gridY =
        smoothstep(
            0.46,
            0.50,
            abs(gridUV.y - 0.5)
        );

    float grid =
        max(gridX, gridY);


    // CORES

    vec3 deepBlue =
        vec3(
            0.005,
            0.015,
            0.12
        );

    vec3 blue =
        vec3(
            0.0,
            0.18,
            0.85
        );

    vec3 cyan =
        vec3(
            0.0,
            0.85,
            1.0
        );

    vec3 white =
        vec3(
            0.65,
            1.0,
            1.0
        );


    // GRADIENTE DE ENERGIA

    vec3 color =
        mix(
            deepBlue,
            blue,
            energy
        );

    color =
        mix(
            color,
            cyan,
            smoothstep(0.55, 0.90, energy)
        );

    color =
        mix(
            color,
            white,
            smoothstep(0.90, 1.0, energy)
        );


    // GRADE SUTIL

    color =
        mix(
            color,
            color * 0.35,
            grid * 0.25
        );


    // BRILHO

    float glow =
        pow(energy, 3.0);

    color +=
        vec3(0.0, 0.25, 0.35) * glow;

    return vec4(color, energy);
}


// GRADE UV (DEPURAÇÃO)
// Mostra a própria parametrização: U vira vermelho, V vira verde, e uma
// grade 10x10 revela como o espaço da textura foi esticado sobre a malha.

vec3 uvGrid(vec2 uv)
{
    vec2 cells =
        uv * 10.0;

    float checker =
        mod(floor(cells.x) + floor(cells.y), 2.0);

    vec2 lineDistance =
        abs(fract(cells) - 0.5);

    float line =
        smoothstep(0.44, 0.48, max(lineDistance.x, lineDistance.y));

    vec3 color =
        vec3(fract(uv.x), fract(uv.y), 0.2);

    color *=
        0.65 + 0.35 * checker;

    return mix(color, vec3(1.0), line);
}


void main()
{
    float time = params.time;

    // ILUMINAÇÃO
    // Luz direcional simples, só para dar leitura de volume ao modelo

    vec3 normal =
        normalize(fragNormal);

    vec3 lightDirection =
        normalize(vec3(0.4, 0.8, 0.6));

    float diffuse =
        max(dot(normal, lightDirection), 0.0);

    float lighting =
        0.35 + 0.65 * diffuse;

    vec3 color;

    if (params.mode == MODE_TEXTURE)
    {
        color =
            texture(baseTexture, fragUV).rgb * lighting;
    }
    else if (params.mode == MODE_DYNAMIC_UV)
    {
        color =
            texture(baseTexture, dynamicUV(fragUV, time)).rgb * lighting;
    }
    else if (params.mode == MODE_PROCEDURAL)
    {
        vec4 pattern =
            proceduralPattern(fragUV, time);

        // O padrão também emite luz própria, como no efeito original
        color =
            pattern.rgb * (0.55 + 0.45 * diffuse);
    }
    else if (params.mode == MODE_UV_GRID)
    {
        color =
            uvGrid(fragUV) * lighting;
    }
    else
    {
        // MISTURA: textura com UV dinâmica, e as regiões de maior energia
        // do padrão procedural passam por cima dela como ondas de luz

        vec3 textured =
            texture(baseTexture, dynamicUV(fragUV, time)).rgb * lighting;

        vec4 pattern =
            proceduralPattern(fragUV, time);

        float waveMask =
            smoothstep(0.45, 0.95, pattern.a) * 0.75;

        color =
            mix(textured, pattern.rgb, waveMask);
    }


    // SAIDA

    outColor =
        vec4(color, 1.0);
}
