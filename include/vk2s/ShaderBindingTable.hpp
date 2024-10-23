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

#include <functional>
#include <optional>

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

        struct RegionInfo
        {
            uint32_t shaderTypeNum;
            uint32_t entryNum;
            std::size_t additionalEntrySize; // size required in addition to normal shader entries
            std::function<void(std::byte* pDst, std::byte* pHandleStart, const uint32_t handleSize, const uint32_t alignedHandleSize)>
                entryWriter;  // Function to write additional entry information to the table (dst is a pointer to the destination)
        };

    public:  // methods

        ShaderBindingTable(Device& device, Pipeline& rayTracePipeline, const uint32_t raygenNum, const uint32_t missNum, const uint32_t hitNum, const uint32_t callableNum, const vk::ArrayProxyNoTemporaries<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups);

        ShaderBindingTable(Device& device, Pipeline& rayTracePipeline, const RegionInfo& raygenShaderInfo, const RegionInfo& missShaderInfo, const RegionInfo& hitShaderInfo, const RegionInfo& callableShaderInfo,
                           const vk::ArrayProxyNoTemporaries<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups);

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