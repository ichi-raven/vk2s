/*****************************************************************/ /**
 * \file   BindGroup.hpp
 * \brief  header file of BindGroup class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_BINDGROUP_HPP_
#define VK2S_INCLUDE_BINDGROUP_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "BindLayout.hpp"
#include "Macro.hpp"
#include "SlotMap.hpp"
#include "Compiler.hpp"

#include <variant>
#include <map>

namespace vk2s
{
    class Device;

    class AccelerationStructure;
    class BindLayout;
    class Buffer;
    class DynamicBuffer;
    class Image;
    class Sampler;

    class BindGroup
    {
    public:  // methods
        BindGroup(Device& device, BindLayout& layout);

        ~BindGroup();

        NONCOPYABLE(BindGroup);
        NONMOVABLE(BindGroup);

        void bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, Buffer& buffer);
        void bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, DynamicBuffer& buffer);
        void bind(const uint8_t set, const uint8_t binding, const vk::DescriptorType type, const vk::ArrayProxyNoTemporaries<Handle<Image>>& image, Handle<Sampler> sampler = Handle<Sampler>());
        void bind(const uint8_t set, const uint8_t binding, AccelerationStructure& as);

        const std::vector<vk::DescriptorSet>& getVkDescriptorSets();

    private:  // types
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, std::vector<vk::DescriptorImageInfo>, vk::WriteDescriptorSetAccelerationStructureKHR>;

    private:  // member variables
        Device& mDevice;

        std::vector<vk::DescriptorSet> mDescriptorSets;
        size_t mPoolIndex;
        DescriptorPoolAllocationInfo mAllocationInfo;
        std::map<std::pair<uint8_t, uint8_t>, DescriptorInfo> mInfoCaches;
        std::vector<vk::WriteDescriptorSet> mWriteQueue;
    };
}  // namespace vk2s

#endif