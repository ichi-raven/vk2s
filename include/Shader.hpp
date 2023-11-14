/*****************************************************************/ /**
 * \file   Shader.hpp
 * \brief  header file of Shader class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_SHADER_HPP_
#define VKPT_INCLUDE_SHADER_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "SlotMap.hpp"
#include "Compiler.hpp"
#include "Macro.hpp"

#include <optional>

namespace vkpt
{
    class Device;

    class Shader
    {
    public:  // methods
        Shader(Device& device, std::string_view path, std::string_view entryPoint);

        ~Shader();

        NONCOPYABLE(Shader);
        NONMOVABLE(Shader);

        const vk::UniqueShaderModule& getVkShaderModule();

        const std::string& getEntryPoint() const;

        const Compiler::ReflectionResult& getReflection();

    private:  // methods
    private:  // member variables
        Device& mDevice;

        vk::UniqueShaderModule mShaderModule;
        std::string mEntryPoint;
        std::optional<Compiler::ReflectionResult> mReflection;
    };
}  // namespace vkpt

#endif