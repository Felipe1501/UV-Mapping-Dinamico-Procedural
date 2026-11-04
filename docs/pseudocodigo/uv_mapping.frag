// ===========================================================================
// PSEUDOCÓDIGO - FRAGMENT SHADER (uv_mapping.frag)
// Tema: UV Mapping Dinâmico e Procedural
//
// Objetivo: calcular a cor de cada ponto do modelo. É aqui que acontece o
//   efeito: o UV é deslocado com o tempo (UV dinâmico) e/ou a cor é gerada
//   por uma função matemática (procedural).
// Entra:    fragUV e fragNormal (vindos do vertex shader, interpolados pelo
//           rasterizador), time e mode (Uniform Buffer) e a textura.
// Sai:      outColor, a cor final do pixel (vec4, RGBA).
// Roda:     uma vez por fragmento (cada pixel coberto por um triângulo do
//           modelo), a cada quadro.
//
// Fluxo dentro do shader:
//   1. calcula a luz (normal + direção da luz)
//   2. escolhe o modo (1 a 5) e calcula a cor desse modo
//   3. escreve outColor
//
// Os 5 modos (tecla 1 a 5 na aplicação):
//   1 textura   2 UV dinâmico   3 procedural   4 mistura   5 grade UV
//
// Laços: NÃO há laços. As três ondas do padrão procedural são uma soma de
//   tamanho fixo, escrita termo a termo. Como não há repetição, não existe
//   critério de parada. Cada fragmento faz sempre o mesmo número de
//   operações.
//
// Padrão dos comentários: Entra / Faz / Sai / Na tela.
// ===========================================================================

#version 450

// ---------------------------------------------------------------------------
// RECURSO: Uniform Buffer (set 0, binding 0), o mesmo do vertex shader.
// ---------------------------------------------------------------------------
layout(set = 0, binding = 0) uniform Parameters
{
    mat4 model;       // mat4: rotação do modelo (usada no vertex shader)
    mat4 view;        // mat4: câmera (usada no vertex shader)
    mat4 projection;  // mat4: projeção (usada no vertex shader)
    float time;       // float: segundos desde o início; faz o efeito andar
    int mode;         // int: modo de 1 a 5, igual para todos os pixels
} params;

// ---------------------------------------------------------------------------
// RECURSO: textura + sampler (set 0, binding 1, COMBINED_IMAGE_SAMPLER).
// sampler2D: imagem 2D já ligada às regras de leitura (o sampler).
// Configuração do sampler, feita na aplicação (createTextureSampler):
//   - filtro LINEAR na ampliação e na redução: mistura os 4 texels mais
//     próximos (com anisotropia, pode combinar mais amostras), para o
//     deslocamento lento não "pular" de texel em texel;
//   - endereçamento REPEAT em u e v: UV acima de 1 volta para o começo
//     (Eq. 3.7). É o que deixa o UV crescer sem limite no modo 2;
//   - mipmap LINEAR, mas a imagem tem 1 só nível (mipLevels = 1), então a
//     escolha de nível (Eq. 3.13) não tem efeito;
//   - anisotropia ligada só se a GPU suportar, no máximo que ela permite.
// A imagem é R8G8B8A8_SRGB: a leitura já devolve a cor em espaço linear.
// ---------------------------------------------------------------------------
layout(set = 0, binding = 1) uniform sampler2D baseTexture;

// ---------------------------------------------------------------------------
// ENTRADAS: saídas do vertex shader, já interpoladas para este pixel.
// ---------------------------------------------------------------------------
layout(location = 0) in vec2 fragUV;     // vec2: coordenada na textura
layout(location = 1) in vec3 fragNormal; // vec3: normal; a interpolação
                                         //   pode deixá-la menor que 1

// ---------------------------------------------------------------------------
// SAÍDA: cor do pixel, gravada no color attachment (location 0).
// ---------------------------------------------------------------------------
layout(location = 0) out vec4 outColor;  // vec4: vermelho, verde, azul, alfa

// Modos (mesmos valores de RenderMode na aplicação, VulkanApp.hpp)
const int MODE_TEXTURE = 1;     // textura original
const int MODE_DYNAMIC_UV = 2;  // textura com UV dinâmico
const int MODE_PROCEDURAL = 3;  // cor procedural, sem imagem
const int MODE_BLEND = 4;       // UV dinâmico + procedural (modo inicial)
const int MODE_UV_GRID = 5;     // grade para ver o próprio UV


// ===========================================================================
// UV DINÂMICO: dynamicUV (Eq. 3.5 translação e Eq. 3.6 ondulação)
// Entra:  uv (vec2, o UV do pixel) e time (float, segundos).
// Faz:    1) soma 0,10 ao u a cada segundo: a textura dá uma volta
//            completa sobre o modelo a cada 10 segundos;
//         2) soma uma ondulação pequena (amplitude 0,015), diferente em
//            cada região, que dá a impressão de líquido.
//         A linha de v usa o u que acabou de ser alterado (Eq. 3.6).
// Sai:    o UV deslocado (vec2). Pode passar de 1: o sampler REPEAT trata.
// Na tela: a malha não se deforma; a textura escorre e ondula sobre ela.
// ===========================================================================
vec2 dynamicUV(vec2 uv, float time)
{
    uv.x += time * 0.10;                            // translação (Eq. 3.5)

    uv.x += sin(uv.y * 12.0 + time * 2.0) * 0.015;  // ondulação em u
    uv.y += cos(uv.x * 12.0 + time * 1.5) * 0.015;  // ondulação em v

    return uv;
}


// ===========================================================================
// PADRÃO PROCEDURAL: proceduralPattern (Eq. 3.11 e Eq. 3.12)
// Entra:  baseUV (vec2) e time (float).
// Faz:    gera a cor só com cálculos, sem ler imagem nenhuma, em 6 passos.
// Sai:    vec4: rgb = cor do padrão; a = energy (0 a 1), a "força" da
//         onda naquele pixel, usada depois como máscara no modo 4.
// Na tela: ondas azuis e ciano que andam pela superfície.
// ===========================================================================
vec4 proceduralPattern(vec2 baseUV, float time)
{
    vec2 uv = baseUV;  // vec2: cópia do UV que vai ser deformada

    // Passo 1 - movimento: o padrão anda em u e balança em v.
    uv.x += time * 0.40;
    uv.y += sin(time * 1.2) * 0.10;

    // Passo 2 - distorção (amplitude 0,08): entorta o uv para as ondas não
    // ficarem retas. Na tela: as ondas ficam irregulares, menos retas.
    // distortionX e distortionY são calculadas com o mesmo uv e só depois
    // somadas.
    float distortionX = sin(uv.y * 14.0 + time * 2.0) * 0.08;  // desvio u
    float distortionY = cos(uv.x * 12.0 - time * 1.5) * 0.08;  // desvio v
    uv.x += distortionX;
    uv.y += distortionY;

    // Passo 3 - três ondas planas (Eq. 3.11). Cada uma é um seno de
    // (k . uv + w * t): k dá a direção e a frequência, w a velocidade.
    // Cada wave (float) vai de -1 a 1.
    float wave1 = sin(uv.x * 20.0 + uv.y * 8.0 + time * 4.0);  // k=(20,8)
    float wave2 = sin(uv.y * 24.0 - uv.x * 6.0 - time * 3.0);  // k=(-6,24)
    float wave3 = sin((uv.x + uv.y) * 18.0 + time * 2.0);      // k=(18,18)

    // Passo 4 - energia: a soma vai de -3 a 3. Dividir por 3 leva para
    // [-1, 1], e "* 0.5 + 0.5" leva para [0, 1]. Isso é necessário porque
    // energy vira peso de mix e smoothstep, que esperam valores de 0 a 1.
    float energy = (wave1 + wave2 + wave3) / 3.0;  // float, de -1 a 1
    energy = energy * 0.5 + 0.5;                   // float, de 0 a 1

    // Passo 5 - grade 8x8 (Eq. 3.8, escala). fract(uv * 8.0) repete o
    // intervalo [0, 1] oito vezes. grid (float) vale 1 perto das linhas
    // da grade e 0 no meio das células; smoothstep suaviza a borda.
    vec2 gridUV = fract(uv * 8.0);  // vec2: posição dentro da célula
    float gridX = smoothstep(0.46, 0.50, abs(gridUV.x - 0.5));
    float gridY = smoothstep(0.46, 0.50, abs(gridUV.y - 0.5));
    float grid = max(gridX, gridY);

    // Passo 6 - cor (Eq. 3.12, mix). Paleta fixa de quatro cores (vec3):
    vec3 deepBlue = vec3(0.005, 0.015, 0.12);  // energia baixa
    vec3 blue = vec3(0.0, 0.18, 0.85);
    vec3 cyan = vec3(0.0, 0.85, 1.0);
    vec3 white = vec3(0.65, 1.0, 1.0);         // energia máxima

    // mix(a, b, x) = a * (1 - x) + b * x. O ciano só aparece com energy
    // acima de 0,55 e o branco acima de 0,90 (smoothstep).
    vec3 color = mix(deepBlue, blue, energy);
    color = mix(color, cyan, smoothstep(0.55, 0.90, energy));
    color = mix(color, white, smoothstep(0.90, 1.0, energy));

    // Linhas da grade: mistura 25% com a cor escurecida para 35%.
    color = mix(color, color * 0.35, grid * 0.25);

    // Brilho nas cristas: soma até 0,25 no verde e 0,35 no azul.
    // A cor pode passar de 1 aqui (ver SAÍDA, no fim do main).
    float glow = pow(energy, 3.0);  // float: cresce rápido perto de 1
    color += vec3(0.0, 0.25, 0.35) * glow;

    return vec4(color, energy);
}


// ===========================================================================
// GRADE UV: uvGrid (depuração, Eq. 3.8 escala)
// Entra:  uv (vec2).
// Faz:    pinta u em vermelho e v em verde, desenha um xadrez 10x10 e
//         linhas brancas nas bordas das células.
// Sai:    cor (vec3).
// Na tela: mostra como a textura foi esticada sobre o modelo e onde ficam
//         as costuras do UV.
// ===========================================================================
vec3 uvGrid(vec2 uv)
{
    vec2 cells = uv * 10.0;  // vec2: 10 células em cada direção

    // checker (float): 0 ou 1, alterna como um tabuleiro de xadrez
    float checker = mod(floor(cells.x) + floor(cells.y), 2.0);

    // line (float): 1 na borda da célula, 0 no meio
    vec2 lineDistance = abs(fract(cells) - 0.5);
    float line = smoothstep(0.44, 0.48, max(lineDistance.x, lineDistance.y));

    // fract mantém u e v entre 0 e 1, para virarem cor válida
    vec3 color = vec3(fract(uv.x), fract(uv.y), 0.2);
    color *= 0.65 + 0.35 * checker;  // células alternadas mais escuras

    return mix(color, vec3(1.0), line);  // linhas em branco
}


void main()
{
    float time = params.time;  // float: t, em segundos

    // =======================================================================
    // 1. ILUMINAÇÃO DIFUSA (Eq. 3.14)
    // Entra:  fragNormal (vec3).
    // Faz:    normalize devolve a normal ao comprimento 1, porque a
    //         interpolação encurta o vetor. dot(normal, luz) é o cosseno do
    //         ângulo entre os dois; max(..., 0.0) zera o lado de trás.
    //         lighting soma 0,35 de luz ambiente, para nada ficar preto.
    // Sai:    diffuse (float, 0 a 1) e lighting (float, 0,35 a 1).
    // Na tela: o modelo tem um lado claro e um lado escuro, e dá para ver
    //         o volume.
    // =======================================================================
    vec3 normal = normalize(fragNormal);                 // vec3, comprimento 1
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.6)); // vec3: luz fixa
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float lighting = 0.35 + 0.65 * diffuse;

    vec3 color;  // vec3: cor do pixel antes da saída

    // =======================================================================
    // 2. ESCOLHA DO MODO (condição)
    // params.mode é igual para todos os pixels do quadro, então todos
    // seguem o mesmo ramo (sem divergência entre threads da GPU).
    // Cada ramo aplica a sua própria iluminação.
    // =======================================================================
    if (params.mode == MODE_TEXTURE)
    {
        // Modo 1. Faz: lê a textura no UV original (amostragem com o
        // sampler LINEAR/REPEAT) e aplica a luz.
        // Na tela: o modelo com a textura original, parada.
        color = texture(baseTexture, fragUV).rgb * lighting;
    }
    else if (params.mode == MODE_DYNAMIC_UV)
    {
        // Modo 2. Faz: lê a textura no UV deslocado por dynamicUV.
        // Na tela: a textura desliza e ondula; a malha não se deforma.
        color = texture(baseTexture, dynamicUV(fragUV, time)).rgb * lighting;
    }
    else if (params.mode == MODE_PROCEDURAL)
    {
        // Modo 3. Faz: usa só o padrão procedural, sem textura.
        // Luz mais clara (0,55 + 0,45), para o padrão parecer emitir luz.
        vec4 pattern = proceduralPattern(fragUV, time);  // rgb + energy
        color = pattern.rgb * (0.55 + 0.45 * diffuse);
    }
    else if (params.mode == MODE_UV_GRID)
    {
        // Modo 5. Faz: desenha a grade de depuração com a luz normal.
        color = uvGrid(fragUV) * lighting;
    }
    else
    {
        // Modo 4 (MODE_BLEND) e qualquer valor inválido (fora de 1 a 5):
        // cai aqui, então o shader nunca fica sem cor.
        // Faz: textura com UV dinâmico + ondas procedurais por cima.
        // waveMask (float, 0 a 0,75): só as regiões de energia alta (acima
        // de 0,45) mostram as ondas, e nunca cobrem mais de 75%.
        // A parte procedural entra sem nova iluminação.
        // Na tela: a textura escorrendo com ondas de luz passando por cima.
        vec3 textured =
            texture(baseTexture, dynamicUV(fragUV, time)).rgb * lighting;
        vec4 pattern = proceduralPattern(fragUV, time);
        float waveMask = smoothstep(0.45, 0.95, pattern.a) * 0.75;
        color = mix(textured, pattern.rgb, waveMask);
    }

    // =======================================================================
    // 3. SAÍDA
    // Entra:  color (vec3).
    // Faz:    junta a cor com alfa 1 (opaco). Não há clamp no shader: se o
    //         brilho passar de 1, o color attachment limita cada canal a
    //         [0, 1] ao gravar o pixel, porque usa um formato normalizado
    //         de 8 bits (a aplicação prefere B8G8R8A8_SRGB).
    // Sai:    outColor (vec4), a cor final do pixel.
    // =======================================================================
    outColor = vec4(color, 1.0);
}

// ===========================================================================
// CASOS EXTREMOS E VALORES INVÁLIDOS (resumo)
// - UV fora de [0, 1]: o modo 2 faz u crescer sem limite. O sampler REPEAT
//   volta para o começo da textura (Eq. 3.7). Na grade, fract faz isso.
// - Normal nula: normalize de um vetor zero não tem resultado válido. O
//   carregador do modelo (ModelLoader.cpp) calcula as normais que faltam
//   pela média das faces e troca normais de comprimento quase zero por
//   (0, 1, 0). Assim, nenhum vértice chega aqui com normal nula. Dentro
//   do triângulo, a interpolação só chegaria perto de zero se os vértices
//   tivessem normais quase opostas, o que não ocorre numa malha suave.
// - Luz por trás: max(..., 0.0) impede luz negativa, e o 0,35 de ambiente
//   evita preto total.
// - Energia procedural: normalizada para [0, 1] antes de virar peso.
// - Cor acima de 1: limitada pelo formato do color attachment na escrita.
// - Modo inválido: cai no último ramo (else), que é a mistura.
// - Tempo muito grande: time é float. Depois de cerca de 18 horas, o
//   menor passo de time chega a 0,008 s (quase meio quadro) e o movimento
//   fica em degraus; o UV deslocado (u + 0,10 t e u + 0,40 t) também
//   perde precisão. Não é tratado no shader. A correção possível, na
//   aplicação, está em "Limitações", no Capítulo 2 do PDF.
// ===========================================================================
