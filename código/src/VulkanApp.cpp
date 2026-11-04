// Janela, teclado, ordem de inicialização e loop principal.

#include "VulkanApp.hpp"

#include <iostream>
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

// Só guarda o caminho do modelo (.glb ou .gltf). A janela e os objetos
// do Vulkan são criados depois, em run().
VulkanApp::VulkanApp(std::string modelPath)
    : modelPath(std::move(modelPath))
{
}

// Fluxo geral do programa: cria a janela, inicializa o Vulkan, roda o
// loop de desenho e, no fim, chama cleanup() para destruir os objetos.
void VulkanApp::run()
{
    // Se algo falhar no meio (por exemplo, um .spv que não foi encontrado),
    // cleanup() libera os objetos já guardados na classe e o erro segue
    // para main(), que mostra a mensagem. cleanup() só destrói o que existe.
    try
    {
        initWindow();
        initVulkan();
        mainLoop();
    }
    catch (...)
    {
        cleanup();
        throw;
    }

    cleanup();
}

// Cria a janela com o GLFW e registra o teclado. Vem antes do Vulkan
// porque a superfície (createSurface) é criada a partir desta janela.
void VulkanApp::initWindow()
{
    if (!glfwInit())
        throw std::runtime_error("Falha ao inicializar GLFW!");

    // NO_API: o GLFW não cria contexto OpenGL; quem desenha é o Vulkan.
    // Tamanho fixo: assim a swapchain é criada uma vez só, sem precisar
    // ser recriada quando a janela muda de tamanho.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(
        WIDTH,
        HEIGHT,
        "UV Mapping Dinamico e Procedural",
        nullptr,
        nullptr
    );

    if (!window)
        throw std::runtime_error("Falha ao criar janela!");

    // O callback do teclado é uma função estática; o ponteiro guardado na
    // janela é como ele encontra este objeto VulkanApp.
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);

    updateWindowTitle();
}

// Chamada pelo GLFW quando o mainLoop processa os eventos da janela.
// 1 a 5 trocam o modo, Espaço pausa a rotação e Esc fecha a janela.
void VulkanApp::keyCallback(
    GLFWwindow* window,
    int key,
    int /*scancode*/,
    int action,
    int /*mods*/
)
{
    // Só reage ao apertar a tecla (ignora soltar e repetição).
    if (action != GLFW_PRESS)
        return;

    auto* app = static_cast<VulkanApp*>(
        glfwGetWindowUserPointer(window)
    );

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5)
    {
        // Teclas 1 a 5 viram os modos 1 a 5. O valor chega à GPU pelo
        // Uniform Buffer (ubo.mode) no próximo quadro, sem regravar
        // nenhum command buffer.
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

// Mostra o modo atual no título da janela.
void VulkanApp::updateWindowTitle()
{
    const std::string title =
        std::string("UV Mapping Dinamico e Procedural | ") +
        modeNames[renderMode];

    glfwSetWindowTitle(window, title.c_str());
}

// Cria os objetos do Vulkan, cada um depois daqueles de que depende.
// Roda uma vez, antes do loop; se algo falhar, run() chama cleanup().
void VulkanApp::initVulkan()
{
    // Lê o modelo glTF (vértices, índices e textura) para a memória da CPU.
    // Se o carregamento falhar, nenhum objeto do Vulkan chega a ser criado.
    loadMesh();

    // 1. Instância (com a camada de validação, se instalada) e superfície,
    //    que liga o Vulkan à janela do GLFW.
    createInstance();
    createSurface();

    // 2. GPU e dispositivo lógico, com as filas gráfica e de apresentação.
    //    A superfície vem antes porque a escolha da GPU testa se ela
    //    consegue apresentar imagens nessa superfície.
    pickPhysicalDevice();
    createLogicalDevice();

    // 3. Swapchain e suas image views. Daqui saem o formato e o tamanho
    //    das imagens, usados no render pass, no pipeline e nos framebuffers.
    createSwapChain();
    createImageViews();

    // 4. Render pass e descriptor set layout vêm antes do pipeline: ele é
    //    criado para esse render pass, o pipeline layout usa o descriptor
    //    set layout, e é no pipeline que entram os shaders (.spv).
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();

    // 5. Command pool antes de enviar a malha e a textura para a GPU: esses
    //    envios usam command buffers avulsos tirados dela.
    createCommandPool();

    // 6. Depth buffer (no formato escolhido em createRenderPass) e um
    //    framebuffer por imagem, com a image view de cor e a de profundidade.
    createDepthResources();
    createFramebuffers();

    // 7. Dados na GPU: malha e textura (via staging buffer), sampler e um
    //    Uniform Buffer por imagem da swapchain.
    createVertexBuffer();
    createIndexBuffer();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createUniformBuffers();

    // 8. Descriptor sets: apontam os bindings 0 e 1 dos shaders para os
    //    Uniform Buffers e a textura, por isso vêm depois deles.
    createDescriptorPool();
    createDescriptorSets();

    // 9. Por último, os command buffers (gravados com render pass,
    //    pipeline, buffers e descriptor sets já prontos) e os semáforos e
    //    fences do drawFrame.
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

// Chamada por run() depois do initVulkan. Ao sair, a GPU pode ainda estar
// desenhando os últimos quadros: cleanup() espera com vkDeviceWaitIdle
// antes de destruir os objetos.
void VulkanApp::mainLoop()
{
    std::cout
        << "Entrando no loop de renderizacao..."
        << std::endl;

    // Loop principal: trata os eventos da janela (teclado, fechar) e
    // desenha um quadro por volta, até a janela ser fechada.
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Minimizada, a janela fica com tamanho 0x0 e não há imagem para
        // apresentar: espera ela voltar em vez de desenhar.
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
            continue;
        }

        drawFrame();
    }

    std::cout
        << "Loop de renderizacao encerrado."
        << std::endl;
}
