cmake_minimum_required(VERSION 3.20)
project(brep_shader_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Classic-mode vcpkg prefix from repo bootstrap:
#   vcpkg install shaderc:x64-windows --classic
set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/../third_party/vcpkg/installed/x64-windows")
list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED}")

find_package(unofficial-shaderc CONFIG REQUIRED)

add_executable(compile_shaders compile_shaders.cpp)
target_link_libraries(compile_shaders PRIVATE unofficial::shaderc::shaderc)
