/*****************************************************************//**
 * @file   Image.cpp
 * @brief  
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#include "../include/vk2s/Image.hpp"

#include "../include/vk2s/Device.hpp"

#include <stb_image.h>

namespace vk2s
{

    Image::Image(Device& device, const vk::ImageCreateInfo& ii, vk::MemoryPropertyFlags pbs, const size_t size, vk::ImageAspectFlags aspectFlags)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        mImage                      = vkDevice->createImageUnique(ii);
        vk::MemoryRequirements reqs = vkDevice->getImageMemoryRequirements(mImage.get());
        vk::MemoryAllocateInfo ai(reqs.size, mDevice.getVkMemoryTypeIndex(reqs.memoryTypeBits, pbs));
        mMemory = vkDevice->allocateMemoryUnique(ai);

        vkDevice->bindImageMemory(mImage.get(), mMemory.get(), 0);

        vk::ImageViewCreateInfo viewInfo;
        viewInfo.image                           = mImage.get();
        viewInfo.viewType                        = vk::ImageViewType::e2D;
        viewInfo.format                          = ii.format;
        viewInfo.subresourceRange.aspectMask     = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        mImageView = vkDevice->createImageViewUnique(viewInfo);

        mFormat = ii.format;

        mExtent     = ii.extent;
        mAspectFlag = aspectFlags;
    }

    Image::~Image()
    {
    }

    void Image::write(const void* pSrc, const size_t size)
    {
        vk::BufferCreateInfo bi({}, size, vk::BufferUsageFlagBits::eTransferSrc);
        vk::MemoryPropertyFlags fb(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        UniqueHandle<Buffer> stagingBuffer = mDevice.create<Buffer>(bi, fb);
        stagingBuffer->write(pSrc, size);

        {  // transfer
            UniqueHandle<Command> command = mDevice.create<Command>();

            command->begin(true);
            command->transitionImageLayout(*this, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            command->copyBufferToImage(stagingBuffer.get(), *this, mExtent.width, mExtent.height);
            command->transitionImageLayout(*this, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            command->end();

            command->execute();

            mDevice.getVkDevice()->waitIdle();
        }
    }

    void Image::loadFromFile(std::string_view path)
    {
        int width = 0, height = 0, bpp = 0;
        void* pData = reinterpret_cast<void*>(stbi_load(path.data(), &width, &height, &bpp, STBI_rgb_alpha));
        write(pData, static_cast<size_t>(width * height * bpp));
        stbi_image_free(pData);
    }


    const vk::UniqueImage& Image::getVkImage()
    {
        return mImage;
    }

    const vk::UniqueDeviceMemory& Image::getVkDeviceMemory()
    {
        return mMemory;
    }

    const vk::UniqueImageView& Image::getVkImageView()
    {
        return mImageView;
    }

    vk::Format Image::getVkFormat() const
    {
        return mFormat;
    }

    vk::Extent3D Image::getVkExtent() const
    {
        return mExtent;
    }

    vk::ImageAspectFlags Image::getVkAspectFlag() const
    {
        return mAspectFlag;
    }

}  // namespace vk2s