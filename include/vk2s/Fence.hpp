/*****************************************************************//**
 * @file   Fence.hpp
 * @brief  header file of Fence class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_FENCE_HPP_
#define VK2S_INCLUDE_FENCE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vk2s
{
    class Device;

    class Fence
    {
    public:  // methods
        Fence(Device& device, const bool signaled = true);

        ~Fence();

        NONCOPYABLE(Fence);
        NONMOVABLE(Fence);

        bool wait();

        void reset();

        bool signaled();

        const vk::UniqueFence& getVkFence();

    private:  // member variables
         Device& mDevice;

        vk::UniqueFence mFence;
    };
}

#endif