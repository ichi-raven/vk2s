/*****************************************************************/ /**
 * \file   BindLayout.hpp
 * \brief  header file of BindLayout class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_BINDLAYOUT_HPP_
#define VKPT_INCLUDE_BINDLAYOUT_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"
#include "SlotMap.hpp"
#include "Compiler.hpp"

namespace vkpt
{
    class Device;

    class Shader;

    class BindLayout
    {
    public:  // methods
        BindLayout(Device& device, const vk::ArrayProxy<Handle<Shader>>& shaders);

        BindLayout(Device& device, const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings);

        ~BindLayout();

        NONCOPYABLE(BindLayout);
        NONMOVABLE(BindLayout);

        const std::vector<vk::DescriptorSetLayout>& getVkDescriptorSetLayouts();

    private:  // methods
    private:  // member variables
        Device& mDevice;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
    };
}  // namespace vkpt

#endif