/*****************************************************************/ /**
 * @file   Command.hpp
 * @brief  header file of Command class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_COMMAND_HPP_
#define VK2S_INCLUDE_COMMAND_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "Macro.hpp"
#include "SlotMap.hpp"

#include <optional>

namespace vk2s
{
    //! forward declaration
    class Device;
    class Fence;
    class RenderPass;
    class Pipeline;
    class Buffer;
    class Image;
    class BindGroup;
    class Semaphore;
    class ShaderBindingTable;
    class Window;

    /**
     * @brief  Command class, write and execute commands to the GPU
     */
    class Command
    {
    public:  // methods
        /**
         * @brief  constructor
         */
        Command(Device& device);

        /**
         * @brief  destructor
         */
        ~Command();

        NONCOPYABLE(Command);
        NONMOVABLE(Command);

        /**
         * @brief  reset the instructions written to this command
         */
        void reset();

        /**
         * @brief  begin writing to the command
         */
        void begin(const bool singleTimeUse = false, const bool secondaryUse = false, const bool simultaneousUse = false);

        /**
         * @brief  end writing to the command
         */
        void end();

        /**
         * @brief begin specified render pass (and drawing to the specified render pass)
         */
        void beginRenderPass(RenderPass& renderpass, const uint32_t frameBufferIndex, const vk::Rect2D& area, const vk::ArrayProxyNoTemporaries<const vk::ClearValue>& clearValues);

        /**
         * @brief end specified render pass (and drawing to the specified render pass)
         */
        void endRenderPass();

        /**
         * @brief  set Pipeline 
         */
        void setPipeline(Handle<Pipeline> pipeline);

        /**
         * @brief  set BindGroup
         */
        void setBindGroup(const uint8_t set, BindGroup& bindGroup, vk::ArrayProxy<const uint32_t> const& dynamicOffsets = {});

        /**
         * @brief set Viewport (only for pipeline DynamicState)
         */
        void setViewport(const uint32_t firstViewport, const vk::ArrayProxy<vk::Viewport> viewports);

        /**
         * @brief set Scissor (only for pipeline DynamicState)
         */
        void setScissor(const uint32_t firstScissor, const vk::ArrayProxy<vk::Rect2D> scissors);

        /**
         * @brief  set VertexBuffer
         */
        void bindVertexBuffer(Buffer& vertexBuffer);

        /**
         * @brief  set IndexBuffer
         */
        void bindIndexBuffer(Buffer& indexBuffer);

        /**
         * @brief  drawing with specified settings (without index)
         */
        void draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t firstVertex, const uint32_t firstInstance);

        /**
         * @brief  drawing with specified settings (with index)
         */
        void drawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t firstIndex, const uint32_t vertexOffset, const uint32_t firstInstance);

        /**
         * @brief  shooting (tracing) a ray
         */
        void traceRays(const ShaderBindingTable& shaderBindingTable, const uint32_t width, const uint32_t height, const uint32_t depth);

        /**
         * @brief  dispatch operations (compute)
         */
        void dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ);

        /**
         * @brief  create pipeline barriers to resources
         */
        void pipelineBarrier(const vk::MemoryBarrier barrier, const vk::PipelineStageFlagBits from, const vk::PipelineStageFlagBits to);

        /**
         * @brief  transitioning the internal layout of an Image 
         */
        void transitionImageLayout(Image& image, const vk::ImageLayout from, const vk::ImageLayout to);

        /**
         * @brief  copy Buffer contents to Image
         */
        void copyBufferToImage(Buffer& buffer, Image& image, const uint32_t width, const uint32_t height);

        /**
         * @brief  copy Image contents to Buffer
         */
        void copyImageToBuffer(Image& image, Buffer& buffer, const uint32_t width, const uint32_t height);

        /**
         * @brief  copy Image contents to Image
         */
        void copyImage(Image& src, Image& dst, const vk::ImageCopy& region);

        /**
         * @brief  copy Image contents to Swapchain
         */
        void copyImageToSwapchain(Image& src, Window& window, const vk::ImageCopy& region, const uint32_t frameBufferIndex);

        /**
         * @brief  clear Image contents
         */
        void clearImage(Image& target, const vk::ImageLayout layout, const vk::ClearValue& clearValue, const vk::ArrayProxy<vk::ImageSubresourceRange>& ranges);

        /**
         * @brief  drawing ImGui
         */
        void drawImGui();

        /**
         * @brief  Execute the instructions written 
         */
        void execute(const Handle<Fence>& signalFence = Handle<Fence>(), const Handle<Semaphore>& waitSem = Handle<Semaphore>(), const Handle<Semaphore>& signalSem = Handle<Semaphore>());

        /**
         * @brief  get vulkan internal handle
         */
        const vk::UniqueCommandBuffer& getVkCommandBuffer();

    private:  // methods
        /**
         * @brief  internal implementation of transitionImageLayout function
         */
        inline void transitionLayoutInternal(vk::Image image, vk::ImageAspectFlags flag, const vk::ImageLayout from, const vk::ImageLayout to);

    private:  // member variables
        //! reference to device
        Device& mDevice;

        //! vulkan command buffer handle
        vk::UniqueCommandBuffer mCommandBuffer;
        //! Pipeline currently set
        Handle<Pipeline> mNowPipeline;
    };
}  // namespace vk2s

#endif