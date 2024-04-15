/*****************************************************************/ /**
 * \file   Pipeline.hpp
 * \brief  header file of Pipeline class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_PIPELINE_HPP_
#define VK2S_INCLUDE_PIPELINE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "SlotMap.hpp"
#include "Macro.hpp"

#include <optional>

namespace vk2s
{
    class Device;

    class BindLayout;
    class Shader;
    class RenderPass;

    class Pipeline
    {
    public:  // types
        struct GraphicsPipelineInfo
        {
            Handle<Shader> vs;
            Handle<Shader> fs;
            vk::ArrayProxyNoTemporaries<Handle<BindLayout>> bindLayouts;
            Handle<RenderPass> renderPass;
            vk::PipelineVertexInputStateCreateInfo inputState;
            vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
            vk::PipelineViewportStateCreateInfo viewportState;
            vk::PipelineRasterizationStateCreateInfo rasterizer;
            vk::PipelineMultisampleStateCreateInfo multiSampling;
            vk::PipelineDepthStencilStateCreateInfo depthStencil;
            vk::PipelineColorBlendStateCreateInfo colorBlending;
        };

        struct ComputePipelineInfo
        {
            Handle<Shader> cs;
            vk::ArrayProxyNoTemporaries<Handle<BindLayout>> bindLayouts;
        };

        struct RayTracingPipelineInfo
        {
            std::vector<Handle<Shader>> raygenShaders;
            std::vector<Handle<Shader>> missShaders;
            std::vector<Handle<Shader>> chitShaders;
            std::vector<Handle<Shader>> callableShaders;
            vk::ArrayProxyNoTemporaries<Handle<BindLayout>> bindLayouts;

            std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        };

    public:  // methods
        Pipeline(Device& device, const GraphicsPipelineInfo& info);
        Pipeline(Device& device, const ComputePipelineInfo& info);
        Pipeline(Device& device, const RayTracingPipelineInfo& info);

        NONCOPYABLE(Pipeline);
        NONMOVABLE(Pipeline);

        ~Pipeline();

        const vk::UniquePipelineLayout& getVkPipelineLayout();

        const vk::UniquePipeline& getVkPipeline();

        vk::PipelineBindPoint getVkPipelineBindPoint() const;

    private:  // methods
    private:  // member variables
        Device& mDevice;

        vk::UniquePipelineLayout mLayout;
        vk::UniquePipeline mPipeline;
        vk::PipelineBindPoint mBindPoint;
    };
}  // namespace vk2s

#endif