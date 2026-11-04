// Escolha da GPU, identificação das filas e criação do dispositivo
// lógico.

#include "VulkanApp.hpp"

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

// Procura na GPU uma família de filas que desenhe (gráfica) e uma que
// apresente na superfície. Usada para testar cada GPU e de novo onde os
// índices são necessários: device, swapchain e command pool.
QueueFamilyIndices VulkanApp::findQueueFamilies(
    VkPhysicalDevice deviceToCheck
)
{
    QueueFamilyIndices indices;

    // A GPU recebe comandos por filas, agrupadas em famílias com as mesmas
    // capacidades (queueFlags). As listas do Vulkan vêm em duas chamadas:
    // a primeira, com nullptr, só conta; a segunda preenche o vetor.
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(
        deviceToCheck,
        &count,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> families(count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        deviceToCheck,
        &count,
        families.data()
    );

    for (uint32_t i = 0; i < count; ++i)
    {
        // VK_QUEUE_GRAPHICS_BIT: a família aceita comandos de desenho.
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        // Apresentar não é um bit de queueFlags: depende da superfície,
        // então é perguntado família por família.
        VkBool32 presentSupport = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(
            deviceToCheck,
            i,
            surface,
            &presentSupport
        );

        if (presentSupport)
            indices.presentFamily = i;

        // Gráfica e apresentação podem ser a mesma família. Nesse caso
        // createSwapChain usa VK_SHARING_MODE_EXCLUSIVE; se forem
        // diferentes, usa CONCURRENT para as duas filas usarem as imagens.
        if (indices.isComplete())
            break;
    }

    return indices;
}

// Confere se a GPU tem as extensões de deviceExtensions (VulkanApp.hpp),
// ou seja, a VK_KHR_swapchain. É um dos testes de isDeviceSuitable.
bool VulkanApp::checkDeviceExtensionSupport(
    VkPhysicalDevice deviceToCheck
)
{
    uint32_t count = 0;

    vkEnumerateDeviceExtensionProperties(
        deviceToCheck,
        nullptr,
        &count,
        nullptr
    );

    std::vector<VkExtensionProperties> available(count);

    vkEnumerateDeviceExtensionProperties(
        deviceToCheck,
        nullptr,
        &count,
        available.data()
    );

    // Mostrar imagens numa janela não faz parte do núcleo do Vulkan, por
    // isso a swapchain é extensão. Cada extensão achada sai do set; se
    // sobrar alguma, a GPU não serve.
    std::set<std::string> required(
        deviceExtensions.begin(),
        deviceExtensions.end()
    );

    for (const auto& extension : available)
        required.erase(extension.extensionName);

    return required.empty();
}

// Pergunta o que a superfície aceita nesta GPU. Serve ao teste de
// isDeviceSuitable e, depois, a createSwapChain, que escolhe formato,
// modo de apresentação e tamanho com base nestes dados.
SwapChainSupportDetails VulkanApp::querySwapChainSupport(
    VkPhysicalDevice deviceToCheck
)
{
    SwapChainSupportDetails details;

    // Limites da superfície: número de imagens da swapchain e tamanhos
    // mínimo, máximo e atual (currentExtent, usado em chooseSwapExtent).
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        deviceToCheck,
        surface,
        &details.capabilities
    );

    // Formatos aceitos: pares formato de pixel + espaço de cor.
    // chooseSwapSurfaceFormat procura B8G8R8A8_SRGB com SRGB_NONLINEAR.
    uint32_t formatCount = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(
        deviceToCheck,
        surface,
        &formatCount,
        nullptr
    );

    if (formatCount > 0)
    {
        details.formats.resize(formatCount);

        vkGetPhysicalDeviceSurfaceFormatsKHR(
            deviceToCheck,
            surface,
            &formatCount,
            details.formats.data()
        );
    }

    // Modos de apresentação: como as imagens prontas vão para a tela
    // (FIFO, MAILBOX...). chooseSwapPresentMode prefere MAILBOX e, sem
    // ele, usa FIFO.
    uint32_t presentCount = 0;

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        deviceToCheck,
        surface,
        &presentCount,
        nullptr
    );

    if (presentCount > 0)
    {
        details.presentModes.resize(presentCount);

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            deviceToCheck,
            surface,
            &presentCount,
            details.presentModes.data()
        );
    }

    return details;
}

// Diz se a GPU serve: precisa das filas gráfica e de apresentação, da
// extensão de swapchain e de ao menos um formato e um modo de
// apresentação. pickPhysicalDevice a chama GPU por GPU até achar uma.
bool VulkanApp::isDeviceSuitable(
    VkPhysicalDevice deviceToCheck
)
{
    const QueueFamilyIndices indices =
        findQueueFamilies(deviceToCheck);

    const bool extensionsSupported =
        checkDeviceExtensionSupport(deviceToCheck);

    bool swapChainAdequate = false;

    if (extensionsSupported)
    {
        const auto support =
            querySwapChainSupport(deviceToCheck);

        swapChainAdequate =
            !support.formats.empty() &&
            !support.presentModes.empty();
    }

    // A anisotropia não entra no teste: é opcional, e createLogicalDevice
    // só a liga se a GPU tiver.
    return indices.isComplete() &&
           extensionsSupported &&
           swapChainAdequate;
}

// Escolhe a GPU (VkPhysicalDevice) que o programa vai usar. Vem depois
// de createSurface, pois o teste das GPUs usa a superfície, e antes de
// createLogicalDevice.
void VulkanApp::pickPhysicalDevice()
{
    // A VkPhysicalDevice não é criada, só listada pela instância. Por
    // isso cleanup não a destrói: ela some junto com a instância.
    uint32_t count = 0;

    vkEnumeratePhysicalDevices(
        instance,
        &count,
        nullptr
    );

    if (count == 0)
    {
        throw std::runtime_error(
            "Nenhuma GPU Vulkan encontrada!"
        );
    }

    std::vector<VkPhysicalDevice> devices(count);

    vkEnumeratePhysicalDevices(
        instance,
        &count,
        devices.data()
    );

    // Fica com a primeira GPU adequada da lista (break abaixo). Não há
    // preferência por GPU dedicada em vez de integrada.
    for (VkPhysicalDevice candidate : devices)
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            candidate,
            &properties
        );

        std::cout
            << "GPU encontrada: "
            << properties.deviceName
            << std::endl;

        if (isDeviceSuitable(candidate))
        {
            physicalDevice = candidate;

            std::cout
                << "GPU selecionada: "
                << properties.deviceName
                << std::endl;

            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error(
            "Nenhuma GPU adequada encontrada!"
        );
    }
}

// Cria o dispositivo lógico (VkDevice) sobre a GPU escolhida: liga as
// extensões e os recursos opcionais e cria as filas. Buffers, imagens,
// swapchain e pipeline são criados depois a partir dele.
void VulkanApp::createLogicalDevice()
{
    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    // Um VkDeviceQueueCreateInfo por família. O set tira a repetição quando
    // gráfica e apresentação são a mesma família: o Vulkan não aceita a
    // mesma família duas vezes nesta lista.
    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    // Uma fila por família. A prioridade (de 0.0 a 1.0) é relativa às
    // outras filas do device; é obrigatória mesmo com uma fila só.
    float priority = 1.0f;

    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo queueInfo{};

        queueInfo.sType =
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        queueInfos.push_back(queueInfo);
    }

    // MoltenVK/Apple pode exigir VK_KHR_portability_subset: se a GPU lista
    // essa extensão, ela tem de ser ligada (ver também createInstance).
    uint32_t extensionCount = 0;

    vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &extensionCount,
        nullptr
    );

    std::vector<VkExtensionProperties> availableExtensions(
        extensionCount
    );

    vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );

    bool portabilitySupported = false;

    for (const auto& extension : availableExtensions)
    {
        if (std::strcmp(
                extension.extensionName,
                "VK_KHR_portability_subset"
            ) == 0)
        {
            portabilitySupported = true;
            break;
        }
    }

    std::vector<const char*> enabledExtensions =
        deviceExtensions;

    if (portabilitySupported)
        enabledExtensions.push_back(
            "VK_KHR_portability_subset"
        );

    // Recursos opcionais (features) que a GPU oferece. Só os marcados em
    // pEnabledFeatures ficam ligados; pedir um sem suporte faz
    // vkCreateDevice falhar.
    VkPhysicalDeviceFeatures supportedFeatures{};

    vkGetPhysicalDeviceFeatures(
        physicalDevice,
        &supportedFeatures
    );

    // Filtragem anisotrópica deixa a textura nítida nas superfícies
    // inclinadas em relação à câmera. É opcional.
    anisotropySupported =
        supportedFeatures.samplerAnisotropy == VK_TRUE;

    // Começa tudo desligado ({}) e liga só a anisotropia, se existir.
    // createTextureSampler confere anisotropySupported antes de usá-la no
    // sampler da baseTexture (binding 1 do uv_mapping.frag).
    VkPhysicalDeviceFeatures features{};

    features.samplerAnisotropy =
        anisotropySupported ? VK_TRUE : VK_FALSE;

    VkDeviceCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueInfos.size());

    createInfo.pQueueCreateInfos =
        queueInfos.data();

    createInfo.pEnabledFeatures =
        &features;

    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(
            enabledExtensions.size()
        );

    createInfo.ppEnabledExtensionNames =
        enabledExtensions.data();

    // As layers de validação são habilitadas na Instance
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    if (vkCreateDevice(
            physicalDevice,
            &createInfo,
            nullptr,
            &device
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Logical Device!"
        );
    }

    // As filas nascem com o device; aqui só se pegam os handles (índice 0,
    // pois queueCount = 1). graphicsQueue recebe os desenhos de drawFrame
    // e as cópias de endSingleTimeCommands.
    vkGetDeviceQueue(
        device,
        indices.graphicsFamily.value(),
        0,
        &graphicsQueue
    );

    // presentQueue só faz vkQueuePresentKHR (drawFrame). Com famílias
    // iguais, os dois handles são a mesma fila.
    vkGetDeviceQueue(
        device,
        indices.presentFamily.value(),
        0,
        &presentQueue
    );

    std::cout
        << "Logical Device criado."
        << std::endl;
}
