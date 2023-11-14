/*****************************************************************/ /**
 * \file   Pipeline.hpp
 * \brief  header file of Pipeline class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_PIPELINE_HPP_
#define VKPT_INCLUDE_PIPELINE_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "SlotMap.hpp"
#include "Macro.hpp"

#include <optional>

namespace vkpt
{
    class Device;

    class BindLayout;
    class Shader;
    class RenderPass;

    class Pipeline
    {
    public:  // types
        struct VkGraphicsPipelineInfo
        {
            Handle<Shader> vs;
            Handle<Shader> fs;
            Handle<BindLayout> bindLayout;
            Handle<RenderPass> renderPass;

            // TODO: too many

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
            Handle<BindLayout> bindLayout;
        };

        struct VkRayTracingPipelineInfo
        {
            Handle<Shader> raygenShader;
            Handle<Shader> missShader;
            Handle<Shader> chitShader;
            Handle<Shader> callableShader;
            Handle<BindLayout> bindLayout;

            std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        };

    public:  // methods
        Pipeline(Device& device, const VkGraphicsPipelineInfo& info);
        Pipeline(Device& device, const ComputePipelineInfo& info);
        Pipeline(Device& device, const VkRayTracingPipelineInfo& info);

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
}  // namespace vkpt

#endif