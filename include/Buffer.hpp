/*****************************************************************/ /**
 * \file   Buffer.hpp
 * \brief  header file of Buffer class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_BUFFER_HPP_
#define VKPT_INCLUDE_BUFFER_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vkpt
{
    class Device;

    class Buffer
    {
    public:  // methods
        Buffer(Device& device, const vk::BufferCreateInfo& bi, vk::MemoryPropertyFlags pbs);

        ~Buffer();

        NONCOPYABLE(Buffer);
        NONMOVABLE(Buffer);

        void write(const void* pSrc, const size_t size, const size_t offset = 0);

        const vk::UniqueBuffer& getVkBuffer();

        const vk::UniqueDeviceMemory& getVkDeviceMemory();

        const vk::DeviceSize getSize() const;

        const vk::DeviceSize getOffset() const;

        const vk::DeviceAddress getVkDeviceAddress() const;

    private:  // member variables
        Device& mDevice;

        vk::UniqueBuffer mBuffer;
        vk::UniqueDeviceMemory mMemory;
        vk::DeviceSize mSize;
        vk::DeviceSize mOffset;
    };
}  // namespace vkpt

#endif