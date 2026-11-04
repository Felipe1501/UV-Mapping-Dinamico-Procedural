// Swapchain (as imagens que vão para a tela), image views, imagem de
// profundidade e framebuffers.

#include "VulkanApp.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

// Escolhe o formato de pixel e o espaço de cor das imagens da swapchain,
// entre os que a superfície aceita. Usada por createSwapChain.
VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats
)
{
    // Preferimos B8G8R8A8_SRGB. O fragment shader grava outColor sem
    // correção de gama; com o formato _SRGB, a GPU converte de linear para
    // sRGB na escrita (a textura R8G8B8A8_SRGB vira linear na leitura).
    for (const auto& format : formats)
    {
        if (format.format ==
                VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    // Se o par não existir, usa o primeiro formato da lista. Ela não vem
    // vazia: isDeviceSuitable só aceita GPUs com pelo menos um formato.
    return formats[0];
}

// Escolhe o modo de apresentação: como as imagens prontas vão para a
// tela. Usada por createSwapChain.
VkPresentModeKHR VulkanApp::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& modes
)
{
    // MAILBOX: a imagem só troca na atualização do monitor (sem tearing),
    // mas uma imagem nova substitui a que esperava na fila. Baixa latência,
    // e o app pode desenhar mais rápido que o monitor (sobras descartadas).
    for (const auto& mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }

    // FIFO: fila presa à taxa do monitor (V-Sync). É o único modo que o
    // Vulkan garante em toda GPU, por isso fica como reserva.
    return VK_PRESENT_MODE_FIFO_KHR;
}

// Escolhe a resolução (largura x altura) das imagens da swapchain.
// Usada por createSwapChain.
VkExtent2D VulkanApp::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities
)
{
    // Em geral a superfície já informa o tamanho atual da janela, e a
    // swapchain usa exatamente esse. UINT32_MAX é o valor especial que
    // quer dizer "quem decide o tamanho é a swapchain".
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;

    // A superfície não definiu o tamanho (acontece no Wayland): usa o
    // tamanho da janela em pixels, limitado ao que a superfície aceita.
    int width = 0;
    int height = 0;

    // Em pixels, não em coordenadas de tela: em telas de alta densidade
    // (HiDPI) os dois tamanhos podem ser diferentes.
    glfwGetFramebufferSize(
        window,
        &width,
        &height
    );

    VkExtent2D extent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    extent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(
            capabilities.maxImageExtent.width,
            extent.width
        )
    );

    extent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(
            capabilities.maxImageExtent.height,
            extent.height
        )
    );

    return extent;
}

// Cria a swapchain: um grupo de imagens que se revezam entre receber o
// desenho e ir para a tela. Roda logo após createLogicalDevice, que ligou
// a extensão VK_KHR_swapchain. É criada uma só vez (janela de tamanho fixo).
void VulkanApp::createSwapChain()
{
    // Pergunta à superfície o que ela aceita (VulkanDevice.cpp) e escolhe
    // formato, modo de apresentação e tamanho com as funções acima.
    const auto support =
        querySwapChainSupport(physicalDevice);

    const auto format =
        chooseSwapSurfaceFormat(
            support.formats
        );

    const auto presentMode =
        chooseSwapPresentMode(
            support.presentModes
        );

    const auto extent =
        chooseSwapExtent(
            support.capabilities
        );

    // Uma imagem a mais que o mínimo, para não ficar esperando o driver
    // liberar uma imagem antes de começar o próximo desenho.
    uint32_t imageCount =
        support.capabilities.minImageCount + 1;

    // maxImageCount == 0 quer dizer "sem limite máximo".
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount)
    {
        imageCount =
            support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    // 1 camada por imagem: mais de uma só em telas 3D estereoscópicas.
    createInfo.imageArrayLayers = 1;

    // As imagens servem de color attachment: o render pass desenha direto
    // nelas (attachment 0) e no fim as deixa no layout PRESENT_SRC_KHR,
    // pronto para a tela.
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // A fila gráfica desenha nas imagens e a de apresentação as mostra.
    // Se forem de famílias diferentes, as duas precisam acessar as imagens.
    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    uint32_t families[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily !=
        indices.presentFamily)
    {
        // CONCURRENT: as duas famílias listadas usam as imagens sem
        // transferir a posse entre filas. Custa um pouco de desempenho, mas
        // dispensa as barreiras de transferência de posse.
        createInfo.imageSharingMode =
            VK_SHARING_MODE_CONCURRENT;

        createInfo.queueFamilyIndexCount = 2;

        createInfo.pQueueFamilyIndices =
            families;
    }
    else
    {
        // Mesma família: EXCLUSIVE, o modo mais rápido. Como só uma família
        // usa as imagens, não há posse a transferir.
        createInfo.imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE;
    }

    // Sem rotação ou espelhamento extra: mantém a transformação atual da
    // superfície.
    createInfo.preTransform =
        support.capabilities.currentTransform;

    // Janela opaca: o alfa é ignorado na composição com outras janelas.
    // Nada se perde, porque o fragment shader sempre grava alfa 1.0.
    createInfo.compositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode =
        presentMode;

    // O driver pode pular pixels escondidos (atrás de outra janela). É
    // seguro: o programa nunca lê as imagens da swapchain de volta.
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(
            device,
            &createInfo,
            nullptr,
            &swapChain
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Swapchain!"
        );
    }

    // minImageCount era só um mínimo: o driver pode ter criado mais. A 1ª
    // chamada lê o total e a 2ª pega as imagens, que pertencem à swapchain
    // e são liberadas junto com ela (vkDestroySwapchainKHR, em cleanup).
    vkGetSwapchainImagesKHR(
        device,
        swapChain,
        &imageCount,
        nullptr
    );

    // Há um framebuffer, um command buffer, um Uniform Buffer e um
    // descriptor set para cada imagem da swapchain.
    swapChainImages.resize(imageCount);

    vkGetSwapchainImagesKHR(
        device,
        swapChain,
        &imageCount,
        swapChainImages.data()
    );

    // Guardados para o render pass (formato da cor), o pipeline (viewport
    // e scissor), a imagem de profundidade e os framebuffers (tamanho).
    swapChainImageFormat =
        format.format;

    swapChainExtent =
        extent;

    std::cout
        << "Swapchain criada."
        << std::endl;
}

// Cria uma image view para cada imagem da swapchain. O framebuffer não
// recebe a VkImage direto, e sim uma view, que diz como enxergá-la (2D,
// formato, aspecto). Vem logo depois de createSwapChain.
void VulkanApp::createImageViews()
{
    swapChainImageViews.resize(
        swapChainImages.size()
    );

    for (size_t i = 0;
         i < swapChainImages.size();
         ++i)
    {
        swapChainImageViews[i] =
            createImageView(
                swapChainImages[i],
                swapChainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
    }

    std::cout
        << "Image Views criadas."
        << std::endl;
}

// Escolhe o formato do depth buffer entre alguns candidatos. É chamada
// por createRenderPass, que guarda o resultado em depthFormat.
VkFormat VulkanApp::findDepthFormat()
{
    // Nem toda GPU aceita todo formato de profundidade (o MoltenVK no
    // Apple Silicon, por exemplo, não aceita D24), então testamos em ordem.
    // D32_SFLOAT vem primeiro: 32 bits em float e sem stencil (não usamos).
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates)
    {
        VkFormatProperties properties{};

        vkGetPhysicalDeviceFormatProperties(
            physicalDevice,
            format,
            &properties
        );

        // createImage usa tiling ótimo (pixels arrumados do jeito mais rápido
        // para a GPU), por isso olhamos optimalTilingFeatures. O formato tem
        // de servir como depth attachment.
        if (properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return format;
        }
    }

    throw std::runtime_error(
        "Nenhum formato de profundidade suportado!"
    );
}

// Cria o depth buffer (imagem de profundidade) e a view dela, do tamanho
// da swapchain. Roda depois de createRenderPass, que escolheu depthFormat
// com findDepthFormat.
void VulkanApp::createDepthResources()
{
    // O depth buffer guarda a distância de cada pixel até a câmera, para
    // que as partes da frente do modelo escondam as de trás.
    createImage(
        swapChainExtent.width,
        swapChainExtent.height,
        depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        depthImage,
        depthImageMemory
    );

    // A imagem nasce em layout UNDEFINED e não precisa de barreira aqui: o
    // render pass a leva para DEPTH_STENCIL_ATTACHMENT_OPTIMAL sozinho
    // (initialLayout e depthRef em createRenderPass).
    depthImageView =
        createImageView(
            depthImage,
            depthFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );

    std::cout
        << "Depth Buffer criado."
        << std::endl;
}

// Cria um framebuffer por imagem da swapchain, juntando a view de cor
// dela com a view de profundidade. Vem depois de createDepthResources, e
// cada command buffer (createCommandBuffers) desenha em um deles.
void VulkanApp::createFramebuffers()
{
    swapChainFramebuffers.resize(
        swapChainImageViews.size()
    );

    for (size_t i = 0;
         i < swapChainImageViews.size();
         ++i)
    {
        // O mesmo depth buffer serve todas as imagens da swapchain: a
        // dependência do render pass faz cada frame esperar a escrita de
        // profundidade do frame anterior terminar antes de começar a sua.
        VkImageView attachments[] = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo info{};

        info.sType =
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        // O framebuffer é feito para este render pass: pAttachments segue a
        // ordem dele (0 = cor, 1 = profundidade). O outColor (location 0) do
        // uv_mapping.frag é gravado no 0, a imagem da swapchain.
        info.renderPass =
            renderPass;

        info.attachmentCount =
            2;

        info.pAttachments =
            attachments;

        info.width =
            swapChainExtent.width;

        info.height =
            swapChainExtent.height;

        info.layers = 1;

        if (vkCreateFramebuffer(
                device,
                &info,
                nullptr,
                &swapChainFramebuffers[i]
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao criar Framebuffer!"
            );
        }
    }

    std::cout
        << "Framebuffers criados."
        << std::endl;
}
