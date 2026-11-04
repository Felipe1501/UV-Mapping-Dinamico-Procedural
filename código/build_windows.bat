@echo off
REM Compila o projeto no Windows (ambiente testado):
REM Visual Studio 2022 Build Tools (MSVC), CMake e Ninja que vem com ele,
REM e as bibliotecas instaladas pelo vcpkg em deps\vcpkg (ver README.md).
REM Uso: rode este arquivo dentro da pasta do codigo.

set ROOT=%~dp0.
set VCPKG=%~dp0deps\vcpkg
set INSTALLED=%VCPKG%\installed\x64-windows

set VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
set CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

REM Prepara o terminal para o compilador do Visual Studio (64 bits).
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

REM Configura: o toolchain do vcpkg faz o CMake achar Vulkan e GLFW,
REM e o glslc do shaderc compila os shaders para SPIR-V.
"%CMAKE%" -S "%ROOT%" -B "%ROOT%\build" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DVCPKG_INSTALLED_DIR="%VCPKG%\installed" ^
  -DVulkan_GLSLC_EXECUTABLE="%INSTALLED%\tools\shaderc\glslc.exe" || exit /b 1

REM Compila os shaders e o programa.
"%CMAKE%" --build "%ROOT%\build" || exit /b 1

echo.
echo Build OK: %ROOT%\build\UVMapping.exe
