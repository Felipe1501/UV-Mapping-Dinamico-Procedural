cmake_minimum_required(VERSION 3.20)

project(UVMappingEntrega4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# Bibliotecas header-only baixadas automaticamente na configuração,
# no mesmo esquema de FetchContent usado pela Eruption Engine.
# - glm: matemática (matrizes model/view/projection)
# - tinygltf: leitura de modelos .glb/.gltf (mesma versão da Eruption Engine)
include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "")
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "")
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "")

FetchContent_Declare(
    tinygltf
    URL https://github.com/syoyo/tinygltf/archive/refs/tags/v2.9.3.tar.gz
    URL_HASH SHA256=c34443de2e9afaaf005713f69f5b061101a24ab2e9ee4021d882ac1d7983dbc0
)

FetchContent_MakeAvailable(glm tinygltf)

add_executable(UVMappingEntrega4
    src/main.cpp
    src/VulkanApp.cpp
    src/ModelLoader.cpp
)

target_link_libraries(UVMappingEntrega4
    PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
        tinygltf
)

# O Vulkan usa profundidade de 0 a 1 (o padrão do glm é -1 a 1, do OpenGL).
target_compile_definitions(UVMappingEntrega4
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# Compila os shaders GLSL para SPIR-V a cada build, se o glslc estiver
# disponível. Os .spv ficam em shaders/, onde a aplicação os procura.
# Sem glslc, a aplicação usa os .spv já versionados no repositório.
if(Vulkan_GLSLC_EXECUTABLE)
    set(SHADER_SOURCES
        ${CMAKE_SOURCE_DIR}/shaders/uv_mapping.vert
        ${CMAKE_SOURCE_DIR}/shaders/uv_mapping.frag
    )

    set(SHADER_BINARIES "")

    foreach(SHADER ${SHADER_SOURCES})
        set(SPIRV "${SHADER}.spv")

        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
            DEPENDS ${SHADER}
            COMMENT "Compilando shader ${SHADER}"
            VERBATIM
        )

        list(APPEND SHADER_BINARIES ${SPIRV})
    endforeach()

    add_custom_target(shaders DEPENDS ${SHADER_BINARIES})
    add_dependencies(UVMappingEntrega4 shaders)
else()
    message(STATUS "glslc nao encontrado: usando os .spv ja compilados em shaders/")
endif()
