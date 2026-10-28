# UV Mapping Dinâmico e Procedural

Projeto desenvolvido para a disciplina de **Computação Gráfica e Processamento de Imagens** da Universidade Católica de Santos.

Nesta etapa, a técnica passou a ser aplicada sobre um **objeto 3D real**. As coordenadas UV agora vêm do próprio modelo, e não mais de um triângulo com valores fixos no código. O modelo é carregado de um arquivo glTF junto com a sua textura, e a GPU usa as UVs de cada vértice para aplicar a textura na superfície. Sobre esse mapeamento são aplicadas as variações **dinâmica** (UVs animadas no tempo) e **procedural** (padrão gerado por funções matemáticas no espaço UV).

O carregamento de modelos segue a abordagem da [Eruption Engine](https://github.com/eruptionlabs/eruption-engine), indicada pelo professor como referência.

---

## 1. O que é o mapeamento UV nesta entrega

Um modelo 3D é uma superfície; uma textura é uma imagem plana. Para "vestir" o modelo com a imagem, cada vértice recebe uma coordenada **(u, v)** que aponta para um ponto da textura, entre (0, 0) e (1, 1). Essas coordenadas são definidas pelo artista quando ele "desdobra" o modelo sobre o plano da imagem, e ficam gravadas no arquivo (atributo `TEXCOORD_0` do glTF).

```text
Arquivo glTF (Duck.glb)
   │
   ├── posições dos vértices ─────────┐
   ├── normais                        │
   ├── coordenadas UV (TEXCOORD_0) ───┼──► Vertex Buffer ──► Vertex Shader
   ├── índices dos triângulos ─────────┘                         │
   └── textura (imagem PNG embutida) ──► VkImage + Sampler       │  UV interpolada
                                                │                ▼  por pixel
                                                └──────────► Fragment Shader
                                                              texture(textura, uv)
```

O rasterizador interpola a UV entre os três vértices de cada triângulo, então cada pixel da superfície recebe a sua própria coordenada na textura. No fragment shader, a cor é lida da textura nessa coordenada.

Com a UV disponível por pixel, o shader pode:

- ler a textura **exatamente** na UV do modelo (mapeamento clássico);
- **alterar a UV com o tempo** antes da leitura (a malha fica parada, o mapeamento se move);
- **gerar a cor** a partir da UV com funções matemáticas (textura procedural).

---

## 2. Modos de visualização

Os modos são trocados pelas teclas **1** a **5**; o modo atual aparece no título da janela.

| Tecla | Modo | O que demonstra |
|-------|------|-----------------|
| `1` | Textura original | Mapeamento UV clássico: a textura aplicada pelas UVs do modelo. O olho e o bico da imagem caem exatamente no lugar certo da malha. |
| `2` | UV dinâmica | As UVs são deslocadas e onduladas com o tempo antes da leitura. A textura desliza sobre o pato. |
| `3` | Procedural | O padrão de ondas das etapas anteriores, agora calculado sobre as UVs do modelo. |
| `4` | Textura + procedural | Textura com UV dinâmica, e as regiões de maior energia do padrão procedural passam por cima como ondas de luz. |
| `5` | Grade UV | Depuração: U vira vermelho, V vira verde e uma grade 10×10 mostra como o espaço da textura foi esticado sobre a malha. |

Outros controles: **Espaço** pausa/retoma a rotação, **Esc** fecha.

### Observação sobre a densidade das UVs

O modo 5 revela que o artista do pato reservou uma área **muito pequena** da textura para a cabeça e o corpo, que são amarelos lisos e não precisam de resolução, e áreas grandes para o olho, o bico e a asa. Por isso, no modo 3 o padrão procedural aparece suave no corpo e mais detalhado nessas regiões. Esse é o comportamento esperado: um padrão gerado no espaço UV herda a distribuição das UVs do modelo.

---

## 3. Requisitos

Para compilar e executar o projeto são necessários:

- C++17
- Vulkan SDK (fornece o loader, as validation layers e o `glslc`)
- GLFW
- CMake 3.20 ou superior
- Conexão com a internet na primeira configuração do CMake

As bibliotecas **glm** (matemática) e **tinygltf** (leitura de modelos) são baixadas automaticamente pelo CMake durante a configuração, com versão e hash fixos. Não é preciso instalá-las.

### Ambientes

- Windows 11, x64, MSVC 19.44 (Visual Studio 2022 Build Tools), Vulkan 1.4.357, NVIDIA RTX 4060 Ti: ambiente onde esta etapa foi testada
- macOS, ARM64, Apple Clang, Vulkan SDK 1.4.357 com MoltenVK: ambiente da etapa anterior

No macOS, o Vulkan é executado através do **MoltenVK**, utilizando a infraestrutura gráfica do sistema.

---

## 4. Estrutura do projeto

```text
uv-mapping-dinamico-procedural-cod/
├── CMakeLists.txt
├── README.md
│
├── src/
│   ├── main.cpp
│   ├── VulkanApp.hpp
│   ├── VulkanApp.cpp
│   ├── ModelLoader.hpp
│   └── ModelLoader.cpp
│
├── shaders/
│   ├── uv_mapping.vert
│   ├── uv_mapping.frag
│   ├── uv_mapping.vert.spv
│   └── uv_mapping.frag.spv
│
├── assets/
│   └── models/
│       ├── Duck.glb
│       └── LICENSE-Duck-SCEA.txt
│
└── evidencias/
    ├── modo1_textura_original.png
    ├── modo2_uv_dinamica.png
    ├── modo3_procedural.png
    ├── modo4_textura_procedural.png
    ├── modo5_grade_uv.png
    └── resultado.png
```

A pasta `build/` é criada durante a compilação e não faz parte do código-fonte necessário para a reprodução do projeto.

---

## 5. Compilação

Na pasta raiz do projeto:

```bash
cmake -S . -B build
```

Depois:

```bash
cmake --build build -j
```

O executável será criado em:

```text
build/UVMappingEntrega4
```

### Shaders

Se o `glslc` do Vulkan SDK for encontrado, o próprio build recompila os shaders GLSL para SPIR-V sempre que eles mudarem. Os `.spv` gerados ficam em `shaders/`.

Sem o `glslc`, a aplicação usa os `.spv` já versionados no repositório. A compilação manual continua possível:

```bash
glslc shaders/uv_mapping.vert -o shaders/uv_mapping.vert.spv
glslc shaders/uv_mapping.frag -o shaders/uv_mapping.frag.spv
```

---

## 6. Execução

A aplicação deve ser executada **a partir da pasta raiz do projeto**, pois os shaders e o modelo são carregados por caminho relativo:

```bash
./build/UVMappingEntrega4
```

Por padrão é carregado `assets/models/Duck.glb`. Qualquer outro modelo `.glb` ou `.gltf` pode ser passado como argumento:

```bash
./build/UVMappingEntrega4 caminho/para/modelo.glb
```

Modelos sem textura recebem um padrão xadrez, que também evidencia o mapeamento.

Se a validation layer (`VK_LAYER_KHRONOS_validation`) não estiver instalada, a aplicação avisa no terminal e roda normalmente, apenas sem as verificações.

---

## 7. Principais arquivos

### `src/main.cpp`

Responsável por iniciar a aplicação, escolher o modelo (padrão ou argumento) e tratar exceções.

### `src/ModelLoader.hpp` e `src/ModelLoader.cpp`

Carregamento do modelo glTF com a biblioteca **tinygltf**, na mesma versão (v2.9.3) e com a mesma lógica de leitura do `GltfParser` da Eruption Engine:

- percorre os nós da cena, acumulando as transformações de cada nó;
- lê de cada primitiva os atributos `POSITION`, `NORMAL` e `TEXCOORD_0` e os índices (8, 16 ou 32 bits), respeitando o `byteStride` dos buffers;
- recalcula as normais quando o modelo não as possui;
- extrai a textura `baseColor` do material, já decodificada, e converte para RGBA de 8 bits;
- centraliza o modelo na origem e ajusta a escala para que qualquer modelo caiba na tela.

### `src/VulkanApp.hpp`

Declaração da classe `VulkanApp`, dos modos de visualização e do `UniformBufferObject`. Os `static_assert` sobre o UBO garantem que o layout em C++ corresponde ao bloco `Parameters` dos shaders.

### `src/VulkanApp.cpp`

Implementação da aplicação Vulkan, incluindo:

- criação da instância e habilitação opcional da validation layer;
- criação da Surface, seleção da GPU e criação do dispositivo lógico;
- criação da Swapchain e das Image Views;
- criação do **depth buffer**, para que as partes da frente do modelo escondam as de trás;
- criação do Render Pass com anexos de cor e profundidade;
- criação do Descriptor Set com o Uniform Buffer e a **textura com sampler**;
- criação do pipeline gráfico com os atributos do vértice, teste de profundidade e descarte de faces traseiras;
- envio de vértices, índices e textura para a memória da GPU através de *staging buffers*;
- criação dos Command Buffers com desenho indexado;
- sincronização, loop principal, controles de teclado e limpeza dos recursos.

### `shaders/uv_mapping.vert`

Transforma cada vértice pelas matrizes model, view e projection, e repassa a UV e a normal para o fragment shader.

### `shaders/uv_mapping.frag`

Calcula a cor de cada pixel conforme o modo escolhido: leitura da textura pela UV, UV dinâmica, padrão procedural, mistura ou grade de depuração. Aplica também uma iluminação difusa simples, para dar leitura de volume ao modelo.

### Arquivos `.spv`

Versões compiladas dos shaders utilizadas pelo Vulkan.

---

## 8. Uniform Buffer

A cada frame a aplicação envia aos shaders, através de um **Uniform Buffer Object (UBO)**:

| Campo | Offset (std140) | Uso |
|-------|-----------------|-----|
| `model` | 0 | Rotação do modelo em torno do eixo Y |
| `view` | 64 | Posição e direção da câmera |
| `projection` | 128 | Projeção em perspectiva (com o eixo Y invertido para o Vulkan) |
| `time` | 192 | Tempo desde o início, usado nas animações de UV e no padrão procedural |
| `mode` | 196 | Modo de visualização escolhido pelo teclado |

A textura é enviada em um segundo binding do mesmo Descriptor Set, como `sampler2D`. O sampler usa o modo de endereçamento **REPEAT**, que faz a textura se repetir fora do intervalo [0, 1] e permite deslocar as UVs continuamente.

---

## 9. Renderização

```text
Duck.glb ──► ModelLoader (tinygltf)
                │
                ▼
   vértices, índices, textura
                │  staging buffers
                ▼
        memória da GPU
                │
Aplicação ──► Command Buffer
                │
                ▼
         Render Pass (cor + profundidade)
                │
                ▼
         Graphics Pipeline
                │
                ├── Vertex Shader   (matrizes MVP, repassa UV e normal)
                │
                └── Fragment Shader (UV ──► textura / padrão procedural)
                │
                ▼
         Graphics Queue ──► Swapchain ──► Janela
```

---

## 10. Funcionalidades implementadas

Nesta entrega estão funcionando:

- carregamento de modelos glTF (`.glb` e `.gltf`) com tinygltf;
- leitura das coordenadas UV, normais e índices do modelo;
- carregamento da textura do material do modelo;
- mapeamento UV da textura sobre o objeto 3D;
- animação das coordenadas UV (UV dinâmica);
- padrão procedural calculado no espaço UV do modelo;
- visualização de depuração da parametrização UV;
- câmera em perspectiva e rotação do modelo (matrizes model, view e projection);
- depth buffer e descarte de faces traseiras;
- vertex e index buffers na memória local da GPU;
- textura com sampler e filtragem anisotrópica (quando suportada);
- troca de modos pelo teclado;
- compilação automática dos shaders pelo CMake;
- validation layer opcional;
- toda a infraestrutura Vulkan da etapa anterior.

---

## 11. Funcionalidades futuras

- geração de mipmaps para a textura;
- recriação da swapchain para permitir redimensionar a janela;
- troca de modelo em tempo de execução;
- controle da câmera pelo mouse.

---

## 12. Modelo e licenças

O modelo **Duck** foi obtido do repositório oficial de modelos de exemplo do Khronos Group, [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Duck).

Copyright 2005 Sony Computer Entertainment Inc. Licenciado sob a SCEA Shared Source License 1.0, cuja cópia acompanha o modelo em `assets/models/LICENSE-Duck-SCEA.txt`. O arquivo foi utilizado sem modificações.

Bibliotecas baixadas pelo CMake:

- [glm](https://github.com/g-truc/glm) 1.0.1, licença MIT
- [tinygltf](https://github.com/syoyo/tinygltf) v2.9.3, licença MIT (inclui stb_image e nlohmann/json)

---

## 13. Evidências

A pasta `evidencias/` contém registros da execução da aplicação.

```text
evidencias/
├── modo1_textura_original.png      mapeamento UV da textura original
├── modo2_uv_dinamica.png           UVs animadas: o olho saiu da cabeça e foi para o corpo
├── modo3_procedural.png            padrão procedural sobre as UVs do modelo
├── modo4_textura_procedural.png    textura com UV dinâmica + padrão procedural
├── modo5_grade_uv.png              grade de depuração da parametrização UV
└── resultado.png                   etapa anterior (triângulo, macOS)
```
