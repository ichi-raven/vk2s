#include "../include/vk2s/ShaderBindingTable.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
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

        // calculate needed sizes for each shaders
        const auto baseAlign = rtPipelineProps.shaderGroupBaseAlignment;
        auto regionRaygen    = mDevice.align(raygenShaderEntrySize * raygenShaderCount, baseAlign);
        auto regionMiss      = mDevice.align(missShaderEntrySize * missShaderCount, baseAlign);
        auto regionHit       = mDevice.align(hitShaderEntrySize * hitShaderCount, baseAlign);
        auto regionCallable  = mDevice.align(callableShaderEntrySize * callableShaderCount, baseAlign);

        vk::BufferCreateInfo ci({}, regionRaygen + regionMiss + regionHit, usage);
        mShaderBindingTable = mDevice.create<Buffer>(ci, memProps);

        // get the shader groups handle of the pipeline
        const auto handleSizeAligned        = mDevice.align(handleSize, handleAlignment);
        const auto handleStorageSize        = shaderGroups.size() * handleSizeAligned;
        const uint32_t allShaderCount = raygenShaderCount + missShaderCount + hitShaderCount + callableShaderCount;
        std::vector<uint8_t> shaderHandleStorage(handleStorageSize);
        const auto res = mDevice.getVkDevice()->getRayTracingShaderGroupHandlesKHR(raytracePipeline.getVkPipeline().get(), 0, allShaderCount, shaderHandleStorage.size(), shaderHandleStorage.data());

        if (res != vk::Result::eSuccess)
        {
            assert(!"failed to get ray tracing shader group handle!");
            return;
        }

        // writing SBT
        vk::BufferDeviceAddressInfo deviceAddressInfo(mShaderBindingTable->getVkBuffer().get());
        const auto deviceAddress = vkDevice->getBufferAddress(deviceAddressInfo);

        mSBTInfo.rgen.deviceAddress = deviceAddress;
        // ray generation shader need size == stride
        mSBTInfo.rgen.stride = raygenShaderEntrySize;
        mSBTInfo.rgen.size   = mSBTInfo.rgen.stride;

        mSBTInfo.miss.deviceAddress = deviceAddress + regionRaygen;
        mSBTInfo.miss.size          = regionMiss;
        mSBTInfo.miss.stride        = missShaderEntrySize;

        mSBTInfo.hit.deviceAddress = deviceAddress + regionRaygen + regionMiss;
        mSBTInfo.hit.size          = regionHit;
        mSBTInfo.hit.stride        = hitShaderEntrySize;

        uint8_t* dst = static_cast<uint8_t*>(vkDevice->mapMemory(mShaderBindingTable->getVkDeviceMemory().get(), 0, mShaderBindingTable->getSize()));
        {
            // write the entry of ray generation shader
            memcpy(dst, shaderHandleStorage.data(), handleSize);
            dst += regionRaygen;

            // write the entry of miss shader
            for (int i = 0; i < missShaderCount; ++i)
            {
                memcpy(dst, shaderHandleStorage.data() + handleSizeAligned * (raygenShaderCount + i), handleSize);

                dst += missShaderEntrySize;
            }

            // write the entry of hit shader
            for (int i = 0; i < hitShaderCount; ++i)
            {
                memcpy(dst, shaderHandleStorage.data() + handleSizeAligned * (raygenShaderCount + missShaderCount + i), handleSize);

                dst += hitShaderEntrySize;
            }

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

}  // namespace vk2s
