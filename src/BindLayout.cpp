#include "../include/vk2s/BindLayout.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
{
    BindLayout::BindLayout(Device& device, const vk::ArrayProxy<Handle<Shader>>& shaders)
        : mDevice(device)
        , mInfo(0)
    {
        assert(!shaders.empty() || !"at least one shader is need to reflection!");

        const auto& vkDevice = mDevice.getVkDevice();

        Compiler::ShaderResourceMap table = std::get<1>(shaders.front()->getReflection());

        for (const auto& shader : shaders)
        {
            Compiler::ShaderResourceMap t = std::get<1>(shaders.front()->getReflection());
            table.merge(t);
        }

        auto allBindings = Compiler::createDescriptorSetLayoutBindings(table);
        mDescriptorSetLayouts.reserve(allBindings.size());

        vk::DescriptorSetLayoutCreateInfo descLayoutci;

        for (auto& bindings : allBindings)
        {
            descLayoutci.bindingCount = static_cast<uint32_t>(bindings.size());
            descLayoutci.pBindings    = bindings.data();

            initAllocationInfo(bindings);

            mDescriptorSetLayouts.emplace_back(vkDevice->createDescriptorSetLayout(descLayoutci));
        }
    }

    BindLayout::BindLayout(Device& device, const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings)
        : mDevice(device)
        , mInfo(0)
    {
        vk::DescriptorSetLayoutCreateInfo descLayoutci;

        descLayoutci.bindingCount = static_cast<uint32_t>(bindings.size());
        descLayoutci.pBindings    = bindings.data();

        initAllocationInfo(bindings);

        mDescriptorSetLayouts.emplace_back(mDevice.getVkDevice()->createDescriptorSetLayout(descLayoutci));
    }

    BindLayout::~BindLayout()
    {
        for (auto& layout : mDescriptorSetLayouts)
        {
            mDevice.getVkDevice()->destroyDescriptorSetLayout(layout);
        }
    }

    inline void BindLayout::initAllocationInfo(const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings)
    {
        for (const auto& b : bindings)
        {
            switch (b.descriptorType)
            {
            case vk::DescriptorType::eAccelerationStructureKHR:
                mInfo.accelerationStructureNum += 1;
                break;
            case vk::DescriptorType::eCombinedImageSampler:
                mInfo.combinedImageSamplerNum += 1;
                break;
            case vk::DescriptorType::eStorageBuffer:
                mInfo.storageBufferNum += 1;
                break;
            case vk::DescriptorType::eStorageImage:
                mInfo.storageImageNum += 1;
                break;
            case vk::DescriptorType::eUniformBuffer:
                mInfo.uniformBufferNum += 1;
                break;
            case vk::DescriptorType::eUniformBufferDynamic:
                mInfo.uniformBufferDynamicNum += 1;
                break;
            default:
                assert(!"invalid (or unsupported) descriptor type!");
                break;
            }
        }
    }

    const std::vector<vk::DescriptorSetLayout>& BindLayout::getVkDescriptorSetLayouts()
    {
        return mDescriptorSetLayouts;
    }

    const DescriptorPoolAllocationInfo& BindLayout::getDescriptorPoolAllocationInfo()
    {
        return mInfo;
    }

}  // namespace vk2s