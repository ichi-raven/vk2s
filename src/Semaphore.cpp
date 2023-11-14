#include "../include/Semaphore.hpp"

#include "../include/Device.hpp"

namespace vkpt
{
    Semaphore::Semaphore(Device& device)
        : mDevice(device)
    {
        mSemaphore = mDevice.getVkDevice()->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
    }

    Semaphore::~Semaphore()
    {

    }

    const vk::UniqueSemaphore& Semaphore::getVkSemaphore()
    {
        return mSemaphore;
    }

}  // namespace vkpt