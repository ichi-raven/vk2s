cmake_minimum_required(VERSION 3.22.0 FATAL_ERROR)
project(vk2s LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Options
set(LIB_NAME "vk2s")

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)

# Macro
#add_definitions("-D_USE_MATH_DEFINES")
add_definitions("-D_CRT_SECURE_NO_WARNINGS")

# Compiler flags
if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
  set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -O2")
elseif (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /Zi")
  # add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  # add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
  set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4099")
endif()

# Vulkan
find_package(Vulkan REQUIRED SPIRV-Tools shaderc_combined)

# Output directories
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
endif()

if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
endif()

if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
endif()

# Traverse subdirectories
set(CMAKE_DEBUG_POSTFIX "-debug")

# directories
set(GLFW_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW)
set(STB_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/stb)
set(GLM_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glm)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui)
set(IMGUI_BACKEND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui/backends)
set(SPIRV_REFLECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SPIRV-Reflect)
set(ASSIMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Assimp)
set(ASSIMP_INCLUDE_DIR ${ASSIMP_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/Assimp/include)
set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinygltf)

# GLFW
add_subdirectory(${GLFW_DIR})
set(GLFW_LIB_NAME glfw3)

# Assimp
add_subdirectory(${ASSIMP_DIR})
set(ASSIMP_LIB_NAME assimp-vc143-mt)

# slang
#set(SLANG_VERSION "2024.13")
set(SLANG_VERSION "2025.12")
# set install dir
set(SLANG_INSTALL_DIR "./ThirdParty/slang")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${SLANG_INSTALL_DIR}")

find_package(slang QUIET)

if (NOT slang_FOUND)

    file(GLOB SLANG_FILES "${SLANG_INSTALL_DIR}/*")
    set(SLANG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/slang)

    if (NOT SLANG_FILES)
        message(STATUS "Slang not found. Downloading and installing it...")
        
        if (WIN32)
            set(SLANG_ZIP_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-windows-x86_64.zip")
        elseif (UNIX)
            set(SLANG_ZIP_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-linux-x86_64.zip")
        else()
            message(FATAL_ERROR "Unsupported OS.")
        endif()

      set(SLANG_ZIP_FILE ./ThirdParty/slang_tmp.zip)

      message("URL: ${SLANG_ZIP_URL}")
      file(DOWNLOAD ${SLANG_ZIP_URL} ${SLANG_ZIP_FILE})
      message("file: ${SLANG_ZIP_FILE}")

      set (DLLEXT ".dll")
      if (WIN32)
          execute_process(
              COMMAND powershell -Command "Expand-Archive -Path '${SLANG_ZIP_FILE}' -DestinationPath '${SLANG_DIR}' -Force"
          )

      elseif (UNIX)
          execute_process(
              COMMAND unzip ${SLANG_ZIP_FILE} -d ${SLANG_DIR}
          )
          set (DLLEXT ".so")
      endif()

      file(REMOVE ${SLANG_ZIP_FILE})

      message(STATUS "Downloaded and extracted Slang to: ${SLANG_DIR}")

      if (result)
          message(FATAL_ERROR "Failed to download and install Slang.")
      endif()

    endif()

elseif(NOT slang_FOUND)
    get_filename_component(SLANG_DIR ${Slang_INCLUDE_DIRS} DIRECTORY)
endif()

# set DLLs
set (DLLEXT ".dll")
if (UNIX)
          execute_process(
              COMMAND unzip ${SLANG_ZIP_FILE} -d ${SLANG_DIR}
          )
          set (DLLEXT ".so")
      endif()
file(GLOB DLL_FILES "${SLANG_DIR}/bin/*${DLLEXT}")

foreach(DLL ${DLL_FILES})
  file(COPY ${DLL} DESTINATION ${CMAKE_BINARY_DIR}/bin)
  message("copied ${DLL} to ${CMAKE_BINARY_DIR}/bin")
endforeach()

set(SLANG_LIB_NAME slang)

add_subdirectory(src)

# examples
if (BUILD_EXAMPLES)
  message("build with examples")
  add_subdirectory(examples)
endif()