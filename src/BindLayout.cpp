#include "../include/BindLayout.hpp"

#include "../include/Device.hpp"

namespace vk2s
{
    BindLayout::BindLayout(Device& device, const vk::ArrayProxy<Handle<Shader>>& shaders)
        : mDevice(device)
    {
        assert(!shaders.empty() || !"at least one shader is need to reflection!");

        const auto& vkDevice = mDevice.getVkDevice();

        auto table = std::get<1>(shaders.front()->getReflection());

        for (const auto& shader : shaders)
        {
            auto t = std::get<1>(shaders.front()->getReflection());
            table.merge(t);
        }

        auto allBindings = Compiler::createDescriptorSetLayoutBindings(table);
        mDescriptorSetLayouts.reserve(allBindings.size());

        vk::DescriptorSetLayoutCreateInfo descLayoutci;

        for (auto& bindings : allBindings)
        {
            descLayoutci.bindingCount = static_cast<uint32_t>(bindings.size());
            descLayoutci.pBindings    = bindings.data();

            mDescriptorSetLayouts.emplace_back(vkDevice->createDescriptorSetLayout(descLayoutci));
        }
    }

    BindLayout::BindLayout(Device& device, const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings)
        : mDevice(device)
    {
        vk::DescriptorSetLayoutCreateInfo descLayoutci;

        descLayoutci.bindingCount = static_cast<uint32_t>(bindings.size());
        descLayoutci.pBindings    = bindings.data();

        mDescriptorSetLayouts.emplace_back(mDevice.getVkDevice()->createDescriptorSetLayout(descLayoutci));
    }

    BindLayout::~BindLayout()
    {
        for (auto& layout : mDescriptorSetLayouts)
        {
            mDevice.getVkDevice()->destroyDescriptorSetLayout(layout);
        }
    }

    const std::vector<vk::DescriptorSetLayout>& BindLayout::getVkDescriptorSetLayouts()
    {
        return mDescriptorSetLayouts;
    }
}  // namespace vk2s