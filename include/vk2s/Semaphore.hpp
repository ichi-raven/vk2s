/*****************************************************************//**
 * @file   Semaphore.hpp
 * @brief  header file of Semaphore class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_SEMAPHORE_HPP_
#define VK2S_INCLUDE_SEMAPHORE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vk2s
{
    class Device;

    class Semaphore
    {
    public:  // methods
        Semaphore(Device& device);

        ~Semaphore();

        NONCOPYABLE(Semaphore);
        NONMOVABLE(Semaphore);

         const vk::UniqueSemaphore& getVkSemaphore();

    private:  // member variables
         Device& mDevice;

        vk::UniqueSemaphore mSemaphore;
    };
}

#endif