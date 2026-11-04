# UV Mapping Dinâmico e Procedural

Trabalho da disciplina Computação Gráfica e Processamento de Imagens.

## PDF do trabalho

[Visualizar PDF](./docs/UV_Mapping_Dinamico_Procedural.pdf)

O PDF reúne todo o trabalho: revisão bibliográfica, arquitetura do pipeline
Vulkan, shaders e código. O apêndice "Checklist dos requisitos" aponta onde
cada item pedido está no documento.

## Pastas

- `docs/`: documento em LaTeX
  - `main.tex`: arquivo principal
  - `capitulos/`: um arquivo por capítulo
  - `referencias.bib`: referências (biblatex, estilo ABNT)
  - `imagens/`: capturas originais e capturas marcadas (`imagens/marcadas/`)
  - `artigos/`: PDFs das três publicações usadas
  - `resumos/`: resumo de cada publicação
  - `diagrama/`: diagrama do pipeline Vulkan (`pipeline_vulkan.dot` e `.pdf`)
  - `pseudocodigo/`: pseudocódigo dos shaders
  - `videos.txt`: links das lives de gameplay usadas nas capturas
- `código/`: aplicação Vulkan em C++, com instruções no `código/README.md`

## Publicações principais

1. Heckbert (1986) - Survey of Texture Mapping
   https://doi.org/10.1109/MCG.1986.276672
2. Dong et al. (2020) - Survey of Procedural Methods for Two-Dimensional Texture Generation
   https://doi.org/10.3390/s20041135
3. Smelik et al. (2014) - A Survey on Procedural Modelling for Virtual Worlds
   https://doi.org/10.1111/cgf.12276

## Conteúdo

**Revisão bibliográfica** (Capítulos 1 a 3 e 6 a 8): teoria, quando
usar, limitações e custo; equações; aplicações reais; análise de dois jogos com
capturas marcadas; resumos das publicações.

**Arquitetura do pipeline Vulkan** (Capítulo 4): diagrama em
`docs/diagrama/`, fluxo de dados, descriptor sets, buffers, render pass,
barreiras, sincronização e justificativa das decisões.

**Shaders** (Capítulo 5):

- `docs/pseudocodigo/uv_mapping.vert`: vertex shader. Calcula a posição do
  vértice na tela e repassa o UV e a normal (não altera o UV).
- `docs/pseudocodigo/uv_mapping.frag`: fragment shader. Calcula a cor de cada
  pixel em cinco modos: textura, UV dinâmico, procedural, mistura e grade UV.

Os dois arquivos estão em sintaxe GLSL e comentados trecho a trecho
(Entra / Faz / Sai / Na tela). O código deles é o mesmo dos shaders compilados
em `código/shaders/`; só os comentários são diferentes.

**Código** (`código/`): aplicação em C++ com Vulkan que
carrega o modelo `Duck.glb` e aplica os shaders. As teclas 1 a 5 trocam o modo.
