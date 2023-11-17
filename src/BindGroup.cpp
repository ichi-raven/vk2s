#include "../include/BindGroup.hpp"

#include "../include/Device.hpp"

namespace vk2s
{
    BindGroup::BindGroup(Device& device, BindLayout& layout)
        : mDevice(device)
    {
        // get mAllocationInfo from BindLayout

        const auto&& [descriptorSets, poolIndex] = mDevice.allocateVkDescriptorSets(layout.getVkDescriptorSetLayouts(), );

        mDescriptorSets = descriptorSets;
        mPoolIndex      = poolIndex;

        // HACK:
        mInfoCaches.reserve(16);
    }

    BindGroup::~BindGroup()
    {
        mDevice.deallocateVkDescriptorSets(mDescriptorSets, mPoolIndex, mAllocationInfo);
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, Buffer& buffer)
    {
        const auto& info = mInfoCaches.emplace(std::make_pair(set, binding), vk::DescriptorBufferInfo(buffer.getVkBuffer().get(), buffer.getOffset(), VK_WHOLE_SIZE)).first->second;

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, {}, std::get<0>(info)));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, DynamicBuffer& buffer)
    {
        const auto& info = mInfoCaches.emplace(std::make_pair(set, binding), vk::DescriptorBufferInfo(buffer.getVkBuffer().get(), buffer.getOffset(), buffer.getBlockSize())).first->second;

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, {}, std::get<0>(info)));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, const vk::ArrayProxyNoTemporaries<Handle<Image>>& images, Handle<Sampler> sampler)
    {
        assert(!images.empty() || !"images must be greater than 0");

        // HACK;
        const auto imageLayout = type == vk::DescriptorType::eStorageImage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;

        std::vector<vk::DescriptorImageInfo> infos;
        infos.reserve(images.size());

        for (const auto& image : images)
        {
            const auto vkImageView = image->getVkImageView().get();
            if (sampler)
            {
                infos.emplace_back(vk::DescriptorImageInfo(sampler->getVkSampler().get(), vkImageView, imageLayout));
            }
            else
            {
                infos.emplace_back(vk::DescriptorImageInfo({}, vkImageView, imageLayout));
            }
        }

        const auto& info = mInfoCaches.emplace(std::make_pair(set, binding), infos).first->second;
        
        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, std::get<1>(info)));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, AccelerationStructure& as)
    {
        const auto& info = mInfoCaches.emplace(std::make_pair(set, binding), vk::WriteDescriptorSetAccelerationStructureKHR(as.getVkAccelerationStructure().get())).first->second;

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, vk::DescriptorType::eAccelerationStructureKHR, {}).setDescriptorCount(1).setPNext(&info));
    }

    const std::vector<vk::DescriptorSet>& BindGroup::getVkDescriptorSets()
    {
        if (!mWriteQueue.empty())
        {
            const auto& vkDevice = mDevice.getVkDevice();
            vkDevice->updateDescriptorSets(mWriteQueue, {});

            mWriteQueue.clear();
            mInfoCaches.clear();
        }

        return mDescriptorSets;
    }
}  // namespace vk2s