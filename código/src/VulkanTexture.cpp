// Imagens e textura: criação da imagem, transições de layout
// (barreiras), cópia dos pixels e sampler.

#include "VulkanApp.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

// Cria uma image view: diz como ler uma VkImage (tipo, formato, partes).
// Shaders e framebuffers sempre acessam imagens por views. Usada para a
// swapchain, o depth buffer e a textura.
VkImageView VulkanApp::createImageView(
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags
)
{
    VkImageViewCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    createInfo.image =
        image;

    createInfo.viewType =
        VK_IMAGE_VIEW_TYPE_2D;

    createInfo.format =
        format;

    // IDENTITY: cada canal (r, g, b, a) é lido como está, sem trocas.
    createInfo.components.r =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.g =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.b =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.a =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    // aspectMask: COLOR (swapchain e textura) ou DEPTH (depth buffer).
    // A view cobre só o nível de mipmap 0 e a camada 0, que é a imagem
    // toda: essas imagens têm um só nível e uma só camada.
    createInfo.subresourceRange.aspectMask =
        aspectFlags;

    createInfo.subresourceRange.baseMipLevel =
        0;

    createInfo.subresourceRange.levelCount =
        1;

    createInfo.subresourceRange.baseArrayLayer =
        0;

    createInfo.subresourceRange.layerCount =
        1;

    VkImageView imageView =
        VK_NULL_HANDLE;

    if (vkCreateImageView(
            device,
            &createInfo,
            nullptr,
            &imageView
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Image View!"
        );
    }

    return imageView;
}

// Cria uma VkImage 2D e reserva memória da GPU para ela. Usada pela
// textura (createTextureImage) e pelo depth buffer (createDepthResources),
// que mudam só o tamanho, o formato e o uso.
void VulkanApp::createImage(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& memory
)
{
    VkImageCreateInfo imageInfo{};

    imageInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    imageInfo.imageType =
        VK_IMAGE_TYPE_2D;

    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;

    // Um só nível de mipmap (sem versões reduzidas da imagem) e uma só
    // camada. Ou seja, o app não usa mipmaps.
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;

    imageInfo.format =
        format;

    // OPTIMAL: a GPU arruma os texels do jeito mais rápido para ela, num
    // arranjo que a CPU não conhece. Por isso os pixels entram por uma
    // cópia feita na GPU (copyBufferToImage), não por escrita direta.
    imageInfo.tiling =
        VK_IMAGE_TILING_OPTIMAL;

    // UNDEFINED: o conteúdo inicial não importa. O layout de uso vem
    // depois: por barreira na textura e pelo render pass na profundidade.
    imageInfo.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    // Para que a imagem serve: a textura pede TRANSFER_DST | SAMPLED e a
    // profundidade DEPTH_STENCIL_ATTACHMENT. Outro uso seria inválido.
    imageInfo.usage =
        usage;

    // Uma amostra por pixel: sem multisampling (MSAA), como no pipeline.
    imageInfo.samples =
        VK_SAMPLE_COUNT_1_BIT;

    // EXCLUSIVE: só uma família de filas usa a imagem por vez. Basta,
    // pois todo uso destas imagens (cópia e desenho) é na graphicsQueue.
    imageInfo.sharingMode =
        VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(
            device,
            &imageInfo,
            nullptr,
            &image
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Image!"
        );
    }

    // Criar a imagem não reserva memória. Primeiro se pergunta ao driver
    // quanto ela precisa: tamanho, alinhamento e tipos de memória aceitos.
    VkMemoryRequirements requirements{};

    vkGetImageMemoryRequirements(
        device,
        image,
        &requirements
    );

    VkMemoryAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    allocInfo.allocationSize =
        requirements.size;

    // DEVICE_LOCAL: memória da própria GPU, a mais eficiente para ela.
    // Não se pede HOST_VISIBLE: a CPU nunca escreve nesta memória.
    allocInfo.memoryTypeIndex =
        findMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

    if (vkAllocateMemory(
            device,
            &allocInfo,
            nullptr,
            &memory
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao alocar memoria da Image!"
        );
    }

    // Liga a memória à imagem (offset 0: a alocação é só dela).
    if (vkBindImageMemory(
            device,
            image,
            memory,
            0
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao associar memoria a Image!"
        );
    }
}

// Muda o layout de uma imagem de cor com uma barreira. Só trata as duas
// transições da textura (antes e depois da cópia); outra lança erro.
void VulkanApp::transitionImageLayout(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
)
{
    // Comando avulso: endSingleTimeCommands envia à graphicsQueue e
    // espera terminar, então a transição já vale quando a função retorna.
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands();

    // A barreira de imagem faz duas coisas: troca o layout e garante a
    // ordem e a visibilidade dos acessos à memória da imagem.
    VkImageMemoryBarrier barrier{};

    barrier.sType =
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    // Layout = arrumação interna da imagem; cada uso tem o seu ideal.
    // Sair de UNDEFINED deixa a GPU descartar o conteúdo antigo, o que
    // não importa aqui: a cópia vai preencher a imagem inteira.
    barrier.oldLayout =
        oldLayout;

    barrier.newLayout =
        newLayout;

    // IGNORED: não há troca de dono entre famílias de filas. A imagem é
    // EXCLUSIVE e só a graphicsQueue a usa.
    barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.image =
        image;

    // Parte afetada: cor, nível de mipmap 0 e camada 0 (a textura toda).
    // Com aspecto COLOR fixo, esta função não serve para o depth buffer.
    barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Cada caso define dois pares. Estágios: QUANDO esperar (o estágio de
    // destino aguarda o de origem). Access masks: QUAIS escritas de antes
    // (src) ficam visíveis para QUAIS acessos de depois (dst).
    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // Antes da cópia: nada a esperar.
        // src (0, TOP_OF_PIPE): nada usou a imagem antes. dst: a escrita
        // da cópia (TRANSFER) só começa depois da troca de layout.
        barrier.srcAccessMask = 0;

        barrier.dstAccessMask =
            VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage =
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        destinationStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        // Depois da cópia: o fragment shader só lê quando a cópia terminar.
        // src: a escrita da cópia (TRANSFER). dst: a leitura do baseTexture
        // no fragment shader, que já vê os pixels copiados.
        barrier.srcAccessMask =
            VK_ACCESS_TRANSFER_WRITE_BIT;

        barrier.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT;

        sourceStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;

        destinationStage =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::runtime_error(
            "Transicao de layout nao suportada!"
        );
    }

    // Grava a barreira: estágios, flags (0) e as listas de barreiras
    // globais e de buffer (vazias) e de imagem (só a nossa).
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

// Copia os pixels do staging buffer para a imagem da textura. A imagem
// já tem de estar em TRANSFER_DST_OPTIMAL (ver createTextureImage).
void VulkanApp::copyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height
)
{
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands();

    VkBufferImageCopy region{};

    // Os pixels começam no byte 0 do buffer. RowLength e ImageHeight 0
    // = pixels colados, sem sobra entre linhas: o vetor RGBA do
    // ModelLoader tem exatamente largura x altura x 4 bytes.
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    // Destino: a cor do nível de mipmap 0 (o único) e da camada 0.
    region.imageSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;

    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    // A partir do canto (0, 0), copia a imagem inteira (profundidade 1).
    region.imageOffset = {0, 0, 0};

    region.imageExtent = {
        width,
        height,
        1
    };

    // O layout passado tem de ser o atual da imagem, deixado pela
    // barreira anterior. A GPU grava os pixels já no arranjo OPTIMAL.
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}

// Envia a textura do modelo para a GPU: staging buffer, barreira, cópia
// e outra barreira. Chamada em initVulkan depois da command pool (usa
// comandos avulsos) e antes da image view e do sampler.
void VulkanApp::createTextureImage()
{
    // Textura baseColor do glTF ou, na falta dela, o xadrez do
    // ModelLoader. RGBA, 8 bits por canal: largura x altura x 4 bytes.
    const TextureData& texture =
        mesh.texture;

    const VkDeviceSize size =
        static_cast<VkDeviceSize>(texture.pixels.size());

    VkBuffer stagingBuffer =
        VK_NULL_HANDLE;

    VkDeviceMemory stagingMemory =
        VK_NULL_HANDLE;

    // Staging buffer: memória que a CPU consegue mapear (HOST_VISIBLE).
    // HOST_COHERENT dispensa vkFlushMappedMemoryRanges após o memcpy, e
    // TRANSFER_SRC permite usá-lo como origem da cópia.
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    // vkMapMemory dá à CPU um ponteiro para a memória do staging buffer.
    void* mapped = nullptr;

    if (vkMapMemory(
            device,
            stagingMemory,
            0,
            size,
            0,
            &mapped
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao mapear memoria da textura!"
        );
    }

    std::memcpy(
        mapped,
        texture.pixels.data(),
        static_cast<size_t>(size)
    );

    vkUnmapMemory(
        device,
        stagingMemory
    );

    // A textura está em sRGB (como toda imagem de cor comum), então o
    // formato SRGB faz a GPU converter para linear na hora da leitura.
    // TRANSFER_DST: recebe a cópia. SAMPLED: é lida no shader como
    // baseTexture (set 0, binding 1 do uv_mapping.frag).
    createImage(
        texture.width,
        texture.height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        textureImage,
        textureImageMemory
    );

    // 1. Prepara a imagem para receber a cópia.
    transitionImageLayout(
        textureImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    // 2. Copia os pixels do staging buffer para a imagem.
    copyBufferToImage(
        stagingBuffer,
        textureImage,
        texture.width,
        texture.height
    );

    // 3. Deixa a imagem pronta para o shader. É o layout que o descriptor
    //    informa (imageInfo.imageLayout em createDescriptorSets).
    transitionImageLayout(
        textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Já pode liberar o staging buffer: endSingleTimeCommands esperou a
    // GPU terminar a cópia (vkQueueWaitIdle).
    vkDestroyBuffer(
        device,
        stagingBuffer,
        nullptr
    );

    vkFreeMemory(
        device,
        stagingMemory,
        nullptr
    );

    std::cout
        << "Textura criada."
        << std::endl;
}

// Cria a view da textura. Com o sampler, ela forma o binding 1
// (COMBINED_IMAGE_SAMPLER), lido como baseTexture no uv_mapping.frag.
void VulkanApp::createTextureImageView()
{
    // Mesmo formato SRGB da imagem: é o formato da view que faz a
    // leitura no shader converter de sRGB para linear.
    textureImageView =
        createImageView(
            textureImage,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
}

// Cria o sampler: define COMO o shader lê a textura (filtro, repetição,
// anisotropia). Em Vulkan ele é separado da imagem e entra no binding 1
// junto com a view (createDescriptorSets).
void VulkanApp::createTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};

    samplerInfo.sType =
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // LINEAR: mistura os 4 texels mais próximos da UV (bilinear), em vez
    // de pegar só o mais próximo (NEAREST, visual pixelado). mag vale com
    // a textura ampliada na tela; min, com ela reduzida.
    samplerInfo.magFilter =
        VK_FILTER_LINEAR;

    samplerInfo.minFilter =
        VK_FILTER_LINEAR;

    // REPEAT faz a textura se repetir fora do intervalo [0, 1].
    // É isso que permite deslocar as UVs continuamente no modo dinâmico.
    samplerInfo.addressModeU =
        VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.addressModeV =
        VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // W é a terceira coordenada, só usada em texturas 3D.
    samplerInfo.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // Filtragem anisotrópica (para superfícies vistas de lado). Só pode
    // ser ligada se a feature samplerAnisotropy foi ativada no device
    // (createLogicalDevice); senão anisotropyEnable fica VK_FALSE.
    if (anisotropySupported)
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            physicalDevice,
            &properties
        );

        samplerInfo.anisotropyEnable =
            VK_TRUE;

        // Usa o máximo que esta GPU aceita (limite maxSamplerAnisotropy).
        samplerInfo.maxAnisotropy =
            properties.limits.maxSamplerAnisotropy;
    }

    // Cor de borda: só vale em CLAMP_TO_BORDER; com REPEAT, não tem efeito.
    samplerInfo.borderColor =
        VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    // Coordenadas normalizadas: a UV vai de 0 a 1 na textura toda,
    // qualquer que seja o tamanho em pixels. O REPEAT exige isso.
    samplerInfo.unnormalizedCoordinates =
        VK_FALSE;

    // Comparação é para ler profundidade (shadow maps); aqui lemos cor.
    samplerInfo.compareEnable =
        VK_FALSE;

    // LINEAR misturaria dois níveis de mipmap vizinhos. Aqui não tem
    // efeito: a imagem só tem um nível (mipLevels = 1 em createImage),
    // ou seja, o app não usa mipmaps.
    samplerInfo.mipmapMode =
        VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(
            device,
            &samplerInfo,
            nullptr,
            &textureSampler
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Sampler!"
        );
    }

    std::cout
        << "Sampler criado."
        << std::endl;
}
