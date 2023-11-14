/*****************************************************************/ /**
 * \file   AccelerationStructure.hpp
 * \brief  header file of AccelerationStructure class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_ACCELERATIONSTRUCTURE_HPP_
#define VK2S_INCLUDE_ACCELERATIONSTRUCTURE_HPP_

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
    class Command;

    class AccelerationStructure
    {
    public:  // methods
        // BLAS
        AccelerationStructure(Device& device, const uint32_t vertexNum, const uint32_t vertexStride, Buffer& vertexBuffer, const uint32_t faceNum, Buffer& indexBuffer, Handle<Command> buildCommand = Handle<Command>());

        // TLAS
        AccelerationStructure(Device& device, const vk::ArrayProxy<vk::AccelerationStructureInstanceKHR>& instances, Handle<Command> buildCommand = Handle<Command>());

        ~AccelerationStructure();

        NONCOPYABLE(AccelerationStructure);
        NONMOVABLE(AccelerationStructure);

        const vk::UniqueAccelerationStructureKHR& getVkAccelerationStructure();

        const vk::DeviceAddress getVkDeviceAddress() const;

    private:  // methods
        void build(const vk::AccelerationStructureTypeKHR type, const vk::ArrayProxyNoTemporaries<vk::AccelerationStructureGeometryKHR>& asGeometry,
                     const vk::ArrayProxyNoTemporaries<vk::AccelerationStructureBuildRangeInfoKHR>& asBuildRangeInfo, vk::BuildAccelerationStructureFlagsKHR flags, Handle<Command> buildCommand = Handle<Command>());

    private:  // member variables
        Device& mDevice;

        vk::UniqueAccelerationStructureKHR mAccelerationStructure;
        vk::DeviceAddress mDeviceAddress;
        vk::DeviceSize mSize;

        Handle<Buffer> mBuffer;
        Handle<Buffer> mScratchBuffer;
        Handle<Buffer> mUpdateBuffer;
    };
}  // namespace vk2s

#endif