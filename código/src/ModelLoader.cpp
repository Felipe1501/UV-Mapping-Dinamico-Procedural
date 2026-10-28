#include "ModelLoader.hpp"

#define NOMINMAX
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

// Leitura dos accessors do glTF, no mesmo esquema do GltfParser da
// Eruption Engine: cada atributo aponta para um trecho de um buffer
// (bufferView + offset), com um passo (byteStride) opcional entre elementos.

static const unsigned char* accessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    size_t elementSize,
    size_t& stride
)
{
    if (accessor.bufferView < 0)
    {
        throw std::runtime_error(
            "Accessor sem bufferView nao e suportado!"
        );
    }

    const tinygltf::BufferView& view =
        model.bufferViews[accessor.bufferView];

    const tinygltf::Buffer& buffer =
        model.buffers[view.buffer];

    stride =
        view.byteStride ? view.byteStride : elementSize;

    return buffer.data.data() +
           view.byteOffset +
           accessor.byteOffset;
}

static std::vector<glm::vec3> readVec3Accessor(
    const tinygltf::Model& model,
    int accessorIndex
)
{
    std::vector<glm::vec3> out;

    const tinygltf::Accessor& accessor =
        model.accessors[accessorIndex];

    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.type != TINYGLTF_TYPE_VEC3)
    {
        throw std::runtime_error(
            "Atributo vec3 precisa ser float!"
        );
    }

    size_t stride = 0;

    const unsigned char* base =
        accessorData(model, accessor, sizeof(glm::vec3), stride);

    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        std::memcpy(
            &out[i],
            base + i * stride,
            sizeof(glm::vec3)
        );
    }

    return out;
}

static std::vector<glm::vec2> readVec2Accessor(
    const tinygltf::Model& model,
    int accessorIndex
)
{
    std::vector<glm::vec2> out;

    const tinygltf::Accessor& accessor =
        model.accessors[accessorIndex];

    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.type != TINYGLTF_TYPE_VEC2)
    {
        throw std::runtime_error(
            "Coordenadas UV precisam ser float!"
        );
    }

    size_t stride = 0;

    const unsigned char* base =
        accessorData(model, accessor, sizeof(glm::vec2), stride);

    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        std::memcpy(
            &out[i],
            base + i * stride,
            sizeof(glm::vec2)
        );
    }

    return out;
}

static std::vector<uint32_t> readIndexAccessor(
    const tinygltf::Model& model,
    int accessorIndex
)
{
    std::vector<uint32_t> out;

    const tinygltf::Accessor& accessor =
        model.accessors[accessorIndex];

    size_t elementSize = 0;

    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            elementSize = sizeof(uint8_t);
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            elementSize = sizeof(uint16_t);
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            elementSize = sizeof(uint32_t);
            break;

        default:
            throw std::runtime_error(
                "Tipo de indice nao suportado!"
            );
    }

    size_t stride = 0;

    const unsigned char* base =
        accessorData(model, accessor, elementSize, stride);

    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        const unsigned char* src = base + i * stride;

        if (elementSize == sizeof(uint8_t))
        {
            out[i] = *src;
        }
        else if (elementSize == sizeof(uint16_t))
        {
            uint16_t value = 0;
            std::memcpy(&value, src, sizeof(value));
            out[i] = value;
        }
        else
        {
            std::memcpy(&out[i], src, sizeof(uint32_t));
        }
    }

    return out;
}

// Matriz local do nó: ou vem pronta (matrix), ou é montada a partir de
// translação, rotação (quaternion x, y, z, w) e escala.
static glm::mat4 nodeLocalMatrix(
    const tinygltf::Node& node
)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 matrix(1.0f);

        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                matrix[column][row] =
                    static_cast<float>(node.matrix[column * 4 + row]);

        return matrix;
    }

    glm::mat4 translation(1.0f);
    glm::mat4 rotation(1.0f);
    glm::mat4 scale(1.0f);

    if (node.translation.size() == 3)
    {
        translation = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            )
        );
    }

    if (node.rotation.size() == 4)
    {
        const glm::quat q(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );

        rotation = glm::mat4_cast(q);
    }

    if (node.scale.size() == 3)
    {
        scale = glm::scale(
            glm::mat4(1.0f),
            glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            )
        );
    }

    return translation * rotation * scale;
}

// Converte a imagem do glTF (já decodificada pelo stb_image dentro do
// tinygltf) para RGBA com 8 bits por canal.
static bool extractTexture(
    const tinygltf::Model& model,
    int materialIndex,
    TextureData& texture
)
{
    if (materialIndex < 0)
        return false;

    const tinygltf::Material& material =
        model.materials[materialIndex];

    const int textureIndex =
        material.pbrMetallicRoughness.baseColorTexture.index;

    if (textureIndex < 0)
        return false;

    const int imageIndex =
        model.textures[textureIndex].source;

    if (imageIndex < 0)
        return false;

    const tinygltf::Image& image =
        model.images[imageIndex];

    if (image.bits != 8 ||
        image.component < 1 ||
        image.component > 4 ||
        image.image.empty())
    {
        std::cout
            << "Textura do modelo em formato nao suportado, usando xadrez."
            << std::endl;

        return false;
    }

    texture.width = static_cast<uint32_t>(image.width);
    texture.height = static_cast<uint32_t>(image.height);
    texture.pixels.resize(
        static_cast<size_t>(texture.width) * texture.height * 4
    );

    const size_t pixelCount =
        static_cast<size_t>(texture.width) * texture.height;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        const unsigned char* src =
            &image.image[i * image.component];

        unsigned char* dst =
            &texture.pixels[i * 4];

        if (image.component >= 3)
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }
        else
        {
            dst[0] = src[0];
            dst[1] = src[0];
            dst[2] = src[0];
        }

        dst[3] =
            (image.component == 4) ? src[3] :
            (image.component == 2) ? src[1] : 255;
    }

    return true;
}

// Textura usada quando o modelo não tem uma: um xadrez deixa evidente
// como as coordenadas UV esticam e dobram a imagem sobre a superfície.
static TextureData makeCheckerTexture()
{
    TextureData texture;

    texture.width = 256;
    texture.height = 256;
    texture.pixels.resize(256 * 256 * 4);

    for (uint32_t y = 0; y < texture.height; ++y)
    {
        for (uint32_t x = 0; x < texture.width; ++x)
        {
            const bool light =
                ((x / 32) + (y / 32)) % 2 == 0;

            unsigned char* dst =
                &texture.pixels[(y * texture.width + x) * 4];

            dst[0] = light ? 230 : 40;
            dst[1] = light ? 230 : 40;
            dst[2] = light ? 230 : 40;
            dst[3] = 255;
        }
    }

    return texture;
}

static void appendPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& world,
    MeshData& mesh
)
{
    // Só triângulos (modo 4, ou -1 que é o padrão do glTF).
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES &&
        primitive.mode != -1)
    {
        return;
    }

    const auto itPos = primitive.attributes.find("POSITION");
    const auto itNrm = primitive.attributes.find("NORMAL");
    const auto itUv = primitive.attributes.find("TEXCOORD_0");

    if (itPos == primitive.attributes.end())
        return;

    const std::vector<glm::vec3> positions =
        readVec3Accessor(model, itPos->second);

    std::vector<glm::vec3> normals =
        (itNrm != primitive.attributes.end())
            ? readVec3Accessor(model, itNrm->second)
            : std::vector<glm::vec3>(positions.size(), glm::vec3(0.0f));

    std::vector<glm::vec2> uvs =
        (itUv != primitive.attributes.end())
            ? readVec2Accessor(model, itUv->second)
            : std::vector<glm::vec2>(positions.size(), glm::vec2(0.0f));

    if (itUv == primitive.attributes.end())
    {
        std::cout
            << "Aviso: primitiva sem TEXCOORD_0, UVs zeradas."
            << std::endl;
    }

    normals.resize(positions.size(), glm::vec3(0.0f));
    uvs.resize(positions.size(), glm::vec2(0.0f));

    std::vector<uint32_t> indices;

    if (primitive.indices >= 0)
    {
        indices = readIndexAccessor(model, primitive.indices);
    }
    else
    {
        indices.resize(positions.size());

        for (size_t i = 0; i < indices.size(); ++i)
            indices[i] = static_cast<uint32_t>(i);
    }

    // Normais ausentes: média das normais das faces vizinhas.
    if (itNrm == primitive.attributes.end())
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const glm::vec3& a = positions[indices[i]];
            const glm::vec3& b = positions[indices[i + 1]];
            const glm::vec3& c = positions[indices[i + 2]];

            const glm::vec3 faceNormal =
                glm::cross(b - a, c - a);

            normals[indices[i]] += faceNormal;
            normals[indices[i + 1]] += faceNormal;
            normals[indices[i + 2]] += faceNormal;
        }
    }

    // Normais são transformadas pela inversa transposta, para continuarem
    // perpendiculares à superfície mesmo com escala não uniforme.
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(world)));

    const uint32_t baseVertex =
        static_cast<uint32_t>(mesh.vertices.size());

    for (size_t i = 0; i < positions.size(); ++i)
    {
        const glm::vec3 position =
            glm::vec3(world * glm::vec4(positions[i], 1.0f));

        glm::vec3 normal =
            normalMatrix * normals[i];

        const float length = glm::length(normal);

        normal = (length > 1e-6f)
            ? normal / length
            : glm::vec3(0.0f, 1.0f, 0.0f);

        Vertex vertex{};

        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.position[2] = position.z;

        vertex.normal[0] = normal.x;
        vertex.normal[1] = normal.y;
        vertex.normal[2] = normal.z;

        // glTF e Vulkan usam a mesma convenção de UV (origem no canto
        // superior esquerdo da imagem), então a UV é usada sem inversão.
        vertex.uv[0] = uvs[i].x;
        vertex.uv[1] = uvs[i].y;

        mesh.vertices.push_back(vertex);
    }

    for (uint32_t index : indices)
    {
        if (index >= positions.size())
        {
            throw std::runtime_error(
                "Indice fora do intervalo no modelo!"
            );
        }

        mesh.indices.push_back(baseVertex + index);
    }

    if (!mesh.hasTexture)
    {
        mesh.hasTexture =
            extractTexture(model, primitive.material, mesh.texture);
    }
}

static void appendNode(
    const tinygltf::Model& model,
    int nodeIndex,
    const glm::mat4& parent,
    MeshData& mesh
)
{
    const tinygltf::Node& node =
        model.nodes[nodeIndex];

    const glm::mat4 world =
        parent * nodeLocalMatrix(node);

    if (node.mesh >= 0)
    {
        for (const tinygltf::Primitive& primitive :
             model.meshes[node.mesh].primitives)
        {
            appendPrimitive(model, primitive, world, mesh);
        }
    }

    for (int child : node.children)
        appendNode(model, child, world, mesh);
}

// Centraliza o modelo na origem e escala para o maior lado medir 2,
// assim qualquer modelo aparece inteiro na frente da câmera.
static void normalizeMesh(MeshData& mesh)
{
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());

    for (const Vertex& vertex : mesh.vertices)
    {
        const glm::vec3 position(
            vertex.position[0],
            vertex.position[1],
            vertex.position[2]
        );

        minimum = glm::min(minimum, position);
        maximum = glm::max(maximum, position);
    }

    const glm::vec3 center = (minimum + maximum) * 0.5f;
    const glm::vec3 size = maximum - minimum;

    const float largest =
        std::max(size.x, std::max(size.y, size.z));

    const float scale =
        (largest > 0.0f) ? 2.0f / largest : 1.0f;

    for (Vertex& vertex : mesh.vertices)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            vertex.position[axis] =
                (vertex.position[axis] - center[axis]) * scale;
        }
    }
}

MeshData loadModel(const std::string& path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    std::string error;
    std::string warning;

    const bool binary =
        path.size() >= 4 &&
        path.compare(path.size() - 4, 4, ".glb") == 0;

    const bool loaded = binary
        ? loader.LoadBinaryFromFile(&model, &error, &warning, path)
        : loader.LoadASCIIFromFile(&model, &error, &warning, path);

    if (!warning.empty())
        std::cout << "Aviso glTF: " << warning << std::endl;

    if (!loaded)
    {
        throw std::runtime_error(
            "Falha ao carregar modelo " + path + ": " + error
        );
    }

    MeshData mesh;

    const int sceneIndex =
        (model.defaultScene >= 0) ? model.defaultScene : 0;

    if (!model.scenes.empty())
    {
        for (int nodeIndex : model.scenes[sceneIndex].nodes)
            appendNode(model, nodeIndex, glm::mat4(1.0f), mesh);
    }
    else
    {
        // Sem cena: percorre só os nós raiz (que não são filhos de ninguém).
        std::vector<bool> isChild(model.nodes.size(), false);

        for (const tinygltf::Node& node : model.nodes)
            for (int child : node.children)
                isChild[child] = true;

        for (size_t i = 0; i < model.nodes.size(); ++i)
            if (!isChild[i])
                appendNode(model, static_cast<int>(i), glm::mat4(1.0f), mesh);
    }

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        throw std::runtime_error(
            "Modelo sem triangulos: " + path
        );
    }

    normalizeMesh(mesh);

    if (!mesh.hasTexture)
    {
        std::cout
            << "Modelo sem textura baseColor, usando xadrez."
            << std::endl;

        mesh.texture = makeCheckerTexture();
    }

    std::cout
        << "Modelo carregado: "
        << path
        << " ("
        << mesh.vertices.size()
        << " vertices, "
        << mesh.indices.size() / 3
        << " triangulos, textura "
        << mesh.texture.width
        << "x"
        << mesh.texture.height
        << ")"
        << std::endl;

    return mesh;
}
