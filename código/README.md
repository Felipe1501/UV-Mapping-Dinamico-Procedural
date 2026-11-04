# UV Mapping Dinâmico e Procedural: código

Projeto da disciplina **Computação Gráfica e Processamento de Imagens** da Universidade Católica de Santos.

Esta pasta tem a aplicação Vulkan em C++17 que integra os shaders descritos no Capítulo 5 do PDF. O programa abre uma janela, inicializa o Vulkan, carrega um modelo 3D (o pato `Duck.glb`, com a sua textura) e o desenha com os shaders `uv_mapping.vert` e `uv_mapping.frag`. Sobre o mapeamento UV do modelo, o fragment shader aplica as variações **dinâmica** (o UV muda com o tempo) e **procedural** (a cor é calculada por funções matemáticas).

O carregamento de modelos segue a abordagem da [Eruption Engine](https://github.com/eruptionlabs/eruption-engine), indicada pelo professor como referência.

---

## 1. O que a aplicação mostra

Os modos são trocados pelas teclas **1** a **5**, e o modo atual aparece no título da janela. A aplicação abre no modo **4**.

| Tecla | Modo | O que demonstra |
|-------|------|-----------------|
| `1` | Textura original | Mapeamento UV clássico: a textura aplicada pelas UVs do modelo. |
| `2` | UV dinâmico | O UV é deslocado e ondulado com o tempo antes da leitura: a textura escorre sobre o pato, que não se deforma. |
| `3` | Procedural | Ondas calculadas a partir do UV, sem ler imagem nenhuma. |
| `4` | Textura + procedural | Textura com UV dinâmico, e as regiões de maior energia do padrão procedural passam por cima como ondas de luz. |
| `5` | Grade UV | Depuração: U vira vermelho, V vira verde e uma grade 10×10 mostra como o espaço da textura foi esticado sobre a malha. |

Outros controles: **Espaço** pausa e retoma a rotação, **Esc** fecha. A janela tem tamanho fixo (800×600); minimizada, a renderização pausa até ela voltar.

O modo 5 mostra que o artista do pato reservou uma área **muito pequena** da textura para a cabeça e o corpo, que são amarelos lisos, e áreas grandes para o olho, o bico e a asa. Por isso, no modo 3 o padrão procedural aparece suave no corpo e mais detalhado nessas regiões: um padrão gerado no espaço UV herda a distribuição das UVs do modelo.

---

## 2. Bibliotecas, ferramentas e versões

| Item | Para que serve | Versão testada |
|------|----------------|----------------|
| Compilador C++17 | compilar o programa | MSVC 19.44.35227 (Visual Studio 2022 Build Tools) |
| CMake 3.20 ou superior | configurar o projeto | 3.31.6 (vem com o Visual Studio) |
| Ninja | gerador do build | 1.12.1 (vem com o Visual Studio) |
| Vulkan: headers e loader | API gráfica | 1.4.357 (pelo vcpkg) |
| Vulkan: validation layers | verificar o uso da API (opcional) | 1.4.357 (pelo vcpkg) |
| glslc (shaderc) | compilar os shaders para SPIR-V | shaderc 2026.2 (pelo vcpkg) |
| GLFW 3 | janela e teclado | 3.5.1 (pelo vcpkg) |
| glm | matrizes model, view e projection | 1.0.1 (baixada pelo CMake) |
| tinygltf | leitura do modelo `.glb` | v2.9.3 (baixada pelo CMake) |

**Ambiente testado:** Windows 11 x64, NVIDIA GeForce RTX 4060 Ti.

A aplicação pede a versão **1.0** da API (`appInfo.apiVersion = VK_API_VERSION_1_0`, em `src/VulkanInstance.cpp`). Além do núcleo 1.0, ela liga as extensões de instância que a GLFW pede para a superfície, a `VK_KHR_get_physical_device_properties2` (necessária para o MoltenVK no macOS; no Windows ela também é pedida e o driver testado a suporta) e a extensão de dispositivo `VK_KHR_swapchain`. A versão 1.4.357 é a dos headers e do loader usados na compilação.

As bibliotecas **glm** e **tinygltf** são baixadas automaticamente pelo CMake na primeira configuração, com versão e hash fixos (precisa de internet nessa primeira vez).

Em outros sistemas, o lugar do vcpkg pode ser ocupado pelo **Vulkan SDK** (que traz headers, loader, validation layers e `glslc`) e pela GLFW do sistema (por exemplo, `brew install glfw` no macOS). O código tem o suporte ao macOS (MoltenVK) ligado por `#ifdef __APPLE__` em `src/VulkanInstance.cpp`, mas esta versão foi testada só no Windows.

---

## 3. Como compilar

### Windows (ambiente testado)

Os comandos abaixo são rodados dentro desta pasta (`código/`). A pasta `deps/` guarda o vcpkg e não vai no ZIP, porque é grande e é gerada por estes comandos.

```bat
git clone https://github.com/microsoft/vcpkg deps\vcpkg
deps\vcpkg\bootstrap-vcpkg.bat
deps\vcpkg\vcpkg install glfw3 vulkan-headers vulkan-loader vulkan-validationlayers shaderc --triplet x64-windows
build_windows.bat
```

O script `build_windows.bat` prepara o compilador do Visual Studio 2022 Build Tools, configura o CMake com o gerador Ninja em modo Release e com o toolchain do vcpkg, e compila. O executável fica em `build\UVMapping.exe`. O script supõe o Visual Studio 2022 Build Tools no caminho padrão; com outra edição (Community, Professional) ou outro caminho, troque a linha `set VS=` no começo dele.

O vcpkg usado no teste estava no commit `4cb050be` (30/09/2026). Um vcpkg mais novo pode instalar versões mais novas das bibliotecas; para repetir as versões da seção 2, rode `git -C deps\vcpkg checkout 4cb050be` antes do `bootstrap-vcpkg.bat`.

### Outros sistemas (Vulkan SDK)

Com o Vulkan SDK e a GLFW instalados:

```bash
cmake -S . -B build
cmake --build build -j
```

Para usar o vcpkg em vez do SDK, acrescente `-DCMAKE_TOOLCHAIN_FILE=<pasta do vcpkg>/scripts/buildsystems/vcpkg.cmake` ao primeiro comando. Com o gerador padrão do Visual Studio (sem `-G Ninja`), o executável fica em `build\Release\` ou `build\Debug\`.

### Como os shaders são compilados para SPIR-V

O `CMakeLists.txt` cria um passo de build que roda o `glslc` em cada shader sempre que o `.vert` ou o `.frag` mudam:

```bash
glslc shaders/uv_mapping.vert -o shaders/uv_mapping.vert.spv
glslc shaders/uv_mapping.frag -o shaders/uv_mapping.frag.spv
```

Os `.spv` gerados ficam em `shaders/`, onde a aplicação os procura. Se o `glslc` não for encontrado, o build avisa e a aplicação usa os `.spv` que já vêm no projeto. Os mesmos comandos acima servem para compilar à mão.

---

## 4. Como executar

A aplicação deve ser executada **a partir desta pasta**, porque os shaders e o modelo são carregados por caminho relativo (`shaders/...` e `assets/...`):

```bat
build\UVMapping.exe
```

No macOS ou no Linux: `./build/UVMapping`.

Por padrão é carregado `assets/models/Duck.glb`. Outro modelo `.glb` ou `.gltf` pode ser passado como argumento (`build\UVMapping.exe caminho\para\modelo.glb`). Modelos sem textura recebem um padrão xadrez, e modelos sem UV recebem UV zero e um aviso.

O terminal mostra cada etapa da inicialização ("Instance criada.", "GPU selecionada: ...", "Graphics Pipeline criada." etc.). Um registro completo está em `evidencias/execucao_log.txt`.

**Camada de validação.** Ela é ligada automaticamente quando está instalada; sem ela, a aplicação avisa no terminal e roda normalmente. Com o vcpkg, o loader do Vulkan precisa saber onde está a camada: antes de executar, defina `VK_LAYER_PATH` com a pasta `deps\vcpkg\installed\x64-windows\bin`. Se esse caminho tiver acentos (como em `código`), o loader não o encontra: nesse caso, copie as DLLs e o arquivo `VkLayer_khronos_validation.json` dessa pasta para uma pasta sem acentos e aponte `VK_LAYER_PATH` para ela.

---

## 5. Estrutura do projeto e principais arquivos

```text
código/
├── CMakeLists.txt          configuração do projeto e compilação dos shaders
├── build_windows.bat       script de compilação no Windows
├── README.md               este documento
├── src/
│   ├── main.cpp            início: escolhe o modelo e trata erros
│   ├── ModelLoader.hpp/.cpp leitura do modelo glTF e da textura
│   ├── VulkanApp.hpp       declaração da classe e do Uniform Buffer
│   ├── VulkanApp.cpp       janela, teclado, ordem de inicialização e loop
│   ├── VulkanInstance.cpp  instância, extensões, validação e superfície
│   ├── VulkanDevice.cpp    escolha da GPU, filas e dispositivo lógico
│   ├── VulkanSwapchain.cpp swapchain, image views, profundidade, framebuffers
│   ├── VulkanShaders.cpp   leitura dos .spv e criação dos shader modules
│   ├── VulkanPipeline.cpp  render pass e pipeline gráfico
│   ├── VulkanDescriptors.cpp descriptor set (Uniform Buffer e textura)
│   ├── VulkanBuffers.cpp   memória, vertex/index/uniform buffers, cópias
│   ├── VulkanTexture.cpp   imagens, barreiras de layout, textura e sampler
│   ├── VulkanRenderer.cpp  command buffers, sincronização e drawFrame
│   └── VulkanCleanup.cpp   destruição de todos os objetos Vulkan
├── shaders/
│   ├── uv_mapping.vert / .frag      fontes GLSL dos shaders (Capítulo 5 do PDF)
│   └── uv_mapping.vert.spv / .frag.spv  SPIR-V gerados pelo glslc
├── assets/models/          Duck.glb e a sua licença
└── evidencias/             capturas e registro da execução
```

A classe `VulkanApp` é uma só, mas as suas funções estão divididas em arquivos por assunto, seguindo a lista de responsabilidades do enunciado. O `VulkanApp.hpp` agrupa as declarações e diz em qual arquivo cada grupo está.

Onde está cada item obrigatório:

| Item | Função (arquivo) |
|------|------------------|
| Instância, extensões e camada de validação | `createInstance`, `checkValidationLayerSupport` (`VulkanInstance.cpp`) |
| Superfície de apresentação | `createSurface` (`VulkanInstance.cpp`) |
| GPU, filas e dispositivo lógico | `pickPhysicalDevice`, `findQueueFamilies`, `createLogicalDevice` (`VulkanDevice.cpp`) |
| Swapchain e image views | `createSwapChain`, `createImageViews` (`VulkanSwapchain.cpp`) |
| Render pass e framebuffers | `createRenderPass` (`VulkanPipeline.cpp`), `createFramebuffers` (`VulkanSwapchain.cpp`) |
| Pipeline gráfico e shaders | `createGraphicsPipeline` (`VulkanPipeline.cpp`), `readFile`, `createShaderModule` (`VulkanShaders.cpp`) |
| Command pool e command buffers | `createCommandPool`, `createCommandBuffers` (`VulkanRenderer.cpp`) |
| Semáforos e fences | `createSyncObjects` (`VulkanRenderer.cpp`) |
| Loop principal | `mainLoop` (`VulkanApp.cpp`) |
| Aquisição, submissão e apresentação | `drawFrame` (`VulkanRenderer.cpp`) |
| Liberação dos recursos | `cleanup` (`VulkanCleanup.cpp`), chamada também se a inicialização falhar (`run`, em `VulkanApp.cpp`) |

---

## 6. Integração dos shaders

Os arquivos `shaders/uv_mapping.vert` e `shaders/uv_mapping.frag` são os shaders descritos no Capítulo 5 do PDF. Eles têm, instrução por instrução, o mesmo código de `docs/pseudocodigo/`, que é a versão comentada usada no PDF; só os comentários são diferentes.

1. **Compilação:** o CMake roda o `glslc` e gera `shaders/uv_mapping.vert.spv` e `shaders/uv_mapping.frag.spv` (seção 3).
2. **Carregamento:** em `createGraphicsPipeline` (`VulkanPipeline.cpp`), a aplicação lê os dois `.spv` com `readFile` e cria um `VkShaderModule` para cada um com `createShaderModule` (`VulkanShaders.cpp`).
3. **Estágios:** o primeiro módulo vai para o estágio `VK_SHADER_STAGE_VERTEX_BIT` e o segundo para o `VK_SHADER_STAGE_FRAGMENT_BIT`, os dois com a função de entrada `main`. Depois que o pipeline é criado, os módulos são destruídos.
4. **Entradas do vertex shader:** `location` 0, 1 e 2 (posição, normal e UV) correspondem aos campos `position`, `normal` e `uv` da struct `Vertex` (offsets 0, 12 e 24 bytes, binding 0 do vertex buffer).
5. **Uniform Buffer:** `set 0, binding 0` (bloco `Parameters`) corresponde ao `UniformBufferObject` de `VulkanApp.hpp` e é visto pelos dois estágios. Os `static_assert` desse arquivo garantem os mesmos offsets do layout std140 (seção 7).
6. **Textura:** `set 0, binding 1` (`sampler2D baseTexture`) corresponde à textura do modelo com o sampler (`COMBINED_IMAGE_SAMPLER`) e é visto só pelo fragment shader.
7. **Push constants:** não são usadas. Os command buffers são gravados uma vez só, e um valor de push constant ficaria gravado dentro deles; por isso o tempo e o modo vão no Uniform Buffer (explicação completa no Capítulo 4 do PDF).

A prova de que os shaders são carregados e usados está no registro da execução ("Graphics Pipeline criada.") e nas capturas: cada modo da seção 1 só existe por causa do fragment shader.

---

## 7. Uniform Buffer

A cada quadro a aplicação envia aos shaders, por um **Uniform Buffer Object (UBO)** (um por imagem da swapchain, mapeado o tempo todo):

| Campo | Offset (std140) | Uso |
|-------|-----------------|-----|
| `model` | 0 | Rotação do modelo em torno do eixo Y |
| `view` | 64 | Posição e direção da câmera |
| `projection` | 128 | Projeção em perspectiva (com o eixo Y invertido para o Vulkan) |
| `time` | 192 | Tempo desde o início, em segundos: anima o UV e o padrão procedural |
| `mode` | 196 | Modo de visualização escolhido pelo teclado |

O sampler da textura usa o modo de endereçamento **REPEAT**, que faz a textura se repetir fora do intervalo [0, 1] e permite deslocar o UV continuamente.

---

## 8. Como um quadro é desenhado

```text
Duck.glb ──► ModelLoader (tinygltf)
                │
                ▼
   vértices, índices, textura ──(staging buffers)──► memória da GPU
                                                        │
drawFrame (VulkanRenderer.cpp)                          │
  1. espera a fence do quadro                           │
  2. pega uma imagem da swapchain (imageAvailable)      │
  3. grava M, V, P, time e mode no Uniform Buffer       │
  4. envia o command buffer daquela imagem ─────────────┤
        Render Pass (cor + profundidade)                │
        Graphics Pipeline                               │
          ├── Vertex Shader   (MVP, repassa UV e normal)│
          └── Fragment Shader (UV ──► textura / procedural)
  5. apresenta a imagem na janela (renderFinished)
```

Até 2 quadros podem estar em preparo ao mesmo tempo (`MAX_FRAMES_IN_FLIGHT`); os comentários de `drawFrame` explicam cada passo da sincronização.

---

## 9. Funcionalidades que já funcionam

- inicialização completa do Vulkan (instância, superfície, GPU, filas, dispositivo, swapchain, render pass, pipeline, framebuffers, comandos e sincronização);
- camada de validação opcional (sem erros nem avisos na execução testada);
- carregamento de modelos glTF (`.glb` e `.gltf`) com UV, normais, índices e textura;
- mapeamento UV da textura sobre o modelo 3D;
- UV dinâmico (UV animado no tempo) e padrão procedural no espaço UV;
- visualização de depuração da parametrização UV;
- câmera em perspectiva e rotação do modelo;
- teste de profundidade e descarte de faces traseiras;
- vertex e index buffers na memória local da GPU, enviados por staging buffer;
- textura SRGB com sampler e filtragem anisotrópica (quando a GPU suporta);
- troca de modos pelo teclado e pausa da rotação;
- compilação automática dos shaders pelo CMake;
- liberação de todos os recursos ao fechar; se a inicialização falhar, `cleanup()` também é chamado e libera os objetos guardados na classe (testado com um `.spv` ausente e a validação ligada, sem avisos);
- pausa da renderização com a janela minimizada.

## 10. Funcionalidades que ainda serão desenvolvidas

- geração de mipmaps para a textura (hoje ela tem um nível só);
- recriação da swapchain para permitir redimensionar a janela;
- tempo enviado de forma periódica, para não perder precisão em execuções muito longas;
- parte dos parâmetros em push constants, com os command buffers regravados a cada quadro;
- troca de modelo durante a execução e câmera controlada pelo mouse.

---

## 11. Evidências

A pasta `evidencias/` contém registros da execução:

```text
evidencias/
├── execucao_log.txt                saída do terminal de uma execução completa
├── modo1_textura_original.png      mapeamento UV da textura original
├── modo2_uv_dinamica.png           UV animado: o olho saiu da cabeça e foi para o corpo
├── modo3_procedural.png            padrão procedural sobre as UVs do modelo
├── modo4_textura_procedural.png    textura com UV dinâmico + padrão procedural
└── modo5_grade_uv.png              grade de depuração da parametrização UV
```

O `execucao_log.txt` e as cinco capturas vêm da mesma execução desta versão do código, compilada com o `build_windows.bat` e com a camada de validação ligada (linha "Validation layer ativada"). Cada captura mostra a janela inteira, com o modo atual na barra de título, e as linhas "Modo: ..." do registro marcam as trocas de modo. Logo no início, a rotação foi pausada (tecla Espaço) para os cinco modos aparecerem na mesma pose. A aplicação passou por todas as etapas, desenhou até a janela ser fechada e liberou os recursos sem nenhuma mensagem de erro ou aviso da validação.

A sequência do modo 2 ao longo do tempo (t ≈ 1, 3 e 5 s) está em `../docs/imagens/marcadas/app_uv_dinamico.png` (Figura 7.6 do PDF).

---

## 12. Modelo e licenças

O modelo **Duck** foi obtido do repositório oficial de modelos de exemplo do Khronos Group, [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Duck).

Copyright 2005 Sony Computer Entertainment Inc. Licenciado sob a SCEA Shared Source License 1.0, cuja cópia acompanha o modelo em `assets/models/LICENSE-Duck-SCEA.txt`. O arquivo foi utilizado sem modificações.

Bibliotecas baixadas pelo CMake:

- [glm](https://github.com/g-truc/glm) 1.0.1, licença MIT
- [tinygltf](https://github.com/syoyo/tinygltf) v2.9.3, licença MIT (inclui stb_image e nlohmann/json)
