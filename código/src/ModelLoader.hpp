#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Vértice da malha: posição, normal e coordenada UV lidas do arquivo glTF.
// As coordenadas UV (TEXCOORD_0) são as que o artista definiu ao "desdobrar"
// o modelo: cada vértice da superfície 3D aponta para um ponto da textura 2D.
struct Vertex
{
    float position[3];
    float normal[3];
    float uv[2];
};

// Imagem RGBA com 8 bits por canal, pronta para virar uma VkImage.
struct TextureData
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<unsigned char> pixels;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Textura base (baseColor) do material do modelo.
    // Se o modelo não tiver textura, é gerado um xadrez.
    TextureData texture;
    bool hasTexture = false;
};

// Carrega um modelo .glb ou .gltf com todas as malhas da cena,
// já transformadas pelos nós, centralizadas na origem e escaladas
// para caber em um cubo de lado 2.
MeshData loadModel(const std::string& path);
