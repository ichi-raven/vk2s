/*****************************************************************/ /**
 * @file   Pipeline.cpp
 * @brief  source file of Pipeline class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#include "../include/vk2s/Pipeline.hpp"

#include "../include/vk2s/Device.hpp"

namespace vk2s
{
    Pipeline::Pipeline(Device& device, const GraphicsPipelineInfo& info)
        : mDevice(device)
        , mBindPoint(vk::PipelineBindPoint::eGraphics)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setPushConstantRanges(info.pushConstantRanges);

        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.reserve(info.bindLayouts.size());
        for (const auto& layout : info.bindLayouts)
        {
            layouts.emplace_back(layout->getVkDescriptorSetLayout().get());
        }

        pipelineLayoutInfo.setSetLayouts(layouts);

        mLayout = vkDevice->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, info.vs->getVkShaderModule().get(), info.vs->getEntryPoint().c_str());
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, info.fs->getVkShaderModule().get(), info.fs->getEntryPoint().c_str());

        std::array shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

        vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &info.inputState, &info.inputAssembly, {}, &info.viewportState, &info.rasterizer, &info.multiSampling, &info.depthStencil, &info.colorBlending, &info.dynamicStates,
                                                    mLayout.get(), info.renderPass->getVkRenderPass().get(), 0, {}, {});

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
        pipelineLayoutInfo.setPushConstantRanges(info.pushConstantRanges);

        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.reserve(info.bindLayouts.size());
        for (const auto& layout : info.bindLayouts)
        {
            layouts.emplace_back(layout->getVkDescriptorSetLayout().get());
        }

        pipelineLayoutInfo.setSetLayouts(layouts);
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

    Pipeline::Pipeline(Device& device, const RayTracingPipelineInfo& info)
        : mDevice(device)
        , mBindPoint(vk::PipelineBindPoint::eRayTracingKHR)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setPushConstantRanges(info.pushConstantRanges);

        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.reserve(info.bindLayouts.size());
        for (const auto& layout : info.bindLayouts)
        {
            layouts.emplace_back(layout->getVkDescriptorSetLayout().get());
        }

        pipelineLayoutInfo.setSetLayouts(layouts);

        mLayout = vkDevice->createPipelineLayoutUnique(pipelineLayoutInfo);

        std::vector<vk::PipelineShaderStageCreateInfo> stages;
        stages.reserve(4);

        for (const auto& raygenShader : info.raygenShaders)
        {
            stages.emplace_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eRaygenKHR, raygenShader->getVkShaderModule().get(), raygenShader->getEntryPoint().c_str()));
        }

        for (const auto& missShader : info.missShaders)
        {
            vk::PipelineShaderStageCreateInfo missStage({}, vk::ShaderStageFlagBits::eMissKHR, missShader->getVkShaderModule().get(), missShader->getEntryPoint().c_str());
            stages.emplace_back(missStage);
        }

        for (const auto& chitShader : info.chitShaders)
        {
            vk::PipelineShaderStageCreateInfo chitStage({}, vk::ShaderStageFlagBits::eClosestHitKHR, chitShader->getVkShaderModule().get(), chitShader->getEntryPoint().c_str());
            stages.emplace_back(chitStage);
        }

        for (const auto& callableShader : info.callableShaders)
        {
            vk::PipelineShaderStageCreateInfo callableStage({}, vk::ShaderStageFlagBits::eCallableKHR, callableShader->getVkShaderModule().get(), callableShader->getEntryPoint().c_str());
            stages.emplace_back(callableStage);
        }

        vk::RayTracingPipelineCreateInfoKHR rtPipelineCI;
        rtPipelineCI.stageCount                   = uint32_t(stages.size());
        rtPipelineCI.pStages                      = stages.data();
        rtPipelineCI.groupCount                   = uint32_t(info.shaderGroups.size());
        rtPipelineCI.pGroups                      = info.shaderGroups.data();
        rtPipelineCI.maxPipelineRayRecursionDepth = 2;  // HACK:
        rtPipelineCI.layout                       = mLayout.get();
        if (mDevice.getVkAvailableExtensions().useNVMotionBlurExt)
        {
            rtPipelineCI.flags = vk::PipelineCreateFlagBits::eRayTracingAllowMotionNV;
        }

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