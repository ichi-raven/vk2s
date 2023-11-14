/*****************************************************************/ /**
 * \file   ShaderBindingTable.hpp
 * \brief  header file of ShaderBindingTable class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_SHADERBINDINGTABLE_HPP_
#define VK2S_INCLUDE_SHADERBINDINGTABLE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"
#include "SlotMap.hpp"

namespace vk2s
{
    class Device;
    class Buffer;
    class Pipeline;

    class ShaderBindingTable
    {
    public: // types

        struct VkShaderBindingTableInfo
        {
            vk::StridedDeviceAddressRegionKHR rgen = {};
            vk::StridedDeviceAddressRegionKHR miss = {};
            vk::StridedDeviceAddressRegionKHR hit  = {};
            vk::StridedDeviceAddressRegionKHR callable = {};
        };

    public:  // methods

        ShaderBindingTable(Device& device, Pipeline& rayTracePipeline, const uint32_t raygenNum, const uint32_t missNum, const uint32_t hitNum, const uint32_t callableNum, const vk::ArrayProxyNoTemporaries<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups);

        ~ShaderBindingTable();

        NONCOPYABLE(ShaderBindingTable);
        NONMOVABLE(ShaderBindingTable);

        const VkShaderBindingTableInfo& getVkSBTInfo() const;

    private: // methods
        inline vk::PhysicalDeviceRayTracingPipelinePropertiesKHR getRayTracingPipelineProperties() const;

    private:  // member variables
        Device& mDevice;

        VkShaderBindingTableInfo mSBTInfo;
        Handle<Buffer> mShaderBindingTable;
    };
}  // namespace vk2s

#endif