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
        const auto handleSizeAligned  = mDevice.align(handleSize, handleAlignment);
        const auto handleStorageSize  = shaderGroups.size() * handleSizeAligned;
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

    // for additional entry writing
    ShaderBindingTable::ShaderBindingTable(Device& device, Pipeline& raytracePipeline, const RegionInfo& raygenShaderInfo, const RegionInfo& missShaderInfo, const RegionInfo& hitShaderInfo, const RegionInfo& callableShaderInfo,
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
        const auto raygenShaderEntrySize   = mDevice.align(handleSize + raygenShaderInfo.additionalEntrySize, handleAlignment);
        const auto missShaderEntrySize     = mDevice.align(handleSize + missShaderInfo.additionalEntrySize, handleAlignment);
        const auto hitShaderEntrySize      = mDevice.align(handleSize + hitShaderInfo.additionalEntrySize, handleAlignment);
        const auto callableShaderEntrySize = mDevice.align(handleSize + callableShaderInfo.additionalEntrySize, handleAlignment);

        // calculate needed sizes for each shaders
        const auto baseAlign = rtPipelineProps.shaderGroupBaseAlignment;
        auto regionRaygen    = mDevice.align(raygenShaderEntrySize * raygenShaderInfo.shaderNum, baseAlign);
        auto regionMiss      = mDevice.align(missShaderEntrySize * missShaderInfo.shaderNum, baseAlign);
        auto regionHit       = mDevice.align(hitShaderEntrySize * hitShaderInfo.shaderNum, baseAlign);
        auto regionCallable  = mDevice.align(callableShaderEntrySize * callableShaderInfo.shaderNum, baseAlign);

        vk::BufferCreateInfo ci({}, regionRaygen + regionMiss + regionHit, usage);
        mShaderBindingTable = mDevice.create<Buffer>(ci, memProps);

        // get the shader groups handle of the pipeline
        const auto handleSizeAligned  = mDevice.align(handleSize, handleAlignment);
        const auto handleStorageSize  = shaderGroups.size() * handleSizeAligned;
        const uint32_t allShaderCount = raygenShaderInfo.shaderNum + missShaderInfo.shaderNum + hitShaderInfo.shaderNum + callableShaderInfo.shaderNum;
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

        std::byte* dst = static_cast<std::byte*>(vkDevice->mapMemory(mShaderBindingTable->getVkDeviceMemory().get(), 0, mShaderBindingTable->getSize()));
        {
            std::byte* pStart = dst;
            
            // write entries of ray generation shader
            {
                std::byte* pStartRaygen = dst;
                for (int i = 0; i < raygenShaderInfo.shaderNum; ++i)
                {
                    memcpy(dst, shaderHandleStorage.data(), handleSize);
                    dst += handleSize;
                    if (raygenShaderInfo.entryWriterPerShader)
                    {
                        (*raygenShaderInfo.entryWriterPerShader)(dst);
                    }
                }
                assert(dst <= pStartRaygen + regionRaygen || !"invalid raygen shader entry writing!");
                dst = pStartRaygen + regionRaygen;
            }

            // write entries of miss shader
            {
                std::byte* pStartMiss = dst;
                for (int i = 0; i < missShaderInfo.shaderNum; ++i)
                {
                    memcpy(dst, shaderHandleStorage.data() + handleSizeAligned * (raygenShaderInfo.shaderNum + i), handleSize);
                    dst += handleSize;
                    if (missShaderInfo.entryWriterPerShader)
                    {
                        (*missShaderInfo.entryWriterPerShader)(dst);
                    }
                }
                assert(dst <= pStartMiss + regionMiss || !"invalid miss shader entry writing!");
                dst = pStartMiss + regionMiss;
            }

            // write entries of hit shader
            {
                std::byte* pStartHit = dst;
                for (int i = 0; i < hitShaderInfo.shaderNum; ++i)
                {
                    memcpy(dst, shaderHandleStorage.data() + handleSizeAligned * (raygenShaderInfo.shaderNum + missShaderInfo.shaderNum + i), handleSize);
                    dst += handleSize;
                    if (hitShaderInfo.entryWriterPerShader)
                    {
                        (*hitShaderInfo.entryWriterPerShader)(dst);
                    }
                }
                assert(dst <= pStartHit + regionHit || !"invalid hit shader entry writing!");
                dst = pStartHit + regionHit;
            }

            // write entries of callable shader
            {
                std::byte* pStartCallable = dst;
                for (int i = 0; i < callableShaderInfo.shaderNum; ++i)
                {
                    memcpy(dst, shaderHandleStorage.data() + handleSizeAligned * (raygenShaderInfo.shaderNum + missShaderInfo.shaderNum + hitShaderInfo.shaderNum + i), handleSize);
                    dst += handleSize;
                    if (callableShaderInfo.entryWriterPerShader)
                    {
                        (*callableShaderInfo.entryWriterPerShader)(dst);
                    }
                }
                assert(dst <= pStartCallable + regionCallable || !"invalid hit shader entry writing!");
                // No alignment correction (since it is the last shader group)
                //dst = pStartCallable + regionCallable;
            }
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
