/*****************************************************************//**
 * @file   Sampler.hpp
 * @brief  header file of Sampler class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_SAMPLER_HPP_
#define VKPT_INCLUDE_SAMPLER_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"

namespace vkpt
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