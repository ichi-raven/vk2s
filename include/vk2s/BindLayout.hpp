/*****************************************************************/ /**
 * \file   BindLayout.hpp
 * \brief  header file of BindLayout class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_BINDLAYOUT_HPP_
#define VK2S_INCLUDE_BINDLAYOUT_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"
#include "SlotMap.hpp"
#include "Compiler.hpp"

namespace vk2s
{
    class Device;

    class Shader;

    struct DescriptorPoolAllocationInfo
    {
        DescriptorPoolAllocationInfo()
            : DescriptorPoolAllocationInfo(0)
        {
        }

        DescriptorPoolAllocationInfo(const uint32_t init)
            : accelerationStructureNum(init)
            , combinedImageSamplerNum(init)
            , storageBufferNum(init)
            , storageImageNum(init)
            , uniformBufferNum(init)
            , uniformBufferDynamicNum(init)
        {
        }

        uint32_t sum() const
        {
            return accelerationStructureNum + combinedImageSamplerNum + storageBufferNum + storageImageNum + uniformBufferNum + uniformBufferDynamicNum;
        }

        uint32_t accelerationStructureNum = 0;
        uint32_t combinedImageSamplerNum  = 0;
        uint32_t storageBufferNum         = 0;
        uint32_t storageImageNum          = 0;
        uint32_t uniformBufferNum         = 0;
        uint32_t uniformBufferDynamicNum  = 0;
    };

    class BindLayout
    {
    public:  // methods
        BindLayout(Device& device, const vk::ArrayProxy<Handle<Shader>>& shaders);

        BindLayout(Device& device, const vk::ArrayProxy<const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>>& bindings);

        ~BindLayout();

        NONCOPYABLE(BindLayout);
        NONMOVABLE(BindLayout);

        const std::vector<vk::DescriptorSetLayout>& getVkDescriptorSetLayouts();

        const DescriptorPoolAllocationInfo& getDescriptorPoolAllocationInfo();

    private:  // methods
        inline void initAllocationInfo(const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings);

    private:  // member variables
        Device& mDevice;

        DescriptorPoolAllocationInfo mInfo;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
    };
}  // namespace vk2s

#endif