// Descriptor set: diz aos shaders onde estão o Uniform Buffer
// (binding 0) e a textura com o sampler (binding 1).

#include "VulkanApp.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

// Descreve o "formato" do descriptor set: que recursos os shaders recebem,
// em qual binding e em quais estágios. Vem antes de createGraphicsPipeline,
// porque o pipeline layout é criado a partir dele.
void VulkanApp::createDescriptorSetLayout()
{
    // Binding 0: parâmetros (matrizes, tempo e modo).
    // É o bloco "layout(set = 0, binding = 0) uniform Parameters" que os
    // dois shaders declaram.
    VkDescriptorSetLayoutBinding uboBinding{};

    uboBinding.binding = 0;

    uboBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Um descritor só; seria mais de um se o shader declarasse um array.
    uboBinding.descriptorCount = 1;

    // Os dois estágios leem o bloco: o vertex shader usa as matrizes e o
    // fragment shader usa time e mode.
    uboBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT |
        VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: textura do modelo, amostrada pelas coordenadas UV.
    // É o "layout(set = 0, binding = 1) uniform sampler2D baseTexture" do
    // uv_mapping.frag.
    VkDescriptorSetLayoutBinding samplerBinding{};

    samplerBinding.binding = 1;

    // COMBINED_IMAGE_SAMPLER junta imagem e sampler em um descritor só,
    // que é o que o sampler2D do GLSL espera.
    samplerBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    samplerBinding.descriptorCount = 1;

    // Só o fragment shader lê a textura.
    samplerBinding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT;

    const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
        uboBinding,
        samplerBinding
    };

    // O número do set não aparece aqui: este layout vira o set 0 por ser o
    // primeiro (e único) de pSetLayouts, em createGraphicsPipeline.
    VkDescriptorSetLayoutCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    createInfo.bindingCount =
        static_cast<uint32_t>(bindings.size());

    createInfo.pBindings =
        bindings.data();

    if (vkCreateDescriptorSetLayout(
            device,
            &createInfo,
            nullptr,
            &descriptorSetLayout
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Descriptor Set Layout!"
        );
    }

    std::cout
        << "Descriptor Set Layout criado."
        << std::endl;
}

// Cria a pool de onde os descriptor sets são alocados. Ela precisa saber
// antes quantos sets e quantos descritores de cada tipo vai fornecer.
// Vem logo antes de createDescriptorSets, que aloca os sets dela.
void VulkanApp::createDescriptorPool()
{
    // Um set por imagem da swapchain, cada um com 1 Uniform Buffer e
    // 1 textura com sampler: swapChainImages.size() descritores de cada tipo.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    poolSizes[0].descriptorCount =
        static_cast<uint32_t>(
            swapChainImages.size()
        );

    poolSizes[1].type =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    poolSizes[1].descriptorCount =
        static_cast<uint32_t>(
            swapChainImages.size()
        );

    VkDescriptorPoolCreateInfo info{};

    info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    info.poolSizeCount =
        static_cast<uint32_t>(poolSizes.size());

    info.pPoolSizes =
        poolSizes.data();

    info.maxSets =
        static_cast<uint32_t>(
            swapChainImages.size()
        );

    // info.flags fica em 0 (sem FREE_DESCRIPTOR_SET_BIT): os sets não são
    // liberados um a um, e sim todos juntos quando cleanup() destrói a pool.
    if (vkCreateDescriptorPool(
            device,
            &info,
            nullptr,
            &descriptorPool
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Descriptor Pool!"
        );
    }

    std::cout
        << "Descriptor Pool criada."
        << std::endl;
}

// Aloca um descriptor set por imagem da swapchain e aponta cada um para os
// recursos reais. Vem depois dos Uniform Buffers e da textura, e antes de
// createCommandBuffers, que liga o set i no command buffer i.
void VulkanApp::createDescriptorSets()
{
    // vkAllocateDescriptorSets pede um layout para cada set. Todos usam o
    // mesmo, então o vetor repete descriptorSetLayout.
    std::vector<VkDescriptorSetLayout> layouts(
        swapChainImages.size(),
        descriptorSetLayout
    );

    VkDescriptorSetAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    allocInfo.descriptorPool =
        descriptorPool;

    allocInfo.descriptorSetCount =
        static_cast<uint32_t>(
            layouts.size()
        );

    allocInfo.pSetLayouts =
        layouts.data();

    descriptorSets.resize(
        swapChainImages.size()
    );

    // Alocar só reserva os sets na pool; o conteúdo é escrito no laço
    // abaixo, com vkUpdateDescriptorSets.
    if (vkAllocateDescriptorSets(
            device,
            &allocInfo,
            descriptorSets.data()
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao alocar Descriptor Sets!"
        );
    }

    for (size_t i = 0;
         i < swapChainImages.size();
         ++i)
    {
        // Binding 0: o Uniform Buffer da imagem i, inteiro. É por isso que
        // há um set por imagem: cada um aponta para um buffer diferente.
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer =
            uniformBuffers[i];

        bufferInfo.offset = 0;

        bufferInfo.range =
            sizeof(UniformBufferObject);

        // Binding 1: a textura (image view + sampler), igual em todos os sets.
        VkDescriptorImageInfo imageInfo{};

        // Layout em que a imagem estará quando o shader ler: é o layout
        // final deixado por createTextureImage.
        imageInfo.imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView =
            textureImageView;

        imageInfo.sampler =
            textureSampler;

        // Cada escrita liga um binding do set i ao seu recurso. dstBinding
        // e descriptorType precisam bater com o layout e com os shaders.
        std::array<VkWriteDescriptorSet, 2> writes{};

        writes[0].sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        writes[0].dstSet =
            descriptorSets[i];

        writes[0].dstBinding =
            0;

        writes[0].dstArrayElement =
            0;

        writes[0].descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        writes[0].descriptorCount =
            1;

        writes[0].pBufferInfo =
            &bufferInfo;

        writes[1].sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        writes[1].dstSet =
            descriptorSets[i];

        writes[1].dstBinding =
            1;

        writes[1].dstArrayElement =
            0;

        writes[1].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        writes[1].descriptorCount =
            1;

        writes[1].pImageInfo =
            &imageInfo;

        // Isso é feito uma vez só: a cada quadro muda o conteúdo do Uniform
        // Buffer, e não o buffer para o qual o set aponta. Os dois últimos
        // argumentos (cópias entre sets) não são usados.
        vkUpdateDescriptorSets(
            device,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
    }

    std::cout
        << "Descriptor Sets criados."
        << std::endl;
}
