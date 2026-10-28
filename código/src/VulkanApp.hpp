#pragma once

#include "ModelLoader.hpp"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const
    {
        return graphicsFamily.has_value() &&
               presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Modos de visualização, trocados pelas teclas 1 a 5.
enum RenderMode : int32_t
{
    MODE_TEXTURE = 1,       // textura original aplicada pelas UVs do modelo
    MODE_DYNAMIC_UV = 2,    // UVs animadas antes de amostrar a textura
    MODE_PROCEDURAL = 3,    // padrão procedural calculado a partir das UVs
    MODE_BLEND = 4,         // textura com UV dinâmica + padrão procedural
    MODE_UV_GRID = 5        // grade de depuração da parametrização UV
};

// Dados enviados aos shaders.
// O layout precisa espelhar o bloco "Parameters" dos shaders em std140:
// as três mat4 ocupam os primeiros 192 bytes, "time" fica no offset 192
// e "mode" no offset 196. Os static_assert abaixo garantem isso.
// O padding mantém o tamanho do bloco compatível com o alinhamento esperado.
struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    float time;
    int32_t mode;
    float padding[2];
};

static_assert(offsetof(UniformBufferObject, view) == 64, "layout std140");
static_assert(offsetof(UniformBufferObject, projection) == 128, "layout std140");
static_assert(offsetof(UniformBufferObject, time) == 192, "layout std140");
static_assert(offsetof(UniformBufferObject, mode) == 196, "layout std140");

class VulkanApp
{
public:
    explicit VulkanApp(std::string modelPath);

    void run();

private:
    std::string modelPath;

    MeshData mesh;
    uint32_t indexCount = 0;

    GLFWwindow* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    bool validationEnabled = false;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    bool anisotropySupported = false;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat{};

    VkRenderPass renderPass = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> descriptorSets;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> commandBuffers;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    std::vector<VkFence> imagesInFlight;

    size_t currentFrame = 0;

    // Estado controlado pelo teclado.
    int32_t renderMode = MODE_BLEND;
    bool rotating = true;
    float rotationAngle = 0.0f;
    float lastFrameTime = 0.0f;

private:
    void initWindow();
    void initVulkan();

    static void keyCallback(
        GLFWwindow* window,
        int key,
        int scancode,
        int action,
        int mods
    );

    void updateWindowTitle();

    bool checkValidationLayerSupport();

    void createInstance();
    void createSurface();

    QueueFamilyIndices findQueueFamilies(
        VkPhysicalDevice deviceToCheck
    );

    bool checkDeviceExtensionSupport(
        VkPhysicalDevice deviceToCheck
    );

    SwapChainSupportDetails querySwapChainSupport(
        VkPhysicalDevice deviceToCheck
    );

    bool isDeviceSuitable(
        VkPhysicalDevice deviceToCheck
    );

    void pickPhysicalDevice();
    void createLogicalDevice();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats
    );

    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& modes
    );

    VkExtent2D chooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities
    );

    void createSwapChain();
    void createImageViews();

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags
    );

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImage& image,
        VkDeviceMemory& memory
    );

    VkFormat findDepthFormat();
    void createDepthResources();

    void createRenderPass();
    void createDescriptorSetLayout();

    VkShaderModule createShaderModule(
        const std::vector<char>& code
    );

    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();

    uint32_t findMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties
    );

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& memory
    );

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(
        VkCommandBuffer commandBuffer
    );

    void copyBuffer(
        VkBuffer source,
        VkBuffer destination,
        VkDeviceSize size
    );

    void uploadToDeviceLocalBuffer(
        const void* data,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer& buffer,
        VkDeviceMemory& memory
    );

    void transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );

    void copyBufferToImage(
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height
    );

    void loadMesh();
    void createVertexBuffer();
    void createIndexBuffer();

    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();

    void updateUniformBuffer(
        uint32_t imageIndex
    );

    void createCommandBuffers();
    void createSyncObjects();

    void drawFrame();
    void mainLoop();
    void cleanup();
};
