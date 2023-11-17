#include "../include/vk2s/Pipeline.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
{
    Pipeline::Pipeline(Device& device, const VkGraphicsPipelineInfo& info)
        : mDevice(device)
        , mBindPoint(vk::PipelineBindPoint::eGraphics)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        if (info.bindLayout)
        {
            pipelineLayoutInfo.setSetLayouts(info.bindLayout->getVkDescriptorSetLayouts());
        }

        mLayout = vkDevice->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, info.vs->getVkShaderModule().get(), info.vs->getEntryPoint().c_str());
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, info.fs->getVkShaderModule().get(), info.fs->getEntryPoint().c_str());

        std::array shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

        vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &info.inputState, &info.inputAssembly, {}, &info.viewportState, &info.rasterizer, &info.multiSampling, &info.depthStencil, &info.colorBlending, {}, mLayout.get(),
                                                    info.renderPass->getVkRenderPass().get(), 0, {}, {});

        vk::ResultValue<vk::UniquePipeline> result = vkDevice->createGraphicsPipelineUnique({}, pipelineInfo);
        if (result.result == vk::Result::eSuccess)
        {
            mPipeline = std::move(result.value);
        }
        else
        {
            throw std::runtime_error("failed to create a pipeline!");
        }
    }

    Pipeline::Pipeline(Device& device, const ComputePipelineInfo& info)
        : mDevice(device)
        , mBindPoint(vk::PipelineBindPoint::eCompute)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        if (info.bindLayout)
        {
            pipelineLayoutInfo.setSetLayouts(info.bindLayout->getVkDescriptorSetLayouts());
        }
        mLayout = vkDevice->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::PipelineShaderStageCreateInfo ssci({}, vk::ShaderStageFlagBits::eCompute, info.cs->getVkShaderModule().get(), info.cs->getEntryPoint().c_str());

        vk::ComputePipelineCreateInfo ci;
        ci.setStage(ssci).setLayout(mLayout.get());

        vk::ResultValue<vk::UniquePipeline> result = vkDevice->createComputePipelineUnique({}, ci);
        if (result.result == vk::Result::eSuccess)
        {
            mPipeline = std::move(result.value);
        }
        else
        {
            throw std::runtime_error("failed to create a pipeline!");
        }
    }

    Pipeline::Pipeline(Device& device, const VkRayTracingPipelineInfo& info)
        : mDevice(device)
        , mBindPoint(vk::PipelineBindPoint::eRayTracingKHR)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        if (info.bindLayout)
        {
            pipelineLayoutInfo.setSetLayouts(info.bindLayout->getVkDescriptorSetLayouts());
        }

        mLayout = vkDevice->createPipelineLayoutUnique(pipelineLayoutInfo);

        std::vector<vk::PipelineShaderStageCreateInfo> stages;
        stages.reserve(4);

        if (info.raygenShader)
        {
            vk::PipelineShaderStageCreateInfo rgsStage({}, vk::ShaderStageFlagBits::eRaygenKHR, info.raygenShader->getVkShaderModule().get(), info.raygenShader->getEntryPoint().c_str());
            stages.emplace_back(rgsStage);
        }

        if (info.missShader)
        {
            vk::PipelineShaderStageCreateInfo missStage({}, vk::ShaderStageFlagBits::eMissKHR, info.missShader->getVkShaderModule().get(),info.missShader->getEntryPoint().c_str());
            stages.emplace_back(missStage);
        }

        if (info.chitShader)
        {
            vk::PipelineShaderStageCreateInfo chitStage({}, vk::ShaderStageFlagBits::eClosestHitKHR, info.chitShader->getVkShaderModule().get(), info.chitShader->getEntryPoint().c_str());
            stages.emplace_back(chitStage);
        }

        if (info.callableShader)
        {
            vk::PipelineShaderStageCreateInfo callableStage({}, vk::ShaderStageFlagBits::eCallableKHR, info.callableShader->getVkShaderModule().get(), info.callableShader->getEntryPoint().c_str());
            stages.emplace_back(callableStage);
        }

        vk::RayTracingPipelineCreateInfoKHR rtPipelineCI;
        rtPipelineCI.stageCount                   = uint32_t(stages.size());
        rtPipelineCI.pStages                      = stages.data();
        rtPipelineCI.groupCount                   = uint32_t(info.shaderGroups.size());
        rtPipelineCI.pGroups                      = info.shaderGroups.data();
        rtPipelineCI.maxPipelineRayRecursionDepth = 1;
        rtPipelineCI.layout                       = mLayout.get();

        vk::ResultValue<vk::UniquePipeline> result = vkDevice->createRayTracingPipelineKHRUnique({}, {}, rtPipelineCI);
        if (result.result == vk::Result::eSuccess)
        {
            mPipeline = std::move(result.value);
        }
        else
        {
            throw std::runtime_error("failed to create a pipeline!");
        }
    }

    Pipeline ::~Pipeline()
    {
    }

    const vk::UniquePipelineLayout& Pipeline::getVkPipelineLayout()
    {
        return mLayout;
    }

    const vk::UniquePipeline& Pipeline::getVkPipeline()
    {
        return mPipeline;
    }

    vk::PipelineBindPoint Pipeline::getVkPipelineBindPoint() const
    {
        return mBindPoint;
    }

}  // namespace vk2s