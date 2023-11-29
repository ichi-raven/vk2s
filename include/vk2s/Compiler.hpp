/*****************************************************************/ /**
 * @file   Compiler.hpp
 * @brief  functions that compiles GLSL to SPIR-V binalies
 * 
 * @author ichi-raven
 * @date   October 2023
 *********************************************************************/

#ifndef VK2S_COMPILER_HPP_
#define VK2S_COMPILER_HPP_

#include <vulkan/vulkan.hpp>

#include <shaderc/shaderc.hpp>

#include <vector>
#include <map>

namespace vk2s
{
    namespace Compiler
    {
        using SPIRVCode = std::vector<std::uint32_t>;

        constexpr auto kSpirvVersion = shaderc_spirv_version_1_6;
        constexpr auto kVulkanEnvVersion = shaderc_env_version_vulkan_1_3;

        std::string readFile(std::string_view path);

        shaderc_shader_kind getShaderStage(std::string_view filepath);

        SPIRVCode compileText(shaderc_shader_kind stage, const std::string& glslShader);

        SPIRVCode compileFile(std::string_view path, const bool optimize = false);

        // SPIRV-Reflect (vertex layout, descriptor set layout bindings)
        using VertexInputAttributes = std::vector<vk::VertexInputAttributeDescription>;
        using ShaderResourceMap     = std::map<std::pair<uint32_t, uint32_t>, vk::DescriptorType>;
        using ReflectionResult      = std::tuple<VertexInputAttributes, ShaderResourceMap>;
        ReflectionResult getReflection(const SPIRVCode& fileData);

        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> createDescriptorSetLayoutBindings(const ShaderResourceMap& resourceMap);

        uint32_t getSizeOfFormat(const vk::Format format) noexcept;

    }  // namespace Compiler
}  // namespace vk2s

#endif