#include "VulkanApp.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

// Nomes exibidos no título da janela para cada modo (índice = modo).
static const char* const modeNames[] = {
    "",
    "1 - Textura original (UV do modelo)",
    "2 - UV dinamica",
    "3 - Procedural",
    "4 - Textura + procedural",
    "5 - Grade UV (depuracao)"
};

VulkanApp::VulkanApp(std::string modelPath)
    : modelPath(std::move(modelPath))
{
}

static std::vector<char> readFile(const std::string& filename)
{
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

    file.seekg(0);

    file.read(
        buffer.data(),
        static_cast<std::streamsize>(fileSize)
    );

    file.close();

    return buffer;
}

void VulkanApp::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void VulkanApp::initWindow()
{
    if (!glfwInit())
        throw std::runtime_error("Falha ao inicializar GLFW!");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(
        WIDTH,
        HEIGHT,
        "UV Mapping - Entrega 4",
        nullptr,
        nullptr
    );

    if (!window)
        throw std::runtime_error("Falha ao criar janela!");

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);

    updateWindowTitle();
}

void VulkanApp::keyCallback(
    GLFWwindow* window,
    int key,
    int /*scancode*/,
    int action,
    int /*mods*/
)
{
    if (action != GLFW_PRESS)
        return;

    auto* app = static_cast<VulkanApp*>(
        glfwGetWindowUserPointer(window)
    );

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5)
    {
        app->renderMode = MODE_TEXTURE + (key - GLFW_KEY_1);
        app->updateWindowTitle();

        std::cout
            << "Modo: "
            << modeNames[app->renderMode]
            << std::endl;
    }
    else if (key == GLFW_KEY_SPACE)
    {
        app->rotating = !app->rotating;
    }
    else if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void VulkanApp::updateWindowTitle()
{
    const std::string title =
        std::string("UV Mapping - Entrega 4 | ") +
        modeNames[renderMode];

    glfwSetWindowTitle(window, title.c_str());
}

void VulkanApp::initVulkan()
{
    loadMesh();

    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFramebuffers();
    createVertexBuffer();
    createIndexBuffer();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();

    // Os dados da malha já estão na GPU; a cópia na CPU pode ser liberada.
    mesh = MeshData{};

    std::cout << "Vulkan inicializado com sucesso!" << std::endl;

    std::cout
        << "Controles: 1-5 trocam o modo, "
        << "Espaco pausa a rotacao, Esc fecha."
        << std::endl;
}

bool VulkanApp::checkValidationLayerSupport()
{
    uint32_t count = 0;

    vkEnumerateInstanceLayerProperties(
        &count,
        nullptr
    );

    std::vector<VkLayerProperties> available(count);

    vkEnumerateInstanceLayerProperties(
        &count,
        available.data()
    );

    for (const char* layerName : validationLayers)
    {
        bool found = false;

        for (const auto& layer : available)
        {
            if (std::strcmp(layerName, layer.layerName) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

void VulkanApp::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "UV Mapping Dinamico e Procedural";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Entrega 4 Vulkan";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;

    const char** glfwExtensions =
        glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount
        );

    std::vector<const char*> extensions(
        glfwExtensions,
        glfwExtensions + glfwExtensionCount
    );

    extensions.push_back(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    );

    extensions.push_back(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    );

#ifdef __APPLE__

    extensions.push_back(
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    );

    createInfo.flags |=
        VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

#endif

    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(extensions.size());

    createInfo.ppEnabledExtensionNames =
        extensions.data();

    // A validation layer só é habilitada se estiver instalada; sem ela
    // a aplicação continua funcionando, apenas sem as verificações.
    validationEnabled =
        checkValidationLayerSupport();

    if (validationEnabled)
    {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());

        createInfo.ppEnabledLayerNames =
            validationLayers.data();
    }
    else
    {
        std::cout
            << "Aviso: VK_LAYER_KHRONOS_validation nao encontrada, "
            << "executando sem validacao."
            << std::endl;
    }

    if (vkCreateInstance(
            &createInfo,
            nullptr,
            &instance
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Vulkan Instance!"
        );
    }

    std::cout << "Instance criada." << std::endl;
}

void VulkanApp::createSurface()
{
    if (glfwCreateWindowSurface(
            instance,
            window,
            nullptr,
            &surface
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Surface!"
        );
    }

    std::cout << "Surface criada." << std::endl;
}

QueueFamilyIndices VulkanApp::findQueueFamilies(
    VkPhysicalDevice deviceToCheck
)
{
    QueueFamilyIndices indices;

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
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(
            deviceToCheck,
            i,
            surface,
            &presentSupport
        );

        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete())
            break;
    }

    return indices;
}

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

    std::set<std::string> required(
        deviceExtensions.begin(),
        deviceExtensions.end()
    );

    for (const auto& extension : available)
        required.erase(extension.extensionName);

    return required.empty();
}

SwapChainSupportDetails VulkanApp::querySwapChainSupport(
    VkPhysicalDevice deviceToCheck
)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        deviceToCheck,
        surface,
        &details.capabilities
    );

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

    return indices.isComplete() &&
           extensionsSupported &&
           swapChainAdequate;
}

void VulkanApp::pickPhysicalDevice()
{
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

void VulkanApp::createLogicalDevice()
{
    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

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

    // MoltenVK/Apple pode exigir VK_KHR_portability_subset
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

    VkPhysicalDeviceFeatures supportedFeatures{};

    vkGetPhysicalDeviceFeatures(
        physicalDevice,
        &supportedFeatures
    );

    // Filtragem anisotrópica deixa a textura nítida nas superfícies
    // inclinadas em relação à câmera. É opcional.
    anisotropySupported =
        supportedFeatures.samplerAnisotropy == VK_TRUE;

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

    vkGetDeviceQueue(
        device,
        indices.graphicsFamily.value(),
        0,
        &graphicsQueue
    );

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

VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats
)
{
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

    return formats[0];
}

VkPresentModeKHR VulkanApp::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& modes
)
{
    for (const auto& mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApp::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities
)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;

    VkExtent2D extent{
        WIDTH,
        HEIGHT
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

void VulkanApp::createSwapChain()
{
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

    uint32_t imageCount =
        support.capabilities.minImageCount + 1;

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
    createInfo.imageArrayLayers = 1;

    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    uint32_t families[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily !=
        indices.presentFamily)
    {
        createInfo.imageSharingMode =
            VK_SHARING_MODE_CONCURRENT;

        createInfo.queueFamilyIndexCount = 2;

        createInfo.pQueueFamilyIndices =
            families;
    }
    else
    {
        createInfo.imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform =
        support.capabilities.currentTransform;

    createInfo.compositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode =
        presentMode;

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

    vkGetSwapchainImagesKHR(
        device,
        swapChain,
        &imageCount,
        nullptr
    );

    swapChainImages.resize(imageCount);

    vkGetSwapchainImagesKHR(
        device,
        swapChain,
        &imageCount,
        swapChainImages.data()
    );

    swapChainImageFormat =
        format.format;

    swapChainExtent =
        extent;

    std::cout
        << "Swapchain criada."
        << std::endl;
}

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

    createInfo.components.r =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.g =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.b =
        VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.components.a =
        VK_COMPONENT_SWIZZLE_IDENTITY;

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

    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;

    imageInfo.format =
        format;

    imageInfo.tiling =
        VK_IMAGE_TILING_OPTIMAL;

    imageInfo.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    imageInfo.usage =
        usage;

    imageInfo.samples =
        VK_SAMPLE_COUNT_1_BIT;

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

    vkBindImageMemory(
        device,
        image,
        memory,
        0
    );
}

VkFormat VulkanApp::findDepthFormat()
{
    // Nem toda GPU aceita todo formato de profundidade (o MoltenVK no
    // Apple Silicon, por exemplo, não aceita D24), então testamos em ordem.
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

void VulkanApp::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};

    colorAttachment.format =
        swapChainImageFormat;

    colorAttachment.samples =
        VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR;

    colorAttachment.storeOp =
        VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    colorAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    colorAttachment.finalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    depthFormat =
        findDepthFormat();

    VkAttachmentDescription depthAttachment{};

    depthAttachment.format =
        depthFormat;

    depthAttachment.samples =
        VK_SAMPLE_COUNT_1_BIT;

    depthAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR;

    depthAttachment.storeOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    depthAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};

    colorRef.attachment = 0;

    colorRef.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};

    depthRef.attachment = 1;

    depthRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};

    subpass.pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    subpass.colorAttachmentCount = 1;

    subpass.pColorAttachments =
        &colorRef;

    subpass.pDepthStencilAttachment =
        &depthRef;

    VkSubpassDependency dependency{};

    dependency.srcSubpass =
        VK_SUBPASS_EXTERNAL;

    dependency.dstSubpass = 0;

    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    dependency.srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments = {
        colorAttachment,
        depthAttachment
    };

    VkRenderPassCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    createInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());

    createInfo.pAttachments =
        attachments.data();

    createInfo.subpassCount = 1;

    createInfo.pSubpasses =
        &subpass;

    createInfo.dependencyCount = 1;

    createInfo.pDependencies =
        &dependency;

    if (vkCreateRenderPass(
            device,
            &createInfo,
            nullptr,
            &renderPass
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Render Pass!"
        );
    }

    std::cout
        << "Render Pass criado."
        << std::endl;
}

void VulkanApp::createDescriptorSetLayout()
{
    // Binding 0: parâmetros (matrizes, tempo e modo).
    VkDescriptorSetLayoutBinding uboBinding{};

    uboBinding.binding = 0;

    uboBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    uboBinding.descriptorCount = 1;

    uboBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT |
        VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: textura do modelo, amostrada pelas coordenadas UV.
    VkDescriptorSetLayoutBinding samplerBinding{};

    samplerBinding.binding = 1;

    samplerBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    samplerBinding.descriptorCount = 1;

    samplerBinding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT;

    const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
        uboBinding,
        samplerBinding
    };

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

VkShaderModule VulkanApp::createShaderModule(
    const std::vector<char>& code
)
{
    VkShaderModuleCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

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

void VulkanApp::createGraphicsPipeline()
{
    const auto vertCode =
        readFile(
            "shaders/uv_mapping.vert.spv"
        );

    const auto fragCode =
        readFile(
            "shaders/uv_mapping.frag.spv"
        );

    VkShaderModule vertModule =
        createShaderModule(vertCode);

    VkShaderModule fragModule =
        createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};

    vertStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    vertStage.stage =
        VK_SHADER_STAGE_VERTEX_BIT;

    vertStage.module =
        vertModule;

    vertStage.pName =
        "main";

    VkPipelineShaderStageCreateInfo fragStage{};

    fragStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    fragStage.stage =
        VK_SHADER_STAGE_FRAGMENT_BIT;

    fragStage.module =
        fragModule;

    fragStage.pName =
        "main";

    VkPipelineShaderStageCreateInfo stages[] = {
        vertStage,
        fragStage
    };

    VkPipelineVertexInputStateCreateInfo vertexInput{};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    inputAssembly.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    inputAssembly.primitiveRestartEnable =
        VK_FALSE;

    VkVertexInputBindingDescription binding{};

    binding.binding = 0;

    binding.stride =
        sizeof(Vertex);

    binding.inputRate =
        VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<
        VkVertexInputAttributeDescription,
        3
    > attributes{};

    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format =
        VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset =
        offsetof(Vertex, position);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format =
        VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset =
        offsetof(Vertex, normal);

    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format =
        VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset =
        offsetof(Vertex, uv);

    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInput.vertexBindingDescriptionCount = 1;

    vertexInput.pVertexBindingDescriptions =
        &binding;

    vertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(
            attributes.size()
        );

    vertexInput.pVertexAttributeDescriptions =
        attributes.data();

    VkViewport viewport{};

    viewport.width =
        static_cast<float>(
            swapChainExtent.width
        );

    viewport.height =
        static_cast<float>(
            swapChainExtent.height
        );

    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};

    scissor.extent =
        swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};

    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    viewportState.viewportCount = 1;

    viewportState.pViewports =
        &viewport;

    viewportState.scissorCount = 1;

    viewportState.pScissors =
        &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};

    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    rasterizer.depthClampEnable =
        VK_FALSE;

    rasterizer.rasterizerDiscardEnable =
        VK_FALSE;

    rasterizer.polygonMode =
        VK_POLYGON_MODE_FILL;

    rasterizer.lineWidth =
        1.0f;

    // Faces de trás do modelo não são desenhadas. O glTF define as faces
    // da frente em sentido anti-horário, e a inversão do eixo Y feita na
    // matriz de projeção mantém esse sentido na tela.
    rasterizer.cullMode =
        VK_CULL_MODE_BACK_BIT;

    rasterizer.frontFace =
        VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    depthStencil.depthTestEnable =
        VK_TRUE;

    depthStencil.depthWriteEnable =
        VK_TRUE;

    depthStencil.depthCompareOp =
        VK_COMPARE_OP_LESS;

    VkPipelineMultisampleStateCreateInfo multisampling{};

    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    multisampling.rasterizationSamples =
        VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable =
        VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};

    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    colorBlending.logicOpEnable =
        VK_FALSE;

    colorBlending.attachmentCount =
        1;

    colorBlending.pAttachments =
        &colorBlendAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};

    layoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    layoutInfo.setLayoutCount =
        1;

    layoutInfo.pSetLayouts =
        &descriptorSetLayout;

    if (vkCreatePipelineLayout(
            device,
            &layoutInfo,
            nullptr,
            &pipelineLayout
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Pipeline Layout!"
        );
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};

    pipelineInfo.sType =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineInfo.stageCount = 2;

    pipelineInfo.pStages =
        stages;

    pipelineInfo.pVertexInputState =
        &vertexInput;

    pipelineInfo.pInputAssemblyState =
        &inputAssembly;

    pipelineInfo.pViewportState =
        &viewportState;

    pipelineInfo.pRasterizationState =
        &rasterizer;

    pipelineInfo.pMultisampleState =
        &multisampling;

    pipelineInfo.pDepthStencilState =
        &depthStencil;

    pipelineInfo.pColorBlendState =
        &colorBlending;

    pipelineInfo.layout =
        pipelineLayout;

    pipelineInfo.renderPass =
        renderPass;

    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &graphicsPipeline
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Graphics Pipeline!"
        );
    }

    vkDestroyShaderModule(
        device,
        fragModule,
        nullptr
    );

    vkDestroyShaderModule(
        device,
        vertModule,
        nullptr
    );

    std::cout
        << "Graphics Pipeline criada."
        << std::endl;
}

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

void VulkanApp::createCommandPool()
{
    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo info{};

    info.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    info.queueFamilyIndex =
        indices.graphicsFamily.value();

    if (vkCreateCommandPool(
            device,
            &info,
            nullptr,
            &commandPool
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Command Pool!"
        );
    }

    std::cout
        << "Command Pool criada."
        << std::endl;
}

uint32_t VulkanApp::findMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
)
{
    VkPhysicalDeviceMemoryProperties memProperties{};

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice,
        &memProperties
    );

    for (uint32_t i = 0;
         i < memProperties.memoryTypeCount;
         ++i)
    {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags &
             properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error(
        "Tipo de memoria adequado nao encontrado!"
    );
}

void VulkanApp::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory
)
{
    VkBufferCreateInfo bufferInfo{};

    bufferInfo.sType =
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    bufferInfo.size =
        size;

    bufferInfo.usage =
        usage;

    bufferInfo.sharingMode =
        VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(
            device,
            &bufferInfo,
            nullptr,
            &buffer
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Buffer!"
        );
    }

    VkMemoryRequirements requirements{};

    vkGetBufferMemoryRequirements(
        device,
        buffer,
        &requirements
    );

    VkMemoryAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    allocInfo.allocationSize =
        requirements.size;

    allocInfo.memoryTypeIndex =
        findMemoryType(
            requirements.memoryTypeBits,
            properties
        );

    if (vkAllocateMemory(
            device,
            &allocInfo,
            nullptr,
            &memory
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao alocar memoria do Buffer!"
        );
    }

    vkBindBufferMemory(
        device,
        buffer,
        memory,
        0
    );
}

VkCommandBuffer VulkanApp::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    allocInfo.commandPool =
        commandPool;

    allocInfo.commandBufferCount =
        1;

    VkCommandBuffer commandBuffer =
        VK_NULL_HANDLE;

    vkAllocateCommandBuffers(
        device,
        &allocInfo,
        &commandBuffer
    );

    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(
        commandBuffer,
        &beginInfo
    );

    return commandBuffer;
}

void VulkanApp::endSingleTimeCommands(
    VkCommandBuffer commandBuffer
)
{
    vkEndCommandBuffer(
        commandBuffer
    );

    VkSubmitInfo submitInfo{};

    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount =
        1;

    submitInfo.pCommandBuffers =
        &commandBuffer;

    vkQueueSubmit(
        graphicsQueue,
        1,
        &submitInfo,
        VK_NULL_HANDLE
    );

    vkQueueWaitIdle(
        graphicsQueue
    );

    vkFreeCommandBuffers(
        device,
        commandPool,
        1,
        &commandBuffer
    );
}

void VulkanApp::copyBuffer(
    VkBuffer source,
    VkBuffer destination,
    VkDeviceSize size
)
{
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands();

    VkBufferCopy region{};

    region.size =
        size;

    vkCmdCopyBuffer(
        commandBuffer,
        source,
        destination,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}

// Copia dados da CPU para um buffer na memória local da GPU (mais rápida),
// passando por um buffer intermediário visível pela CPU (staging buffer).
void VulkanApp::uploadToDeviceLocalBuffer(
    const void* data,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkBuffer& buffer,
    VkDeviceMemory& memory
)
{
    VkBuffer stagingBuffer =
        VK_NULL_HANDLE;

    VkDeviceMemory stagingMemory =
        VK_NULL_HANDLE;

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    void* mapped = nullptr;

    vkMapMemory(
        device,
        stagingMemory,
        0,
        size,
        0,
        &mapped
    );

    std::memcpy(
        mapped,
        data,
        static_cast<size_t>(size)
    );

    vkUnmapMemory(
        device,
        stagingMemory
    );

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer,
        memory
    );

    copyBuffer(
        stagingBuffer,
        buffer,
        size
    );

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
}

void VulkanApp::transitionImageLayout(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
)
{
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};

    barrier.sType =
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    barrier.oldLayout =
        oldLayout;

    barrier.newLayout =
        newLayout;

    barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.image =
        image;

    barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // Antes da cópia: nada a esperar.
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

    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;

    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};

    region.imageExtent = {
        width,
        height,
        1
    };

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

void VulkanApp::loadMesh()
{
    mesh =
        loadModel(modelPath);

    indexCount =
        static_cast<uint32_t>(mesh.indices.size());
}

void VulkanApp::createVertexBuffer()
{
    uploadToDeviceLocalBuffer(
        mesh.vertices.data(),
        sizeof(Vertex) * mesh.vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    std::cout
        << "Vertex Buffer criado."
        << std::endl;
}

void VulkanApp::createIndexBuffer()
{
    uploadToDeviceLocalBuffer(
        mesh.indices.data(),
        sizeof(uint32_t) * mesh.indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        indexBuffer,
        indexBufferMemory
    );

    std::cout
        << "Index Buffer criado."
        << std::endl;
}

void VulkanApp::createTextureImage()
{
    const TextureData& texture =
        mesh.texture;

    const VkDeviceSize size =
        static_cast<VkDeviceSize>(texture.pixels.size());

    VkBuffer stagingBuffer =
        VK_NULL_HANDLE;

    VkDeviceMemory stagingMemory =
        VK_NULL_HANDLE;

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    void* mapped = nullptr;

    vkMapMemory(
        device,
        stagingMemory,
        0,
        size,
        0,
        &mapped
    );

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
    createImage(
        texture.width,
        texture.height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        textureImage,
        textureImageMemory
    );

    transitionImageLayout(
        textureImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    copyBufferToImage(
        stagingBuffer,
        textureImage,
        texture.width,
        texture.height
    );

    transitionImageLayout(
        textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

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

void VulkanApp::createTextureImageView()
{
    textureImageView =
        createImageView(
            textureImage,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
}

void VulkanApp::createTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};

    samplerInfo.sType =
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

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

    samplerInfo.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_REPEAT;

    if (anisotropySupported)
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            physicalDevice,
            &properties
        );

        samplerInfo.anisotropyEnable =
            VK_TRUE;

        samplerInfo.maxAnisotropy =
            properties.limits.maxSamplerAnisotropy;
    }

    samplerInfo.borderColor =
        VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    samplerInfo.unnormalizedCoordinates =
        VK_FALSE;

    samplerInfo.compareEnable =
        VK_FALSE;

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

void VulkanApp::createUniformBuffers()
{
    const VkDeviceSize size =
        sizeof(UniformBufferObject);

    uniformBuffers.resize(
        swapChainImages.size()
    );

    uniformBuffersMemory.resize(
        swapChainImages.size()
    );

    uniformBuffersMapped.resize(
        swapChainImages.size()
    );

    for (size_t i = 0;
         i < swapChainImages.size();
         ++i)
    {
        createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i],
            uniformBuffersMemory[i]
        );

        vkMapMemory(
            device,
            uniformBuffersMemory[i],
            0,
            size,
            0,
            &uniformBuffersMapped[i]
        );
    }

    std::cout
        << "Uniform Buffers criados."
        << std::endl;
}

void VulkanApp::createDescriptorPool()
{
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

void VulkanApp::createDescriptorSets()
{
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
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer =
            uniformBuffers[i];

        bufferInfo.offset = 0;

        bufferInfo.range =
            sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};

        imageInfo.imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView =
            textureImageView;

        imageInfo.sampler =
            textureSampler;

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

void VulkanApp::updateUniformBuffer(
    uint32_t imageIndex
)
{
    static const auto startTime =
        std::chrono::high_resolution_clock::now();

    const auto currentTime =
        std::chrono::high_resolution_clock::now();

    const float time =
        std::chrono::duration<float>(
            currentTime - startTime
        ).count();

    // A rotação acumula pelo tempo entre frames, então pausar e retomar
    // (tecla Espaço) continua do mesmo ângulo, sem salto.
    const float deltaTime =
        time - lastFrameTime;

    lastFrameTime = time;

    if (rotating)
        rotationAngle += deltaTime * 0.6f;

    UniformBufferObject ubo{};

    ubo.model = glm::rotate(
        glm::mat4(1.0f),
        rotationAngle,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    ubo.view = glm::lookAt(
        glm::vec3(0.0f, 0.6f, 3.6f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    ubo.projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(swapChainExtent.width) /
        static_cast<float>(swapChainExtent.height),
        0.1f,
        100.0f
    );

    // O glm segue a convenção do OpenGL (Y para cima no espaço de recorte);
    // no Vulkan o Y aponta para baixo, então o eixo é invertido.
    ubo.projection[1][1] *= -1.0f;

    ubo.time = time;
    ubo.mode = renderMode;

    std::memcpy(
        uniformBuffersMapped[imageIndex],
        &ubo,
        sizeof(ubo)
    );
}

void VulkanApp::createCommandBuffers()
{
    commandBuffers.resize(
        swapChainFramebuffers.size()
    );

    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.commandPool =
        commandPool;

    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    allocInfo.commandBufferCount =
        static_cast<uint32_t>(
            commandBuffers.size()
        );

    if (vkAllocateCommandBuffers(
            device,
            &allocInfo,
            commandBuffers.data()
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao alocar Command Buffers!"
        );
    }

    for (size_t i = 0;
         i < commandBuffers.size();
         ++i)
    {
        VkCommandBufferBeginInfo beginInfo{};

        beginInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(
                commandBuffers[i],
                &beginInfo
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao iniciar Command Buffer!"
            );
        }

        std::array<VkClearValue, 2> clearValues{};

        clearValues[0].color = {{
            0.04f,
            0.04f,
            0.10f,
            1.0f
        }};

        // Profundidade 1.0 = o ponto mais distante possível.
        clearValues[1].depthStencil = {
            1.0f,
            0
        };

        VkRenderPassBeginInfo renderPassInfo{};

        renderPassInfo.sType =
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

        renderPassInfo.renderPass =
            renderPass;

        renderPassInfo.framebuffer =
            swapChainFramebuffers[i];

        renderPassInfo.renderArea.offset =
            {0, 0};

        renderPassInfo.renderArea.extent =
            swapChainExtent;

        renderPassInfo.clearValueCount =
            static_cast<uint32_t>(clearValues.size());

        renderPassInfo.pClearValues =
            clearValues.data();

        vkCmdBeginRenderPass(
            commandBuffers[i],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdBindPipeline(
            commandBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline
        );

        VkBuffer buffers[] = {
            vertexBuffer
        };

        VkDeviceSize offsets[] = {
            0
        };

        vkCmdBindVertexBuffers(
            commandBuffers[i],
            0,
            1,
            buffers,
            offsets
        );

        vkCmdBindIndexBuffer(
            commandBuffers[i],
            indexBuffer,
            0,
            VK_INDEX_TYPE_UINT32
        );

        vkCmdBindDescriptorSets(
            commandBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &descriptorSets[i],
            0,
            nullptr
        );

        vkCmdDrawIndexed(
            commandBuffers[i],
            indexCount,
            1,
            0,
            0,
            0
        );

        vkCmdEndRenderPass(
            commandBuffers[i]
        );

        if (vkEndCommandBuffer(
                commandBuffers[i]
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao finalizar Command Buffer!"
            );
        }
    }

    std::cout
        << "Command Buffers gravados."
        << std::endl;
}

void VulkanApp::createSyncObjects()
{
    imageAvailableSemaphores.resize(
        MAX_FRAMES_IN_FLIGHT
    );

    renderFinishedSemaphores.resize(
        swapChainImages.size()
    );

    inFlightFences.resize(
        MAX_FRAMES_IN_FLIGHT
    );

    imagesInFlight.resize(
        swapChainImages.size(),
        VK_NULL_HANDLE
    );

    VkSemaphoreCreateInfo semaphoreInfo{};

    semaphoreInfo.sType =
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};

    fenceInfo.sType =
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    fenceInfo.flags =
        VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0;
         i < MAX_FRAMES_IN_FLIGHT;
         ++i)
    {
        if (vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &imageAvailableSemaphores[i]
            ) != VK_SUCCESS ||
            vkCreateFence(
                device,
                &fenceInfo,
                nullptr,
                &inFlightFences[i]
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao criar sincronizacao de frame!"
            );
        }
    }

    for (size_t i = 0;
         i < renderFinishedSemaphores.size();
         ++i)
    {
        if (vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &renderFinishedSemaphores[i]
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao criar renderFinishedSemaphore!"
            );
        }
    }

    std::cout
        << "Sincronizacao criada."
        << std::endl;
}

void VulkanApp::drawFrame()
{
    vkWaitForFences(
        device,
        1,
        &inFlightFences[currentFrame],
        VK_TRUE,
        UINT64_MAX
    );

    uint32_t imageIndex = 0;

    VkResult result =
        vkAcquireNextImageKHR(
            device,
            swapChain,
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &imageIndex
        );

    if (result != VK_SUCCESS &&
        result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error(
            "Falha ao adquirir imagem da Swapchain!"
        );
    }

    if (imagesInFlight[imageIndex] !=
        VK_NULL_HANDLE)
    {
        vkWaitForFences(
            device,
            1,
            &imagesInFlight[imageIndex],
            VK_TRUE,
            UINT64_MAX
        );
    }

    imagesInFlight[imageIndex] =
        inFlightFences[currentFrame];

    // Atualiza o tempo enviado ao shader.
    updateUniformBuffer(imageIndex);

    VkSemaphore waitSemaphore =
        imageAvailableSemaphores[currentFrame];

    VkSemaphore signalSemaphore =
        renderFinishedSemaphores[imageIndex];

    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};

    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount =
        1;

    submitInfo.pWaitSemaphores =
        &waitSemaphore;

    submitInfo.pWaitDstStageMask =
        &waitStage;

    submitInfo.commandBufferCount =
        1;

    submitInfo.pCommandBuffers =
        &commandBuffers[imageIndex];

    submitInfo.signalSemaphoreCount =
        1;

    submitInfo.pSignalSemaphores =
        &signalSemaphore;

    vkResetFences(
        device,
        1,
        &inFlightFences[currentFrame]
    );

    if (vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            inFlightFences[currentFrame]
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao enviar comandos para GPU!"
        );
    }

    VkSwapchainKHR swapChains[] = {
        swapChain
    };

    VkPresentInfoKHR presentInfo{};

    presentInfo.sType =
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount =
        1;

    presentInfo.pWaitSemaphores =
        &signalSemaphore;

    presentInfo.swapchainCount =
        1;

    presentInfo.pSwapchains =
        swapChains;

    presentInfo.pImageIndices =
        &imageIndex;

    result =
        vkQueuePresentKHR(
            presentQueue,
            &presentInfo
        );

    if (result != VK_SUCCESS &&
        result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error(
            "Falha ao apresentar imagem!"
        );
    }

    currentFrame =
        (currentFrame + 1) %
        MAX_FRAMES_IN_FLIGHT;
}

void VulkanApp::mainLoop()
{
    std::cout
        << "Entrando no loop de renderizacao..."
        << std::endl;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        drawFrame();
    }

    vkDeviceWaitIdle(device);

    std::cout
        << "Loop de renderizacao encerrado."
        << std::endl;
}

void VulkanApp::cleanup()
{
    if (device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    for (size_t i = 0;
         i < imageAvailableSemaphores.size();
         ++i)
    {
        vkDestroySemaphore(
            device,
            imageAvailableSemaphores[i],
            nullptr
        );

        vkDestroyFence(
            device,
            inFlightFences[i],
            nullptr
        );
    }

    for (VkSemaphore semaphore :
         renderFinishedSemaphores)
    {
        vkDestroySemaphore(
            device,
            semaphore,
            nullptr
        );
    }

    for (size_t i = 0;
         i < uniformBuffers.size();
         ++i)
    {
        if (uniformBuffersMapped[i] != nullptr)
        {
            vkUnmapMemory(
                device,
                uniformBuffersMemory[i]
            );
        }

        vkDestroyBuffer(
            device,
            uniformBuffers[i],
            nullptr
        );

        vkFreeMemory(
            device,
            uniformBuffersMemory[i],
            nullptr
        );
    }

    if (vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(
            device,
            vertexBuffer,
            nullptr
        );
    }

    if (vertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            device,
            vertexBufferMemory,
            nullptr
        );
    }

    if (indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(
            device,
            indexBuffer,
            nullptr
        );
    }

    if (indexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            device,
            indexBufferMemory,
            nullptr
        );
    }

    if (textureSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(
            device,
            textureSampler,
            nullptr
        );
    }

    if (textureImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(
            device,
            textureImageView,
            nullptr
        );
    }

    if (textureImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(
            device,
            textureImage,
            nullptr
        );
    }

    if (textureImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            device,
            textureImageMemory,
            nullptr
        );
    }

    if (depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(
            device,
            depthImageView,
            nullptr
        );
    }

    if (depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(
            device,
            depthImage,
            nullptr
        );
    }

    if (depthImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            device,
            depthImageMemory,
            nullptr
        );
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(
            device,
            descriptorPool,
            nullptr
        );
    }

    for (VkFramebuffer framebuffer :
         swapChainFramebuffers)
    {
        vkDestroyFramebuffer(
            device,
            framebuffer,
            nullptr
        );
    }

    if (graphicsPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(
            device,
            graphicsPipeline,
            nullptr
        );
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(
            device,
            pipelineLayout,
            nullptr
        );
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(
            device,
            descriptorSetLayout,
            nullptr
        );
    }

    if (renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(
            device,
            renderPass,
            nullptr
        );
    }

    for (VkImageView imageView :
         swapChainImageViews)
    {
        vkDestroyImageView(
            device,
            imageView,
            nullptr
        );
    }

    if (swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(
            device,
            swapChain,
            nullptr
        );
    }

    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(
            device,
            commandPool,
            nullptr
        );
    }

    if (device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(
            device,
            nullptr
        );
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            instance,
            surface,
            nullptr
        );
    }

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(
            instance,
            nullptr
        );
    }

    if (window != nullptr)
    {
        glfwDestroyWindow(window);
    }

    glfwTerminate();
}