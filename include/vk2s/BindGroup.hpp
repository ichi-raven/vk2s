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
#include <unordered_map>

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

        void bind(const uint8_t binding, const vk::DescriptorType type, Buffer& buffer);
        void bind(const uint8_t binding, const vk::DescriptorType type, DynamicBuffer& buffer);
        void bind(const uint8_t binding, const vk::DescriptorType type, const vk::ArrayProxy<Handle<Image>>& image, const Handle<Sampler>& sampler = Handle<Sampler>());
        void bind(const uint8_t binding, const vk::DescriptorType type, const vk::ArrayProxy<UniqueHandle<Image>>& image, const Handle<Sampler>& sampler = Handle<Sampler>());

        void bind(const uint8_t binding, AccelerationStructure& as);

        const vk::DescriptorSet& getVkDescriptorSet();

    private:  // types
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, std::vector<vk::DescriptorImageInfo>, vk::WriteDescriptorSetAccelerationStructureKHR>;

        //struct HashPair
        //{
        //    template <class T1, class T2>
        //    size_t operator()(const std::pair<T1, T2>& p) const
        //    {
        //        const auto hash1 = std::hash<T1>{}(p.first);

        //        const auto hash2 = std::hash<T2>{}(p.second);

        //        size_t seed = 0;
        //        seed ^= hash1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        //        seed ^= hash2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        //        return seed;
        //    }
        //};

    private:  // member variables
        Device& mDevice;

        vk::DescriptorSet mDescriptorSet;
        size_t mPoolIndex;
        DescriptorPoolAllocationInfo mAllocationInfo;
        std::unordered_map<uint8_t, DescriptorInfo> mInfoCaches;
        std::vector<vk::WriteDescriptorSet> mWriteQueue;
    };
}  // namespace vk2s

#endif