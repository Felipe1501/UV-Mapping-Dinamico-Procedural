// Comandos, sincronização e desenho de cada quadro.

#include "VulkanApp.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Cria a command pool, de onde sai a memória dos command buffers. Vem
// antes do envio da malha e da textura para a GPU, porque os command
// buffers avulsos de beginSingleTimeCommands também saem dela.
void VulkanApp::createCommandPool()
{
    QueueFamilyIndices indices =
        findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo info{};

    info.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // A pool fica presa a uma família de filas: os command buffers dela só
    // podem ser enviados a filas dessa família. Aqui é a gráfica, que
    // recebe tanto o desenho quanto os comandos da inicialização.
    info.queueFamilyIndex =
        indices.graphicsFamily.value();

    // Sem flags: os command buffers de desenho são gravados uma vez só e
    // nunca reiniciados, então a pool dispensa RESET_COMMAND_BUFFER_BIT.
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

// Monta os dados do quadro (matrizes, tempo e modo) e copia para o Uniform
// Buffer da imagem atual. Chamada pelo drawFrame a cada quadro; os shaders
// leem esses dados no bloco "Parameters" (set 0, binding 0).
void VulkanApp::updateUniformBuffer(
    uint32_t imageIndex
)
{
    // Segundos desde a primeira chamada. Vira ubo.time, que o fragment
    // shader usa para animar a UV dinâmica e o padrão procedural.
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

    // Mesmo layout do bloco "Parameters" dos shaders (std140); os
    // static_assert em VulkanApp.hpp conferem os offsets.
    UniformBufferObject ubo{};

    // model: gira o modelo em torno do eixo Y. Como é só rotação, o vertex
    // shader pode usar mat3(model) também para girar as normais.
    ubo.model = glm::rotate(
        glm::mat4(1.0f),
        rotationAngle,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // view: leva a cena para o ponto de vista da câmera, que fica um pouco
    // acima e afastada no eixo Z, olhando para a origem.
    ubo.view = glm::lookAt(
        glm::vec3(0.0f, 0.6f, 3.6f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // projection: perspectiva com campo de visão vertical de 45°, na
    // proporção da swapchain. A profundidade sai de 0 a 1, como o Vulkan
    // espera, graças ao GLM_FORCE_DEPTH_ZERO_TO_ONE (CMakeLists.txt).
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

    // time e mode só são usados pelo fragment shader; os valores de mode
    // (1 a 5) são os mesmos das constantes MODE_* do uv_mapping.frag.
    ubo.time = time;
    ubo.mode = renderMode;

    // O buffer fica mapeado desde a criação e é HOST_COHERENT: basta o
    // memcpy, sem vkFlushMappedMemoryRanges. A fence da imagem, esperada
    // no drawFrame, garante que a GPU não está lendo este buffer agora.
    std::memcpy(
        uniformBuffersMapped[imageIndex],
        &ubo,
        sizeof(ubo)
    );
}

// Aloca um command buffer por imagem da swapchain e grava nele, uma vez
// só, todos os comandos de desenho. O que muda a cada quadro fica no
// Uniform Buffer, então o drawFrame só reenvia o buffer já gravado.
void VulkanApp::createCommandBuffers()
{
    // Um por framebuffer, porque cada gravação fixa em qual imagem o
    // render pass desenha (renderPassInfo.framebuffer, mais abaixo).
    commandBuffers.resize(
        swapChainFramebuffers.size()
    );

    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.commandPool =
        commandPool;

    // PRIMARY: é enviado direto para a fila. Um SECONDARY só pode ser
    // executado de dentro de um primary.
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
        // flags fica 0: sem ONE_TIME_SUBMIT, o buffer pode ser enviado em
        // vários quadros; sem SIMULTANEOUS_USE, não pode ser reenviado
        // enquanto executa, e por isso o drawFrame espera a fence da imagem.
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

        // Valores de limpeza, na ordem dos anexos do render pass: 0 = cor
        // (fundo azul-escuro) e 1 = profundidade. São usados porque os dois
        // anexos têm loadOp CLEAR.
        std::array<VkClearValue, 2> clearValues{};

        clearValues[0].color = {{
            0.04f,
            0.04f,
            0.10f,
            1.0f
        }};

        // Profundidade 1.0 = o ponto mais distante possível.
        // Com o teste LESS do pipeline, vence o fragmento mais próximo.
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

        // Área afetada pelo render pass: a imagem inteira da swapchain.
        renderPassInfo.renderArea.offset =
            {0, 0};

        renderPassInfo.renderArea.extent =
            swapChainExtent;

        renderPassInfo.clearValueCount =
            static_cast<uint32_t>(clearValues.size());

        renderPassInfo.pClearValues =
            clearValues.data();

        // Início do render pass: os anexos saem de UNDEFINED para os layouts
        // do subpass e são limpos. INLINE = os comandos ficam neste mesmo
        // command buffer, sem command buffers secundários.
        vkCmdBeginRenderPass(
            commandBuffers[i],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Liga o pipeline gráfico: shaders uv_mapping.vert e .frag, formato
        // dos vértices, rasterização e teste de profundidade.
        vkCmdBindPipeline(
            commandBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline
        );

        // Vertex buffer no binding 0, o mesmo descrito no pipeline. Dele o
        // uv_mapping.vert lê as locations 0, 1 e 2 (posição, normal e UV).
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

        // Os índices dizem quais vértices formam cada triângulo. UINT32
        // porque a malha guarda os índices como uint32_t.
        vkCmdBindIndexBuffer(
            commandBuffers[i],
            indexBuffer,
            0,
            VK_INDEX_TYPE_UINT32
        );

        // Set 0 dos shaders = descriptorSets[i]: binding 0 é o Uniform Buffer
        // "Parameters" da imagem i e binding 1 é o sampler2D baseTexture.
        // Por isso o drawFrame grava no Uniform Buffer de índice imageIndex.
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

        // Desenha a malha toda: indexCount índices, 1 instância, a partir do
        // índice 0, sem deslocamento de vértice nem de instância.
        vkCmdDrawIndexed(
            commandBuffers[i],
            indexCount,
            1,
            0,
            0,
            0
        );

        // Fim do render pass: a imagem de cor passa para PRESENT_SRC_KHR (o
        // finalLayout), pronta para ser apresentada.
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

// Cria os semáforos e as fences do drawFrame. Semáforos ordenam as
// operações da GPU entre si (aquisição, desenho, apresentação), sem a CPU
// esperar; fences deixam a CPU esperar a GPU terminar um quadro.
void VulkanApp::createSyncObjects()
{
    imageAvailableSemaphores.resize(
        MAX_FRAMES_IN_FLIGHT
    );

    // renderFinished é um por imagem (não por quadro): no drawFrame, a
    // apresentação de cada imagem espera o semáforo com o índice dela.
    renderFinishedSemaphores.resize(
        swapChainImages.size()
    );

    inFlightFences.resize(
        MAX_FRAMES_IN_FLIGHT
    );

    // Antes do primeiro quadro nenhuma imagem está em uso: todas começam
    // sem fence (VK_NULL_HANDLE), e o drawFrame não espera por elas.
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

    // A fence nasce sinalizada: assim o primeiro vkWaitForFences do
    // drawFrame não trava esperando um quadro que nunca foi enviado.
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

// Desenha um quadro. A CPU pode preparar até MAX_FRAMES_IN_FLIGHT quadros
// enquanto a GPU ainda desenha os anteriores; fences e semáforos garantem
// a ordem certa (Seção 4.7 do PDF).
void VulkanApp::drawFrame()
{
    // 1. Espera a GPU terminar o quadro que usou este "slot" da última vez.
    if (vkWaitForFences(
            device,
            1,
            &inFlightFences[currentFrame],
            VK_TRUE,
            UINT64_MAX
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao esperar a fence do quadro!"
        );
    }

    // 2. Pede a próxima imagem da swapchain. imageAvailable será
    //    sinalizado quando a imagem estiver livre para receber o desenho.
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

    // 3. Se esta imagem ainda está sendo desenhada por um quadro anterior,
    //    espera. Isso protege o Uniform Buffer desta imagem, que vai ser
    //    reescrito agora (há um Uniform Buffer por imagem).
    if (imagesInFlight[imageIndex] !=
        VK_NULL_HANDLE)
    {
        if (vkWaitForFences(
                device,
                1,
                &imagesInFlight[imageIndex],
                VK_TRUE,
                UINT64_MAX
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao esperar a fence da imagem!"
            );
        }
    }

    imagesInFlight[imageIndex] =
        inFlightFences[currentFrame];

    // 4. Grava matrizes, tempo e modo no Uniform Buffer desta imagem.
    updateUniformBuffer(imageIndex);

    // 5. Envia o command buffer desta imagem (gravado uma vez, no início).
    //    Ele espera imageAvailable só na etapa de escrita da cor e, ao
    //    terminar, sinaliza renderFinished e a fence do quadro.

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

    // A fence só volta para "não sinalizada" aqui, logo antes do envio
    // que vai sinalizá-la de novo.
    if (vkResetFences(
            device,
            1,
            &inFlightFences[currentFrame]
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao resetar a fence do quadro!"
        );
    }

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

    // 6. Mostra a imagem na tela. A apresentação espera renderFinished,
    //    ou seja, só acontece depois que o desenho terminou.
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

    // Passa para o próximo slot (0, 1, 0, 1, ...).
    currentFrame =
        (currentFrame + 1) %
        MAX_FRAMES_IN_FLIGHT;
}
