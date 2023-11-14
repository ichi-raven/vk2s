#include "../include/Shader.hpp"

#include "../include/Device.hpp"

namespace vkpt
{
    Shader::Shader(Device& device, std::string_view path, std::string_view entryPoint)
        : mDevice(device)
        , mEntryPoint(entryPoint)
    {
        const auto code = Compiler::compileFile(path);

        vk::ShaderModuleCreateInfo createInfo({}, code.size() * sizeof(code[0]), reinterpret_cast<const uint32_t*>(code.data()));

        mShaderModule = mDevice.getVkDevice()->createShaderModuleUnique(createInfo);

        mReflection = Compiler::getReflection(code);
    }

    Shader::~Shader()
    {
    }

    const vk::UniqueShaderModule& Shader::getVkShaderModule()
    {
        return mShaderModule;
    }

    const std::string& Shader::getEntryPoint() const
    {
        return mEntryPoint;
    }

    const Compiler::ReflectionResult& Shader::getReflection()
    {
        return *mReflection;
    }
}  // namespace vkpt
