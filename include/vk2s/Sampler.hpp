/*****************************************************************//**
 * @file   Sampler.hpp
 * @brief  header file of Sampler class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_SAMPLER_HPP_
#define VK2S_INCLUDE_SAMPLER_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vk2s
{
    class Device;

    class Sampler
    {
    public:  // methods
        Sampler(Device& device, const vk::SamplerCreateInfo& ci);

        ~Sampler();

        NONCOPYABLE(Sampler);
        NONMOVABLE(Sampler);

         const vk::UniqueSampler& getVkSampler();

    private:  // member variables
         Device& mDevice;

        vk::UniqueSampler mSampler;
    };
}

#endif