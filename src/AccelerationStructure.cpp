#include "../include/AccelerationStructure.hpp"

#include "../include/Device.hpp"

namespace vkpt
{
    // BLAS
    AccelerationStructure::AccelerationStructure(Device& device, const uint32_t vertexNum, const uint32_t vertexStride, Buffer& vertexBuffer, const uint32_t faceNum, Buffer& indexBuffer, Handle<Command> buildCommand)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::AccelerationStructureGeometryKHR geometryInfo;
        geometryInfo.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        geometryInfo.geometryType = vk::GeometryTypeKHR::eTriangles;

        {
            vk::BufferDeviceAddressInfo VBdeviceAddressInfo(vertexBuffer.getVkBuffer().get());
            vk::BufferDeviceAddressInfo IBdeviceAddressInfo(indexBuffer.getVkBuffer().get());

            auto& triangles                    = geometryInfo.geometry.triangles;
            triangles.vertexFormat             = vk::Format::eR32G32B32Sfloat;
            triangles.vertexData.deviceAddress = vkDevice->getBufferAddress(VBdeviceAddressInfo);
            triangles.maxVertex                = vertexNum;
            triangles.vertexStride             = vertexStride;
            triangles.indexType                = vk::IndexType::eUint32;
            triangles.indexData.deviceAddress  = vkDevice->getBufferAddress(IBdeviceAddressInfo);
        }

        vk::AccelerationStructureBuildRangeInfoKHR asBuildRangeInfo(faceNum);

        std::array geometries = { geometryInfo };
        std::array ranges     = { asBuildRangeInfo };

        build(vk::AccelerationStructureTypeKHR::eBottomLevel, geometries, ranges, {}, buildCommand);

        mDevice.destroy(mScratchBuffer);
    }

    // TLAS
    AccelerationStructure::AccelerationStructure(Device& device, const vk::ArrayProxy<vk::AccelerationStructureInstanceKHR>& instances, Handle<Command> buildCommand)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        const auto instancesBufferSize   = sizeof(vk::AccelerationStructureInstanceKHR) * instances.size();
        const vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        const auto hostMemProps          = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::BufferCreateInfo ci({}, instancesBufferSize, usage);

        UniqueHandle<Buffer> instanceBuffer = mDevice.create<Buffer>(ci, hostMemProps);
        instanceBuffer->write(instances.data(), instancesBufferSize);

        vk::BufferDeviceAddressInfo instanceBufferDeviceAddressInfo(instanceBuffer->getVkBuffer().get());
        const auto address = vkDevice->getBufferAddress(instanceBufferDeviceAddressInfo);
        vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress(address);

        vk::AccelerationStructureGeometryKHR asGeometry;
        asGeometry.geometryType       = vk::GeometryTypeKHR::eInstances;
        asGeometry.flags              = vk::GeometryFlagBitsKHR::eOpaque;
        asGeometry.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR(VK_FALSE, instanceDataDeviceAddress);

        vk::AccelerationStructureBuildRangeInfoKHR asBuildRangeInfo(static_cast<uint32_t>(instances.size()), 0, 0, 0);

        std::array geometries = { asGeometry };
        std::array ranges     = { asBuildRangeInfo };

        build(vk::AccelerationStructureTypeKHR::eTopLevel, geometries, ranges, {});

        mDevice.destroy(mScratchBuffer);
    }

    AccelerationStructure ::~AccelerationStructure()
    {
    }

    const vk::UniqueAccelerationStructureKHR& AccelerationStructure::getVkAccelerationStructure()
    {
        return mAccelerationStructure;
    }

    const vk::DeviceAddress AccelerationStructure::getVkDeviceAddress() const
    {
        return mDeviceAddress;
    }

    void AccelerationStructure::build(const vk::AccelerationStructureTypeKHR type, const vk::ArrayProxyNoTemporaries<vk::AccelerationStructureGeometryKHR>& asGeometry,
                                      const vk::ArrayProxyNoTemporaries<vk::AccelerationStructureBuildRangeInfoKHR>& asBuildRangeInfo, vk::BuildAccelerationStructureFlagsKHR flags, Handle<Command> buildCommand)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::AccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo;
        asBuildGeometryInfo.type  = type;
        asBuildGeometryInfo.mode  = vk::BuildAccelerationStructureModeKHR::eBuild;
        asBuildGeometryInfo.flags = flags;

        asBuildGeometryInfo.geometryCount = 1;
        asBuildGeometryInfo.pGeometries   = asGeometry.data();

        std::vector<uint32_t> numPrimitives;
        numPrimitives.reserve(asBuildRangeInfo.size());
        for (const auto& buildRangeInfo : asBuildRangeInfo)
        {
            numPrimitives.emplace_back(buildRangeInfo.primitiveCount);
        }

        const auto asBuildSizesInfo = vkDevice->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, asBuildGeometryInfo, numPrimitives);

        // allocate accleration structure
        vk::BufferUsageFlags asUsage     = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vk::MemoryPropertyFlags memProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::BufferCreateInfo ci({}, asBuildSizesInfo.accelerationStructureSize, asUsage);

        mBuffer = mDevice.create<Buffer>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mSize   = asBuildSizesInfo.accelerationStructureSize;

        vk::AccelerationStructureCreateInfoKHR asCreateInfo;
        asCreateInfo.buffer = mBuffer->getVkBuffer().get();
        asCreateInfo.size   = mSize;
        asCreateInfo.type   = asBuildGeometryInfo.type;

        // create
        mAccelerationStructure = vkDevice->createAccelerationStructureKHRUnique(asCreateInfo);

        // get the device address of AccelerationStructure
        vk::AccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo(mAccelerationStructure.get());
        mDeviceAddress = vkDevice->getAccelerationStructureAddressKHR(asDeviceAddressInfo);

        // prepare scratch buffer
        if (asBuildSizesInfo.buildScratchSize > 0)
        {
            vk::BufferCreateInfo scratchci({}, asBuildSizesInfo.buildScratchSize, asUsage | vk::BufferUsageFlagBits::eStorageBuffer);
            mScratchBuffer = mDevice.create<Buffer>(scratchci, memProps);
        }

        // prepare update buffer
        if (asBuildSizesInfo.updateScratchSize > 0)
        {
            vk::BufferCreateInfo updateci({}, asBuildSizesInfo.updateScratchSize, asUsage | vk::BufferUsageFlagBits::eStorageBuffer);
            mUpdateBuffer = mDevice.create<Buffer>(updateci, memProps);
        }

        // build acceleration structure
        asBuildGeometryInfo.dstAccelerationStructure = mAccelerationStructure.get();
        vk::BufferDeviceAddressInfo scratchDeviceAddressInfo(mScratchBuffer->getVkBuffer().get());
        asBuildGeometryInfo.scratchData.deviceAddress = vkDevice->getBufferAddress(scratchDeviceAddressInfo);

        {  // exec build
            std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeInfoPtrs;
            buildRangeInfoPtrs.reserve(asBuildRangeInfo.size());
            for (auto& v : asBuildRangeInfo)
            {
                buildRangeInfoPtrs.emplace_back(&v);
            }

            auto command = buildCommand ? buildCommand : mDevice.create<Command>();

            if (!buildCommand)
            {
                command->begin(true);
            }

            command->getVkCommandBuffer()->buildAccelerationStructuresKHR(asBuildGeometryInfo, buildRangeInfoPtrs);

            // need memory barrier
            vk::MemoryBarrier barrier;

            barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR | vk::AccessFlagBits::eAccelerationStructureWriteKHR;
            barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR;

            command->pipelineBarrier(barrier, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR);

            if (!buildCommand)
            {
                command->end();

                command->execute();
            }

            // HACK: BAD
            vkDevice->waitIdle();
        }
    }

}  // namespace vkpt