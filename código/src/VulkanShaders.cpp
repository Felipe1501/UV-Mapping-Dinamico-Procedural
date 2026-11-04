// Carregamento dos shaders: lê os arquivos .spv (gerados pelo glslc a
// partir de shaders/uv_mapping.vert e .frag) e cria os shader modules.

#include "VulkanApp.hpp"

#include <fstream>
#include <stdexcept>

// Lê um arquivo binário inteiro para a memória. É usada por
// createGraphicsPipeline para carregar os shaders compilados (.spv).
std::vector<char> VulkanApp::readFile(const std::string& filename)
{
    // binary: lê os bytes como estão, sem a conversão de fim de linha do
    // modo texto (o SPIR-V é binário). ate: abre já no fim do arquivo,
    // então tellg() devolve o tamanho.
    std::ifstream file(
        filename,
        std::ios::ate | std::ios::binary
    );

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Nao foi possivel abrir: " + filename
        );
    }

    const size_t fileSize =
        static_cast<size_t>(file.tellg());

    std::vector<char> buffer(fileSize);

    // Volta para o início e lê o arquivo inteiro de uma vez.
    file.seekg(0);

    file.read(
        buffer.data(),
        static_cast<std::streamsize>(fileSize)
    );

    file.close();

    return buffer;
}

// Cria um shader module com o código SPIR-V lido por readFile. É chamada
// por createGraphicsPipeline, uma vez para cada shader.
VkShaderModule VulkanApp::createShaderModule(
    const std::vector<char>& code
)
{
    // O Vulkan não recebe GLSL: o shader chega em SPIR-V, um código
    // intermediário. O shader module só guarda esse código; a compilação
    // para a GPU acontece na criação do pipeline.
    VkShaderModuleCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    // codeSize é em bytes, mas pCode aponta para uint32_t: o SPIR-V é uma
    // sequência de palavras de 32 bits. O std::vector aloca a memória com
    // alinhamento suficiente para esse cast.
    createInfo.codeSize =
        code.size();

    createInfo.pCode =
        reinterpret_cast<const uint32_t*>(
            code.data()
        );

    VkShaderModule shaderModule =
        VK_NULL_HANDLE;

    if (vkCreateShaderModule(
            device,
            &createInfo,
            nullptr,
            &shaderModule
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Shader Module!"
        );
    }

    return shaderModule;
}
