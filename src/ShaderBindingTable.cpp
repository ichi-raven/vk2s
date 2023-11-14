#include "../include/ShaderBindingTable.hpp"

#include "../include/Device.hpp"

namespace vkpt
{

    ShaderBindingTable::ShaderBindingTable(Device& device, Pipeline& raytracePipeline, const uint32_t raygenShaderCount, const uint32_t missShaderCount, const uint32_t hitShaderCount, const uint32_t callableShaderCount,
                                           const vk::ArrayProxyNoTemporaries<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        const auto memProps = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        const auto usage           = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        const auto rtPipelineProps = getRayTracingPipelineProperties();

        // get each size of entry
        // align each shader entry size to shaderGroupHandleAlignment
        const auto handleSize              = rtPipelineProps.shaderGroupHandleSize;
        const auto handleAlignment         = rtPipelineProps.shaderGroupHandleAlignment;
        const auto raygenShaderEntrySize   = mDevice.align(handleSize, handleAlignment);
        const auto missShaderEntrySize     = mDevice.align(handleSize, handleAlignment);
        const auto hitShaderEntrySize      = mDevice.align(handleSize, handleAlignment);
        const auto callableShaderEntrySize = mDevice.align(handleSize, handleAlignment);
        //hitShaderEntrySize += sizeof(uint64_t);  // IndexBuffer address
        //hitShaderEntrySize += sizeof(uint64_t);  // VertexBuffer address
        //hitShaderEntrySize = mDevice.align(hitShaderEntrySize, handleAlignment);

        // calculate needed sizes for each shaders
        const auto baseAlign = rtPipelineProps.shaderGroupBaseAlignment;
        auto regionRaygen    = mDevice.align(raygenShaderEntrySize * raygenShaderCount, baseAlign);
        auto regionMiss      = mDevice.align(missShaderEntrySize * missShaderCount, baseAlign);
        auto regionHit       = mDevice.align(hitShaderEntrySize * hitShaderCount, baseAlign);
        auto regionCallable  = mDevice.align(callableShaderEntrySize * callableShaderCount, baseAlign);

        vk::BufferCreateInfo ci({}, regionRaygen + regionMiss + regionHit, usage);
        mShaderBindingTable = mDevice.create<Buffer>(ci, memProps);

        // get the shader groups handle of the pipeline
        auto handleSizeAligned = mDevice.align(handleSize, handleAlignment);
        auto handleStorageSize = shaderGroups.size() * handleSizeAligned;
        std::vector<uint8_t> shaderHandleStorage(handleStorageSize);
        const auto res = mDevice.getVkDevice()->getRayTracingShaderGroupHandlesKHR(raytracePipeline.getVkPipeline().get(), 0, static_cast<uint32_t>(shaderGroups.size()), shaderHandleStorage.size(), shaderHandleStorage.data());

        if (res != vk::Result::eSuccess)
        {
            assert(!"failed to get ray tracing shader group handle!");
            return;
        }

        // writing SBT
        vk::BufferDeviceAddressInfo deviceAddressInfo(mShaderBindingTable->getVkBuffer().get());
        const auto deviceAddress = vkDevice->getBufferAddress(deviceAddressInfo);

        void* p = vkDevice->mapMemory(mShaderBindingTable->getVkDeviceMemory().get(), 0, mShaderBindingTable->getSize());
        {
            auto dst = static_cast<uint8_t*>(p);

            // write the entry of ray generation shader
            auto raygen = shaderHandleStorage.data();  // + handleSizeAligned* static_cast<uint32_t>(ShaderGroups::eGroupRayGenShader);
            memcpy(dst, raygen, handleSize);
            dst += regionRaygen;
            mSBTInfo.rgen.deviceAddress = deviceAddress;
            // ray generation shader need size == stride
            mSBTInfo.rgen.stride = raygenShaderEntrySize;
            mSBTInfo.rgen.size   = mSBTInfo.rgen.stride;

            // write the entry of miss shader
            auto miss = shaderHandleStorage.data() + handleSizeAligned * raygenShaderCount;  // * static_cast<uint32_t>(ShaderGroups::eGroupMissShader);
            memcpy(dst, miss, handleSize);
            dst += regionMiss;
            mSBTInfo.miss.deviceAddress = deviceAddress + regionRaygen;
            mSBTInfo.miss.size          = regionMiss;
            mSBTInfo.miss.stride        = missShaderEntrySize;

            // write the entry of hit shader
            auto hit = shaderHandleStorage.data() + handleSizeAligned * (raygenShaderCount + missShaderCount);  //static_cast<uint32_t>(ShaderGroups::eGroupHitShader);

            auto entryStart = dst;
            for (int i = 0; i < hitShaderCount; ++i)
            {
                memcpy(entryStart, hit, handleSize);
                //writeSBTDataForHitShader(entryStart + handleSize, meshInstance);

                entryStart += hitShaderEntrySize;
            }
            entryStart -= hitShaderEntrySize;

            mSBTInfo.hit.deviceAddress = deviceAddress + regionRaygen + regionMiss;
            mSBTInfo.hit.size          = regionHit;
            mSBTInfo.hit.stride        = hitShaderEntrySize;

            // write the entry of callable shader
            //auto callable = shaderHandleStorage.data() + handleSizeAligned * (raygenShaderCount + missShaderCount + hitShaderCount);  //static_cast<uint32_t>(ShaderGroups::eGroupHitShader);
            //memcpy(dst, callable, handleSize);
            //dst += regionCallable;
            //mSBTInfo.callable.deviceAddress = deviceAddress + regionRaygen + regionMiss + regionHit;
            //mSBTInfo.callable.size          = regionCallable;
            //mSBTInfo.callable.stride        = callableShaderEntrySize;
        }

        vkDevice->unmapMemory(mShaderBindingTable->getVkDeviceMemory().get());
    }

    ShaderBindingTable::~ShaderBindingTable()
    {
    }

    const ShaderBindingTable::VkShaderBindingTableInfo& ShaderBindingTable::getVkSBTInfo() const
    {
        return mSBTInfo;
    }

    inline vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ShaderBindingTable::getRayTracingPipelineProperties() const
    {
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR physDevRtPipelineProps;
        vk::PhysicalDeviceProperties2 physDevProps2;
        physDevProps2.pNext = &physDevRtPipelineProps;
        mDevice.getVkPhysicalDevice().getProperties2(&physDevProps2);
        return physDevRtPipelineProps;
    }

}  // namespace vkpt
