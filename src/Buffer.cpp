#include "../include/vk2s/Buffer.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
{
    Buffer::Buffer(Device& device, const vk::BufferCreateInfo& bi, vk::MemoryPropertyFlags pbs)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        mBuffer = vkDevice->createBufferUnique(bi);

        // for ray tracing
        vk::MemoryAllocateFlagsInfo allocateFlagsInfo(vk::MemoryAllocateFlagBits::eDeviceAddress);

        vk::MemoryRequirements reqs = vkDevice->getBufferMemoryRequirements(mBuffer.get());
        vk::MemoryAllocateInfo ai(reqs.size, mDevice.getVkMemoryTypeIndex(reqs.memoryTypeBits, pbs));
        if (bi.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            ai.pNext = &allocateFlagsInfo;
        }

        mMemory = vkDevice->allocateMemoryUnique(ai);
        mSize   = bi.size;
        mOffset = 0;

        vkDevice->bindBufferMemory(mBuffer.get(), mMemory.get(), mOffset);
    }

    Buffer::~Buffer()
    {
    }

    void Buffer::write(const void* pSrc, const size_t size, const size_t offset)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        void* p = vkDevice->mapMemory(mMemory.get(), offset, size);
        assert(p || !"failed to map memory!");
        memcpy(p, pSrc, size);
        vkDevice->unmapMemory(mMemory.get());
    }

    const vk::UniqueBuffer& Buffer::getVkBuffer()
    {
        return mBuffer;
    }

    const vk::UniqueDeviceMemory& Buffer::getVkDeviceMemory()
    {
        return mMemory;
    }

    const vk::DeviceSize Buffer::getSize() const
    {
        return mSize;
    }

    const vk::DeviceSize Buffer::getOffset() const
    {
        return mOffset;
    }

    const vk::DeviceAddress Buffer::getVkDeviceAddress() const
    {
        return mDevice.getVkDevice()->getBufferAddress(vk::BufferDeviceAddressInfo(mBuffer.get()));
    }

}  // namespace vk2s