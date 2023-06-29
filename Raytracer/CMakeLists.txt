# CMakeList.txt : CMake project for Raytracer, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Raytracer "Raytracer.cpp" "Raytracer.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Raytracer PROPERTY CXX_STANDARD 20)
endif()

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(Raytracer PUBLIC  glfw3dll)

# https://cmake.org/cmake/help/latest/module/FindVulkan.html
find_package(Vulkan REQUIRED)
target_link_libraries(Raytracer PUBLIC  Vulkan::Vulkan)

include_directories(${Vulkan_INCLUDE_DIRS})
target_include_directories(Raytracer PUBLIC ../External)
