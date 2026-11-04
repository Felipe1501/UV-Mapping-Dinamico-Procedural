// Memória e buffers: vertex, index e uniform buffers, e as cópias da
// CPU para a memória da GPU.

#include "VulkanApp.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

// Escolhe um tipo de memória da GPU que sirva para o recurso e tenha as
// propriedades pedidas. Usada por createBuffer e por createImage.
uint32_t VulkanApp::findMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
)
{
    // A GPU informa os seus tipos de memória e as propriedades de cada um
    // (por exemplo DEVICE_LOCAL ou HOST_VISIBLE).
    VkPhysicalDeviceMemoryProperties memProperties{};

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice,
        &memProperties
    );

    for (uint32_t i = 0;
         i < memProperties.memoryTypeCount;
         ++i)
    {
        // typeFilter vem de memoryTypeBits: se o bit i está ligado, o recurso
        // aceita o tipo de memória i. O tipo também precisa ter todas as
        // propriedades pedidas (pode ter outras a mais).
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

// Cria um buffer e a memória dele. No Vulkan os dois são objetos
// separados: cria o buffer, aloca a memória e depois liga um ao outro.
// Usada para os staging, vertex, index e uniform buffers.
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

    // usage declara para que o buffer vai servir (vértices, índices,
    // uniform, origem ou destino de cópia). Usar de outro jeito é erro.
    bufferInfo.usage =
        usage;

    // EXCLUSIVE: só uma família de filas usa o buffer. Cópias e desenho vão
    // para a graphicsQueue; a fila de apresentação não toca nos buffers.
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

    // O driver informa o tamanho real (pode ser maior que size, por
    // alinhamento) e, em memoryTypeBits, os tipos de memória aceitos.
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

    // O último argumento é o offset dentro da memória: o buffer começa no
    // início da alocação.
    if (vkBindBufferMemory(
            device,
            buffer,
            memory,
            0
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao associar memoria ao Buffer!"
        );
    }
}

// Cria e inicia um command buffer para um comando avulso (cópia ou
// barreira), fora do desenho dos quadros. Usada por copyBuffer,
// transitionImageLayout e copyBufferToImage.
VkCommandBuffer VulkanApp::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    // PRIMARY: pode ser enviado direto para a fila com vkQueueSubmit.
    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    // Vem da mesma command pool do desenho, que é da família gráfica.
    // Por isso ele é enviado para a graphicsQueue.
    allocInfo.commandPool =
        commandPool;

    allocInfo.commandBufferCount =
        1;

    VkCommandBuffer commandBuffer =
        VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(
            device,
            &allocInfo,
            &commandBuffer
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao alocar Command Buffer temporario!"
        );
    }

    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // ONE_TIME_SUBMIT: avisa o driver que este command buffer vai ser
    // enviado uma vez só (endSingleTimeCommands libera logo depois).
    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(
            commandBuffer,
            &beginInfo
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao iniciar Command Buffer temporario!"
        );
    }

    return commandBuffer;
}

// Par de beginSingleTimeCommands: fecha o command buffer avulso, envia
// para a graphicsQueue, espera a execução terminar e depois o libera.
void VulkanApp::endSingleTimeCommands(
    VkCommandBuffer commandBuffer
)
{
    if (vkEndCommandBuffer(
            commandBuffer
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao finalizar Command Buffer temporario!"
        );
    }

    VkSubmitInfo submitInfo{};

    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount =
        1;

    submitInfo.pCommandBuffers =
        &commandBuffer;

    // Comandos de cópia avulsos: envia e espera a fila esvaziar, já que
    // só acontecem na inicialização.
    // Essa espera (vkQueueWaitIdle) faz o papel de uma fence: quando ela
    // volta, a fila terminou tudo. Por isso uploadToDeviceLocalBuffer e
    // createTextureImage podem apagar o staging buffer logo em seguida.
    if (vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            VK_NULL_HANDLE
        ) != VK_SUCCESS ||
        vkQueueWaitIdle(
            graphicsQueue
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao executar comandos de copia!"
        );
    }

    // Pode liberar já: a fila terminou de usar este command buffer.
    vkFreeCommandBuffers(
        device,
        commandPool,
        1,
        &commandBuffer
    );
}

// Copia size bytes de um buffer para outro, na GPU. É usada por
// uploadToDeviceLocalBuffer para levar os dados do staging ao buffer final.
void VulkanApp::copyBuffer(
    VkBuffer source,
    VkBuffer destination,
    VkDeviceSize size
)
{
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands();

    // srcOffset e dstOffset ficam em 0: copia do início de um buffer para
    // o início do outro.
    VkBufferCopy region{};

    region.size =
        size;

    // Aqui o comando só é gravado. Ele roda na GPU quando
    // endSingleTimeCommands envia o command buffer para a fila.
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

    // Staging buffer: só serve de origem da cópia (TRANSFER_SRC).
    // HOST_VISIBLE deixa a CPU mapear essa memória, e HOST_COHERENT dispensa
    // vkFlushMappedMemoryRanges depois do memcpy.
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    // Mapear dá à CPU um ponteiro para a memória do staging buffer.
    void* mapped = nullptr;

    if (vkMapMemory(
            device,
            stagingMemory,
            0,
            size,
            0,
            &mapped
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Falha ao mapear memoria do staging buffer!"
        );
    }

    std::memcpy(
        mapped,
        data,
        static_cast<size_t>(size)
    );

    // Os dados só são copiados uma vez, então já pode desmapear.
    vkUnmapMemory(
        device,
        stagingMemory
    );

    // Buffer final: destino da cópia (TRANSFER_DST) mais o uso real
    // (vértices ou índices). DEVICE_LOCAL é a memória mais rápida para a
    // GPU, mas pode não ser acessível pela CPU; daí o staging buffer.
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

    // copyBuffer só volta depois que a fila terminou a cópia, então o
    // staging buffer não é mais usado e pode ser destruído.
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

// Lê o modelo glTF para a CPU (vértices, índices e textura). É o primeiro
// passo de initVulkan; a cópia na CPU é descartada no fim de initVulkan.
void VulkanApp::loadMesh()
{
    mesh =
        loadModel(modelPath);

    // Quantos índices vkCmdDrawIndexed desenha: 3 por triângulo, já que a
    // topologia do pipeline é TRIANGLE_LIST.
    indexCount =
        static_cast<uint32_t>(mesh.indices.size());
}

// Envia os vértices para um buffer na memória local da GPU. É o buffer
// que createCommandBuffers liga com vkCmdBindVertexBuffers.
void VulkanApp::createVertexBuffer()
{
    // Cada Vertex tem 8 floats (32 bytes). O pipeline lê deste buffer as
    // locations 0, 1 e 2 do uv_mapping.vert (posição, normal e UV), como
    // descrito em createGraphicsPipeline.
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

// Envia os índices para a GPU, logo depois do vertex buffer. Cada 3
// índices formam um triângulo, e um mesmo vértice pode ser reaproveitado
// por vários triângulos.
void VulkanApp::createIndexBuffer()
{
    // O tipo uint32_t precisa combinar com VK_INDEX_TYPE_UINT32, usado em
    // vkCmdBindIndexBuffer (createCommandBuffers).
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

// Cria um Uniform Buffer por imagem da swapchain para o bloco Parameters
// (set 0, binding 0). Cada quadro grava só o buffer da imagem que vai
// desenhar (updateUniformBuffer), e não o de outra imagem ainda em uso.
void VulkanApp::createUniformBuffers()
{
    // Mesmo layout do bloco Parameters em std140 (ver UniformBufferObject
    // em VulkanApp.hpp).
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
        // Sem staging: a CPU reescreve este buffer a cada quadro, então a
        // memória é HOST_VISIBLE. Com HOST_COHERENT, o memcpy basta, sem
        // vkFlushMappedMemoryRanges.
        createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i],
            uniformBuffersMemory[i]
        );

        // Fica mapeado o tempo todo: a cada quadro a CPU só faz um memcpy.
        // O vkUnmapMemory só acontece em cleanup().
        if (vkMapMemory(
                device,
                uniformBuffersMemory[i],
                0,
                size,
                0,
                &uniformBuffersMapped[i]
            ) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Falha ao mapear Uniform Buffer!"
            );
        }
    }

    std::cout
        << "Uniform Buffers criados."
        << std::endl;
}
