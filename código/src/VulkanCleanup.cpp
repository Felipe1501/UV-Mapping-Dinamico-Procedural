// Encerramento: destrói os objetos Vulkan guardados na classe.

#include "VulkanApp.hpp"

// Libera os objetos Vulkan guardados na classe ao encerrar (ou depois de
// um erro na inicialização). Cada objeto só é destruído se tiver sido
// criado. Quem usa outros objetos sai antes deles, e device, surface e
// instance saem por último.
void VulkanApp::cleanup()
{
    // Espera a GPU terminar o último quadro antes de destruir qualquer coisa.
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

    // O descriptor pool (e os descriptor sets dentro dele) aponta para os
    // Uniform Buffers e para a textura: sai antes deles.
    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(
            device,
            descriptorPool,
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

    // Os framebuffers usam as image views (cor e profundidade):
    // saem antes delas.
    for (VkFramebuffer framebuffer :
         swapChainFramebuffers)
    {
        vkDestroyFramebuffer(
            device,
            framebuffer,
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
