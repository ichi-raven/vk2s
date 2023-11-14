#include "../include/BindGroup.hpp"

#include "../include/Device.hpp"

namespace vkpt
{
    BindGroup::BindGroup(Device& device, BindLayout& layout)
        : mDevice(device)
    {
        mDescriptorSets = mDevice.allocateVkDescriptorSets(layout.getVkDescriptorSetLayouts());

        // TODO: HACK; !!!!!!!!!!!!!!!!!
        mInfoCaches.reserve(16);
    }

    BindGroup::~BindGroup()
    {
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, Buffer& buffer)
    {
        mInfoCaches.emplace_back(vk::DescriptorBufferInfo(buffer.getVkBuffer().get(), buffer.getOffset(), VK_WHOLE_SIZE));

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, {}, std::get<0>(mInfoCaches.back())));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, DynamicBuffer& buffer)
    {
        mInfoCaches.emplace_back(vk::DescriptorBufferInfo(buffer.getVkBuffer().get(), buffer.getOffset(), buffer.getBlockSize()));

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, {}, std::get<0>(mInfoCaches.back())));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, Image& image, Handle<Sampler> sampler)
    {
        // HACK:
        const auto imageLayout = type == vk::DescriptorType::eStorageImage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;

        std::vector<vk::DescriptorImageInfo> infos;
        if (sampler)
        {
            infos.emplace_back(vk::DescriptorImageInfo(sampler->getVkSampler().get(), image.getVkImageView().get(), imageLayout));
        }
        else
        {
            infos.emplace_back(vk::DescriptorImageInfo({}, image.getVkImageView().get(), imageLayout));
        }

        mInfoCaches.emplace_back(infos);

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, std::get<1>(mInfoCaches.back())));
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

        mInfoCaches.emplace_back(infos);
        
        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, type, std::get<1>(mInfoCaches.back())));
    }

    void BindGroup::bind(const uint8_t set, const uint8_t binding, AccelerationStructure& as)
    {
        mInfoCaches.emplace_back(vk::WriteDescriptorSetAccelerationStructureKHR(as.getVkAccelerationStructure().get()));

        mWriteQueue.emplace_back(vk::WriteDescriptorSet(mDescriptorSets[set], binding, 0, vk::DescriptorType::eAccelerationStructureKHR, {}).setDescriptorCount(1).setPNext(&mInfoCaches.back()));
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
}  // namespace vkpt