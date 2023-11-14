/*****************************************************************/ /**
 * \file   Command.hpp
 * \brief  header file of Command class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VKPT_INCLUDE_COMMAND_HPP_
#define VKPT_INCLUDE_COMMAND_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"
#include "SlotMap.hpp"

#include <optional>

namespace vkpt
{
    class Device;

    class RenderPass;
    class Pipeline;
    class Buffer;
    class Image;
    class BindGroup;
    class Semaphore;
    class ShaderBindingTable;
    class Window;

    class Command
    {
    public:  // methods
        Command(Device& device);

        ~Command();

        NONCOPYABLE(Command);
        NONMOVABLE(Command);

        void begin(const bool singleTimeUse = false, const bool secondaryUse = false, const bool simultaneousUse = false);

        void end();

        void beginRenderPass(RenderPass& renderpass, const uint32_t frameBufferIndex, const vk::Rect2D& area, const vk::ArrayProxyNoTemporaries<vk::ClearValue>& clearValues);

        void endRenderPass();

        void setPipeline(Handle<Pipeline> pipeline);

        void setBindGroup(BindGroup& bindGroup, vk::ArrayProxy<const uint32_t> const& dynamicOffsets = {});

        void bindVertexBuffer(Buffer& vertexBuffer);
        void bindIndexBuffer(Buffer& indexBuffer);

        void draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t firstVertex, const uint32_t firstInstance);

        void drawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t firstIndex, const uint32_t vertexOffset, const uint32_t firstInstance);

        void traceRays(const ShaderBindingTable& shaderBindingTable, const uint32_t width, const uint32_t height, const uint32_t depth);

        void dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ);

        void pipelineBarrier(const vk::MemoryBarrier barrier, const vk::PipelineStageFlagBits from, const vk::PipelineStageFlagBits to);

        void transitionImageLayout(Image& image, const vk::ImageLayout from, const vk::ImageLayout to);
        
        void copyBufferToImage(Buffer& buffer, Image& image, const uint32_t width, const uint32_t height);
        
        void copyImage(Image& src, Image& dst, const vk::ImageCopy& region);

        void copyImageToSwapchain(Image& src, Window& window, const vk::ImageCopy& region, const uint32_t frameBufferIndex);

        void drawImGui();

        void execute(Handle<Semaphore> waitSem = Handle<Semaphore>(), Handle<Semaphore> signalSem = Handle<Semaphore>());

        const vk::UniqueCommandBuffer& getVkCommandBuffer();

    private: // methods
        inline void transitionLayoutInternal(vk::Image image, vk::ImageAspectFlags flag, const vk::ImageLayout from, const vk::ImageLayout to);

    private:  // member variables
        Device& mDevice;

        vk::UniqueCommandBuffer mCommandBuffer;
        vk::UniqueFence mFence;
        std::optional<Handle<Pipeline>> mNowPipeline;
    };
}  // namespace vkpt

#endif