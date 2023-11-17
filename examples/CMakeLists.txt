
set(APP_NAME "examples")

include_directories(
  ${CMAKE_SOURCE_DIR}/include
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


# Create executable
add_executable(${APP_NAME}
main.cpp
pathtracing.cpp
rasterize.cpp
raytracing.cpp
)

# link libraries
get_filename_component(VULKAN_LIB_DIR ${Vulkan_LIBRARIES} DIRECTORY)
target_link_directories(${APP_NAME} PRIVATE ${CMAKE_BINARY_DIR}/lib ${VULKAN_LIB_DIR})

target_link_libraries(${APP_NAME} 
  PRIVATE
  Vulkan::Vulkan
  Vulkan::glslang
  Vulkan::SPIRV-Tools
  # HACK : CMake's FindVulkan can't find SPIRV-Tools-opt
  optimized SPIRV-Tools-opt debug SPIRV-Tools-optd
  optimized ${GLFW_LIB_NAME} debug ${GLFW_LIB_NAME}${CMAKE_DEBUG_POSTFIX}
  optimized ${ASSIMP_LIB_NAME} debug ${ASSIMP_LIB_NAME}${CMAKE_DEBUG_POSTFIX}
  optimized zlibstatic.lib debug zlibstaticd.lib
  ${LIB_NAME}
)

if(MSVC)
  set_property(TARGET ${APP_NAME} APPEND PROPERTY LINK_FLAGS "/DEBUG /PROFILE")
endif()
