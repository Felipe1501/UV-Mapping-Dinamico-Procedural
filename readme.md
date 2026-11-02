PROJETO OVERLEAF - UV MAPPING DINAMICO E PROCEDURAL

## PDF do trabalho

[Visualizar PDF](./docs/UV_Mapping_Dinamico_Procedural.pdf)

Este projeto contém os arquivos em LaTeX utilizados para as entregas da
disciplina Computação Gráfica e Processamento de Imagens, com o tema
UV Mapping Dinâmico e Procedural.

O projeto inclui a documentação teórica da técnica, aplicações práticas,
utilização em jogos e a arquitetura do pipeline Vulkan relacionada ao tema.


PASTAS:

- capitulos/: textos e seções do trabalho
- imagens/: capturas e imagens utilizadas no trabalho
- artigos/: PDFs das publicações utilizadas como referência
- resumos/: resumos das publicações utilizadas
- diagrama/: arquivos utilizados para a elaboração do diagrama do pipeline Vulkan


ARQUIVOS PRINCIPAIS:

- main.tex: arquivo principal do projeto LaTeX
- referencias.bib: referências bibliográficas, quando utilizadas
- readme.txt: informações e organização do projeto


ENTREGA 1:

A primeira entrega apresenta a revisão bibliográfica sobre UV Mapping
Dinâmico e Procedural, incluindo:

- fundamentação teórica;
- equações e funcionamento matemático;
- relação com Vulkan;
- aplicações práticas;
- utilização da técnica em jogos;
- capturas de gameplay;
- vídeos de demonstração;
- resumo das publicações utilizadas;
- conclusão e referências.


PUBLICAÇÕES PRINCIPAIS:

1. Heckbert (1986) - Survey of Texture Mapping
   https://doi.org/10.1109/MCG.1986.276672

2. Dong et al. (2020) - Survey of Procedural Methods for Two-Dimensional Texture Generation
   https://doi.org/10.3390/s20041135

3. Smelik et al. (2014) - A Survey on Procedural Modelling for Virtual Worlds
   https://doi.org/10.1111/cgf.12276


ENTREGA 2:

A segunda entrega complementa o trabalho com a arquitetura do pipeline
Vulkan relacionada ao UV Mapping Dinâmico e Procedural.

O diagrama apresenta:

- fluxo de dados;
- aplicação/CPU;
- Vertex Buffer;
- Index Buffer;
- Uniform Buffer (UBO);
- Descriptor Set e Bindings;
- Texture Image;
- Image View;
- Sampler;
- Vertex Input;
- Vertex Shader;
- Rasterização;
- interpolação das coordenadas UV;
- Fragment Shader;
- Render Pass;
- Subpass;
- Color Attachment;
- Depth Attachment;
- Framebuffer;
- dependências de execução;
- barreiras de memória;
- transições de Image Layout;
- operações específicas de UV Mapping Dinâmico;
- operações específicas de Mapeamento Procedural.

A explicação escrita descreve a função de cada etapa, os dados processados,
os recursos compartilhados, os shaders utilizados, os buffers, os
Descriptor Sets e Bindings, as sincronizações e as transições de imagens.

A arquitetura também apresenta a relação entre os recursos do Vulkan e a
execução das técnicas de UV Mapping Dinâmico e Procedural.

ENTREGA 3:

A terceira entrega apresenta o pseudocódigo dos shaders necessários para
representar a implementação do UV Mapping Dinâmico e Procedural em uma
arquitetura gráfica baseada em Vulkan.

Foram desenvolvidos dois shaders em sintaxe semelhante à linguagem GLSL:

- docs/pseudocodigo/uv_mapping.vert
  Pseudocódigo do Vertex Shader.

- docs/pseudocodigo/uv_mapping.frag
  Pseudocódigo do Fragment Shader.

Os shaders finais, compiláveis, estão em código/shaders/.

O Vertex Shader é responsável pelo processamento dos vértices, transformação
das posições, preparação das coordenadas UV e aplicação do deslocamento
dependente do tempo.

O Fragment Shader é responsável pela utilização das coordenadas UV
interpoladas, amostragem da textura, cálculo procedural, normalização,
iluminação e produção da cor final.

A documentação da Entrega 3 apresenta:

- objetivo de cada shader;
- fluxo de dados entre os shaders;
- entradas e saídas;
- Uniform Buffer;
- textura e Sampler;
- transformações de posição;
- transformação e deslocamento das coordenadas UV;
- amostragem da textura;
- filtragem e mipmapping;
- função procedural;
- normalização de vetores;
- cálculos de iluminação;
- condições e tratamento de limites;
- cálculo da cor final;
- resultado esperado;
- relação dos shaders com o UV Mapping Dinâmico e Procedural.
