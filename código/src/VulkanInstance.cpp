// Instância do Vulkan, camada de validação e superfície (ligação
// com a janela).

#include "VulkanApp.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

// Confere se as camadas de validationLayers (VulkanApp.hpp) estão
// instaladas no sistema. Chamada por createInstance, que só liga a
// validação se esta função devolver true.
bool VulkanApp::checkValidationLayerSupport()
{
    // Padrão do Vulkan para listas: a primeira chamada (com nullptr) só
    // devolve a quantidade; a segunda preenche o vetor já dimensionado.
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

// Cria a VkInstance, a conexão da aplicação com o Vulkan. É o primeiro
// objeto Vulkan criado em initVulkan; createSurface e pickPhysicalDevice
// dependem dela. Aqui entram as extensões de instância e a validação.
void VulkanApp::createInstance()
{
    // Nome e versão só identificam o programa para o driver. apiVersion
    // é a versão mais alta do Vulkan que a aplicação pretende usar (1.0).
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "UV Mapping Dinamico e Procedural";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Sem engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // O núcleo do Vulkan não conhece janelas. A GLFW diz quais extensões
    // de instância a superfície precisa (VK_KHR_surface e a do sistema
    // operacional, como VK_KHR_win32_surface no Windows).
    uint32_t glfwExtensionCount = 0;

    const char** glfwExtensions =
        glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount
        );

    std::vector<const char*> extensions(
        glfwExtensions,
        glfwExtensions + glfwExtensionCount
    );

    // A VK_KHR_portability_subset, que createLogicalDevice liga quando a
    // GPU a oferece (MoltenVK, no macOS), depende desta extensão de
    // instância quando apiVersion é 1.0.
    extensions.push_back(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    );

    // No macOS o Vulkan costuma rodar sobre o Metal pelo MoltenVK, que não
    // cobre a API inteira ("portability"). O loader só lista esse tipo de
    // driver se a instância pedir a extensão e a flag abaixo.
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

        std::cout
            << "Validation layer ativada: erros de uso da API "
            << "aparecem neste terminal."
            << std::endl;
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

// Cria a VkSurfaceKHR, que liga o Vulkan à janela da GLFW. Vem antes de
// pickPhysicalDevice porque findQueueFamilies e querySwapChainSupport
// usam a superfície para testar cada GPU.
void VulkanApp::createSurface()
{
    // A GLFW chama a função de superfície do sistema (no Windows,
    // vkCreateWin32SurfaceKHR). Isso exige uma janela sem contexto
    // OpenGL, por isso initWindow usa GLFW_NO_API.
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
