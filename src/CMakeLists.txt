
include_directories(
  ${CMAKE_SOURCE_DIR}/include/vk2s
  ${GLFW_DIR}/include
  ${STB_DIR}
  ${GLM_DIR}
  ${IMGUI_DIR}
  ${IMGUI_BACKEND_DIR}
  ${SPIRV_REFLECT_DIR}
  ${ASSIMP_INCLUDE_DIR}
  ${Vulkan_INCLUDE_DIRS})

# imgui 
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)
set(IMGUI_SOURCE_FILES
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
)

# Create library
add_library(${LIB_NAME}
Compiler.cpp
AccelerationStructure.cpp
AssetLoader.cpp
Buffer.cpp
BindGroup.cpp
BindLayout.cpp
Camera.cpp
Command.cpp
Device.cpp
DynamicBuffer.cpp
Fence.cpp
Image.cpp
Pipeline.cpp
RenderPass.cpp
Sampler.cpp
Semaphore.cpp
Shader.cpp
ShaderBindingTable.cpp
Window.cpp
${IMGUI_SOURCE_FILES}
${SPIRV_REFLECT_DIR}/spirv_reflect.cpp
)

# link libraries
get_filename_component(VULKAN_LIB_DIR ${Vulkan_LIBRARIES} DIRECTORY)
target_link_directories(${LIB_NAME} PRIVATE ${CMAKE_BINARY_DIR}/lib ${VULKAN_LIB_DIR})

target_link_libraries(${LIB_NAME} 
  PRIVATE
  Vulkan::Vulkan
  Vulkan::glslang
  Vulkan::SPIRV-Tools
  # HACK : CMake's FindVulkan can't find SPIRV-Tools-opt
  optimized SPIRV-Tools-opt debug SPIRV-Tools-optd
  optimized ${GLFW_LIB_NAME} debug ${GLFW_LIB_NAME}${CMAKE_DEBUG_POSTFIX}
  optimized ${ASSIMP_LIB_NAME} debug ${ASSIMP_LIB_NAME}${CMAKE_DEBUG_POSTFIX}
  optimized zlibstatic.lib debug zlibstaticd.lib
)

if(MSVC)
  set_property(TARGET ${LIB_NAME} APPEND PROPERTY LINK_FLAGS "/DEBUG /PROFILE")
endif()
