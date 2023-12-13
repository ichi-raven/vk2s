/*****************************************************************/ /**
 * \file   Image.hpp
 * \brief  header file of Image class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_IMAGE_HPP_
#define VK2S_INCLUDE_IMAGE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vk2s
{
    class Device;

    class Image
    {
    public:  // methods
        Image(Device& device, const vk::ImageCreateInfo& ii, vk::MemoryPropertyFlags pbs, const size_t size, vk::ImageAspectFlags aspectFlags);

        ~Image();

        NONCOPYABLE(Image);
        NONMOVABLE(Image);

        void write(const void* pSrc, const size_t size);

        const vk::UniqueImage& getVkImage();

        const vk::UniqueDeviceMemory& getVkDeviceMemory();

        const vk::UniqueImageView& getVkImageView();

        vk::Format getVkFormat() const;

        vk::Extent3D getVkExtent() const;

        vk::ImageAspectFlags getVkAspectFlag() const;

    private:  // member variables
        Device& mDevice;

        vk::UniqueImage mImage;
        vk::UniqueDeviceMemory mMemory;
        vk::UniqueImageView mImageView;
        vk::Extent3D mExtent;
        vk::Format mFormat;
        vk::ImageAspectFlags mAspectFlag;
    };
}  // namespace vk2s

#endif