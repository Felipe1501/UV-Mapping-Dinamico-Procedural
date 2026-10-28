#include "VulkanApp.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    // Outro modelo .glb ou .gltf pode ser passado como argumento.
    const std::string modelPath =
        (argc > 1) ? argv[1] : "assets/models/Duck.glb";

    VulkanApp app(modelPath);

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
