// Render pass e pipeline gráfico: liga os shaders aos seus estágios e
// define entrada de vértices, rasterização, profundidade e cor.

#include "VulkanApp.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

// Cria o render pass: quais imagens o desenho usa (cor e profundidade),
// o que fazer com elas no início e no fim, e em que layout ficam. Vem
// depois da swapchain; o pipeline e os framebuffers são criados para ele.
void VulkanApp::createRenderPass()
{
    // Attachment 0: a imagem da swapchain, que recebe a cor do desenho.
    // O formato tem de ser o mesmo das imagens da swapchain.
    VkAttachmentDescription colorAttachment{};

    colorAttachment.format =
        swapChainImageFormat;

    // Uma amostra por pixel: sem multisampling (MSAA).
    colorAttachment.samples =
        VK_SAMPLE_COUNT_1_BIT;

    // CLEAR: no início, a imagem é pintada com a cor de fundo
    // (clearValues[0] em createCommandBuffers). STORE: o resultado é
    // guardado, porque vai ser mostrado na tela.
    colorAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR;

    colorAttachment.storeOp =
        VK_ATTACHMENT_STORE_OP_STORE;

    // Imagem de cor não tem stencil.
    colorAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    colorAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // UNDEFINED: o conteúdo anterior pode ser descartado, já que a imagem
    // é limpa. PRESENT_SRC_KHR: no fim do render pass, a imagem fica no
    // layout que vkQueuePresentKHR (em drawFrame) exige.
    colorAttachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    colorAttachment.finalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // O formato de profundidade é escolhido aqui porque o render pass é
    // criado antes da imagem de profundidade (createDepthResources usa
    // este mesmo depthFormat).
    depthFormat =
        findDepthFormat();

    // Attachment 1: o depth buffer, usado no teste de profundidade.
    VkAttachmentDescription depthAttachment{};

    depthAttachment.format =
        depthFormat;

    depthAttachment.samples =
        VK_SAMPLE_COUNT_1_BIT;

    // CLEAR: cada quadro começa com profundidade 1.0, a mais distante
    // (clearValues[1]). DONT_CARE: a profundidade não é usada depois do
    // desenho, então não precisa ser guardada.
    depthAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR;

    depthAttachment.storeOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // O stencil não é usado.
    depthAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    depthAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // A imagem de profundidade só é usada neste render pass: começa sem
    // conteúdo útil (UNDEFINED) e termina no layout de profundidade.
    depthAttachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Referências usadas pelo subpass: "attachment" é a posição no array
    // attachments, e "layout" é o layout da imagem durante o subpass. O
    // render pass faz as transições sozinho, sem vkCmdPipelineBarrier.
    VkAttachmentReference colorRef{};

    colorRef.attachment = 0;

    colorRef.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};

    depthRef.attachment = 1;

    depthRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Um único subpass, gráfico (não de compute), que desenha o modelo
    // usando a cor e a profundidade.
    VkSubpassDescription subpass{};

    subpass.pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    subpass.colorAttachmentCount = 1;

    // pColorAttachments[0] é onde o uv_mapping.frag escreve, com
    // "layout(location = 0) out vec4 outColor".
    subpass.pColorAttachments =
        &colorRef;

    subpass.pDepthStencilAttachment =
        &depthRef;

    // Dependência de EXTERNAL (os comandos enviados antes à mesma fila,
    // inclusive os do quadro anterior) para o subpass 0.
    VkSubpassDependency dependency{};

    dependency.srcSubpass =
        VK_SUBPASS_EXTERNAL;

    dependency.dstSubpass = 0;

    // Antes, precisa terminar: a espera de imageAvailable (drawFrame a faz
    // em COLOR_ATTACHMENT_OUTPUT) e a escrita de profundidade do quadro
    // anterior (LATE_FRAGMENT_TESTS), já que o depth buffer é um só.
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    // Só a profundidade tem srcAccessMask: ela foi escrita pelo quadro
    // anterior. Na cor, a apresentação só leu a imagem, e depois de uma
    // leitura basta esperar.
    dependency.srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Depois, esperam: a transição de layout e as escritas do subpass,
    // inclusive os clears (o da cor em COLOR_ATTACHMENT_OUTPUT e o da
    // profundidade em EARLY_FRAGMENT_TESTS).
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // A ordem deste array define os índices 0 (cor) e 1 (profundidade).
    // Os framebuffers e os clearValues seguem a mesma ordem.
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

// Cria o pipeline gráfico: junta os dois shaders e todo o estado fixo do
// desenho. Vem depois do render pass e do descriptor set layout, que ele
// usa; createCommandBuffers liga este pipeline antes de desenhar.
void VulkanApp::createGraphicsPipeline()
{
    // Shaders já compilados para SPIR-V pelo glslc (ver CMakeLists.txt).
    // O caminho é relativo à pasta de execução, que precisa ter shaders/.
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

    // Cada estágio diz qual shader module roda em qual etapa do pipeline.
    VkPipelineShaderStageCreateInfo vertStage{};

    vertStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    // Etapa de vértices: o uv_mapping.vert aplica model, view e projection
    // à posição e passa a UV e a normal para o fragment shader.
    vertStage.stage =
        VK_SHADER_STAGE_VERTEX_BIT;

    vertStage.module =
        vertModule;

    // Ponto de entrada: a função main() do shader. O campo existe porque
    // um módulo SPIR-V pode ter mais de uma função de entrada.
    vertStage.pName =
        "main";

    VkPipelineShaderStageCreateInfo fragStage{};

    fragStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    // Etapa de fragmentos: o uv_mapping.frag calcula a cor de cada
    // fragmento (pixel) coberto pelo modelo, conforme o modo escolhido.
    fragStage.stage =
        VK_SHADER_STAGE_FRAGMENT_BIT;

    fragStage.module =
        fragModule;

    fragStage.pName =
        "main";

    // Só vertex e fragment: tessellation e geometry não são usados.
    VkPipelineShaderStageCreateInfo stages[] = {
        vertStage,
        fragStage
    };

    // Entrada de vértices: preenchida mais abaixo, depois do binding e
    // dos atributos.
    VkPipelineVertexInputStateCreateInfo vertexInput{};

    // Montagem: cada 3 índices do index buffer formam um triângulo
    // separado. O ModelLoader.cpp só aceita primitivas de triângulos.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    inputAssembly.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Primitive restart (recomeçar a primitiva com um índice especial)
    // só vale para strips e fans; numa lista fica desligado.
    inputAssembly.primitiveRestartEnable =
        VK_FALSE;

    // Um único vertex buffer (binding 0), com posição, normal e UV de cada
    // vértice lado a lado (struct Vertex, em ModelLoader.hpp). O
    // createCommandBuffers liga o vertexBuffer neste binding 0.
    VkVertexInputBindingDescription binding{};

    binding.binding = 0;

    // stride: quantos bytes pular para chegar ao próximo vértice.
    binding.stride =
        sizeof(Vertex);

    // VERTEX: avança um Vertex a cada vértice (INSTANCE avançaria a cada
    // instância desenhada).
    binding.inputRate =
        VK_VERTEX_INPUT_RATE_VERTEX;

    // Cada location aqui é a mesma do uv_mapping.vert. O formato diz
    // quantos floats ler (R, G, B são só nomes de canal), e o offset diz
    // onde o atributo começa dentro da struct Vertex.
    std::array<
        VkVertexInputAttributeDescription,
        3
    > attributes{};

    // location 0 = inPosition: vec3, por isso R32G32B32_SFLOAT.
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format =
        VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset =
        offsetof(Vertex, position);

    // location 1 = inNormal: também vec3.
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format =
        VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset =
        offsetof(Vertex, normal);

    // location 2 = inUV: vec2, por isso só R32G32. É a coordenada de
    // textura que o modelo traz para cada vértice.
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

    // Viewport: leva o resultado dos shaders para a imagem inteira da
    // swapchain. minDepth fica 0 (zerado pelo {}) e maxDepth 1: a
    // profundidade gravada no depth buffer vai de 0 a 1.
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

    // Scissor do tamanho da imagem: nenhum pixel é recortado.
    VkRect2D scissor{};

    scissor.extent =
        swapChainExtent;

    // Viewport e scissor ficam fixos no pipeline (não há estado dinâmico).
    // Isso funciona porque a janela tem tamanho fixo (GLFW_RESIZABLE
    // desligado em initWindow).
    VkPipelineViewportStateCreateInfo viewportState{};

    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    viewportState.viewportCount = 1;

    viewportState.pViewports =
        &viewport;

    viewportState.scissorCount = 1;

    viewportState.pScissors =
        &scissor;

    // Rasterização: transforma cada triângulo nos fragmentos (pixels)
    // que ele cobre na imagem.
    VkPipelineRasterizationStateCreateInfo rasterizer{};

    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    // FALSE: o que passa dos planos near e far é recortado, em vez de ter
    // a profundidade presa nesses limites.
    rasterizer.depthClampEnable =
        VK_FALSE;

    // TRUE descartaria os triângulos antes de rasterizar (nada na tela).
    rasterizer.rasterizerDiscardEnable =
        VK_FALSE;

    // FILL preenche os triângulos (LINE desenharia só as arestas).
    rasterizer.polygonMode =
        VK_POLYGON_MODE_FILL;

    // Só vale para linhas; sem o recurso wideLines, tem de ser 1.0.
    rasterizer.lineWidth =
        1.0f;

    // Faces de trás do modelo não são desenhadas. O glTF define as faces
    // da frente em sentido anti-horário, e a inversão do eixo Y feita na
    // matriz de projeção mantém esse sentido na tela.
    rasterizer.cullMode =
        VK_CULL_MODE_BACK_BIT;

    rasterizer.frontFace =
        VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Teste de profundidade: cada fragmento é comparado com o valor do
    // depth buffer, para que o que está na frente esconda o que está atrás.
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    depthStencil.depthTestEnable =
        VK_TRUE;

    // Quem passa no teste grava a sua profundidade no depth buffer.
    depthStencil.depthWriteEnable =
        VK_TRUE;

    // LESS: passa o fragmento mais perto (menor profundidade) que o valor
    // guardado. O buffer começa em 1.0, o mais distante (clearValues[1]).
    depthStencil.depthCompareOp =
        VK_COMPARE_OP_LESS;

    // Uma amostra por pixel (sem MSAA), igual aos attachments do render
    // pass.
    VkPipelineMultisampleStateCreateInfo multisampling{};

    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    multisampling.rasterizationSamples =
        VK_SAMPLE_COUNT_1_BIT;

    // Escrita da cor no attachment: grava os quatro canais (R, G, B, A).
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    // Sem blending: a cor do uv_mapping.frag (com alfa 1.0) substitui o
    // pixel, sem misturar com o que já estava na imagem.
    colorBlendAttachment.blendEnable =
        VK_FALSE;

    // Um estado de blend por attachment de cor do subpass (aqui, só 1).
    VkPipelineColorBlendStateCreateInfo colorBlending{};

    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    // Sem operações lógicas (bit a bit) entre a cor nova e a antiga.
    colorBlending.logicOpEnable =
        VK_FALSE;

    colorBlending.attachmentCount =
        1;

    colorBlending.pAttachments =
        &colorBlendAttachment;

    // Pipeline layout: diz quais recursos os shaders acessam. O set 0 é o
    // descriptorSetLayout (VulkanDescriptors.cpp): binding 0 = Uniform
    // Buffer "Parameters"; binding 1 = sampler2D baseTexture (só no .frag).
    VkPipelineLayoutCreateInfo layoutInfo{};

    layoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // A posição em pSetLayouts é o número do set nos shaders (set = 0).
    // Não há push constants.
    layoutInfo.setLayoutCount =
        1;

    layoutInfo.pSetLayouts =
        &descriptorSetLayout;

    // Os shader modules são variáveis locais, que o cleanup() não
    // conhece: em caso de erro, eles são destruídos aqui antes do throw.
    if (vkCreatePipelineLayout(
            device,
            &layoutInfo,
            nullptr,
            &pipelineLayout
        ) != VK_SUCCESS)
    {
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);

        throw std::runtime_error(
            "Falha ao criar Pipeline Layout!"
        );
    }

    // Junta tudo em um só objeto: estágios, estados fixos, layout e
    // render pass. Sem pDynamicState, nada disso muda depois de criado.
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

    // O pipeline é feito para o subpass 0 deste render pass, e só pode
    // ser usado com render passes compatíveis com ele.
    pipelineInfo.renderPass =
        renderPass;

    pipelineInfo.subpass = 0;

    // VK_NULL_HANDLE: sem pipeline cache (ele pode acelerar a criação).
    // A função pode criar vários pipelines de uma vez; aqui, só 1.
    const VkResult result =
        vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &graphicsPipeline
        );

    // O pipeline guarda o seu próprio código compilado: os shader modules
    // não são mais necessários, com ou sem sucesso na criação.
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

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao criar Graphics Pipeline!"
        );
    }

    std::cout
        << "Graphics Pipeline criada."
        << std::endl;
}
