#include "../include/vk2s/BindLayout.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
{
    BindLayout::BindLayout(Device& device, const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings)
        : mDevice(device)
        , mInfo(0)
    {
        vk::DescriptorSetLayoutCreateInfo descLayoutci;

        descLayoutci.bindingCount = static_cast<uint32_t>(bindings.size());
        descLayoutci.pBindings    = bindings.data();

        initAllocationInfo(bindings);

        mDescriptorSetLayout = mDevice.getVkDevice()->createDescriptorSetLayoutUnique(descLayoutci);
    }

    BindLayout::~BindLayout()
    {

    }

    inline void BindLayout::initAllocationInfo(const vk::ArrayProxy<const vk::DescriptorSetLayoutBinding>& bindings)
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
            case vk::DescriptorType::eSampledImage:
                mInfo.sampledImageNum += 1;
                break;
            case vk::DescriptorType::eSampler:
                mInfo.samplerNum += 1;
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

    const vk::UniqueDescriptorSetLayout& BindLayout::getVkDescriptorSetLayout()
    {
        return mDescriptorSetLayout;
    }

    const DescriptorPoolAllocationInfo& BindLayout::getDescriptorPoolAllocationInfo()
    {
        return mInfo;
    }

}  // namespace vk2s