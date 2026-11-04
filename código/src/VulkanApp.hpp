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

// Quantos quadros a CPU pode preparar enquanto a GPU ainda desenha.
const int MAX_FRAMES_IN_FLIGHT = 2;

// Camada de validação: verifica o uso da API e avisa erros no terminal.
// É ligada só se estiver instalada (ver createInstance).
inline const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Extensão de dispositivo necessária para apresentar imagens na tela.
inline const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Famílias de filas encontradas na GPU: uma que desenha (gráfica) e uma
// que apresenta na janela. Podem ser a mesma família.
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

// O que a superfície (janela) aceita: tamanhos, formatos e modos de
// apresentação. Usado para configurar a swapchain.
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

// A aplicação inteira. As funções estão divididas em arquivos .cpp por
// assunto; os comentários abaixo dizem em qual arquivo cada grupo está.
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

    // Instância, superfície e dispositivo
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    bool validationEnabled = false;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    bool anisotropySupported = false;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Swapchain e imagens que vão para a tela
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat{};

    // Render pass, descriptors e pipeline
    VkRenderPass renderPass = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> descriptorSets;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Comandos
    VkCommandPool commandPool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> commandBuffers;

    // Buffers e textura
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

    // Sincronização (usada em drawFrame, VulkanRenderer.cpp):
    // - imageAvailable (1 por quadro em execução): avisa que a imagem da
    //   swapchain está livre para receber o desenho;
    // - renderFinished (1 por imagem): avisa a apresentação que o desenho
    //   terminou;
    // - inFlightFences (1 por quadro em execução): a CPU espera a GPU
    //   terminar o quadro antes de reutilizar os recursos dele;
    // - imagesInFlight: qual fence está usando cada imagem da swapchain.
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
    // VulkanApp.cpp: janela, teclado, inicialização e loop principal
    void initWindow();
    void initVulkan();
    void mainLoop();

    static void keyCallback(
        GLFWwindow* window,
        int key,
        int scancode,
        int action,
        int mods
    );

    void updateWindowTitle();

    // VulkanInstance.cpp: instância, validação e superfície
    bool checkValidationLayerSupport();

    void createInstance();
    void createSurface();

    // VulkanDevice.cpp: GPU, filas e dispositivo lógico
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

    // VulkanSwapchain.cpp: swapchain, profundidade e framebuffers
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

    VkFormat findDepthFormat();
    void createDepthResources();

    void createFramebuffers();

    // VulkanShaders.cpp: carregamento dos shaders (SPIR-V)
    static std::vector<char> readFile(
        const std::string& filename
    );

    VkShaderModule createShaderModule(
        const std::vector<char>& code
    );

    // VulkanPipeline.cpp: render pass e pipeline gráfico
    void createRenderPass();
    void createGraphicsPipeline();

    // VulkanDescriptors.cpp: descriptor set layout, pool e sets
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

    // VulkanBuffers.cpp: memória, buffers e cópias para a GPU
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

    void loadMesh();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();

    // VulkanTexture.cpp: imagens e textura
    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImage& image,
        VkDeviceMemory& memory
    );

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags
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

    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    // VulkanRenderer.cpp: comandos, sincronização e desenho
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void updateUniformBuffer(
        uint32_t imageIndex
    );

    void drawFrame();

    // VulkanCleanup.cpp: encerramento
    void cleanup();
};
