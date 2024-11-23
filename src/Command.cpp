#include "../include/vk2s/Command.hpp"

#include "../include/vk2s/Device.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace vk2s
{
    Command::Command(Device& device)
        : mDevice(device)
    {
        vk::CommandBufferAllocateInfo allocInfo(mDevice.getVkCommandPool().get(), vk::CommandBufferLevel::ePrimary, 1);

        mCommandBuffer = std::move(mDevice.getVkDevice()->allocateCommandBuffersUnique(allocInfo).front());
    }

    Command::~Command()
    {
        mDevice.getVkDevice()->waitIdle();
        mCommandBuffer->reset();
    }

    void Command::reset()
    {
        mCommandBuffer->reset();
    }

    void Command::begin(const bool singleTimeUse, const bool secondaryUse, const bool simultaneousUse)
    {
        vk::CommandBufferUsageFlags usage{};
        if (singleTimeUse)
        {
            usage |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        }
        if (secondaryUse)
        {
            usage |= vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        }
        if (simultaneousUse)
        {
            usage |= vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        }

        mCommandBuffer->begin(vk::CommandBufferBeginInfo(usage));
    }

    void Command::end()
    {
        mCommandBuffer->end();
    }

    void Command::beginRenderPass(RenderPass& renderpass, const uint32_t frameBufferIndex, const vk::Rect2D& area, const vk::ArrayProxyNoTemporaries<const vk::ClearValue>& clearValues)
    {
        vk::RenderPassBeginInfo bi(renderpass.getVkRenderPass().get(), renderpass.getVkFrameBuffers()[frameBufferIndex].get(), area, clearValues);
        mCommandBuffer->beginRenderPass(bi, vk::SubpassContents::eInline);
    }

    void Command::endRenderPass()
    {
        mCommandBuffer->endRenderPass();
    }

    void Command::setPipeline(Handle<Pipeline> pipeline)
    {
        mCommandBuffer->bindPipeline(pipeline->getVkPipelineBindPoint(), pipeline->getVkPipeline().get());
        mNowPipeline = pipeline;
    }

    void Command::setBindGroup(const uint8_t set, BindGroup& bindGroup, vk::ArrayProxy<const uint32_t> const& dynamicOffsets)
    {
        assert(mNowPipeline || !"pipeline isn't set yet!");
        mCommandBuffer->bindDescriptorSets(mNowPipeline->getVkPipelineBindPoint(), mNowPipeline->getVkPipelineLayout().get(), set, bindGroup.getVkDescriptorSet(), dynamicOffsets);
    }

    void Command::bindVertexBuffer(Buffer& vertexBuffer)
    {
        mCommandBuffer->bindVertexBuffers(0, vertexBuffer.getVkBuffer().get(), { 0 });
    }

    void Command::bindIndexBuffer(Buffer& indexBuffer)
    {
        mCommandBuffer->bindIndexBuffer(indexBuffer.getVkBuffer().get(), 0, vk::IndexType::eUint32);
    }

    void Command::draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t firstVertex, const uint32_t firstInstance)
    {
        mCommandBuffer->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void Command::drawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t firstIndex, const uint32_t vertexOffset, const uint32_t firstInstance)
    {
        mCommandBuffer->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void Command::traceRays(const ShaderBindingTable& shaderBindingTable, const uint32_t width, const uint32_t height, const uint32_t depth)
    {
        const auto& sbtInfo = shaderBindingTable.getVkSBTInfo();
        mCommandBuffer->traceRaysKHR(sbtInfo.rgen, sbtInfo.miss, sbtInfo.hit, sbtInfo.callable, width, height, depth);
    }

    void Command::dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ)
    {
        mCommandBuffer->dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void Command::pipelineBarrier(const vk::MemoryBarrier barrier, const vk::PipelineStageFlagBits from, const vk::PipelineStageFlagBits to)
    {
        mCommandBuffer->pipelineBarrier(from, to, {}, { barrier }, {}, {});
    }

    void Command::transitionImageLayout(Image& image, const vk::ImageLayout from, const vk::ImageLayout to)
    {
        transitionLayoutInternal(image.getVkImage().get(), image.getVkAspectFlag(), from, to);
    }

    void Command::copyBufferToImage(Buffer& buffer, Image& image, const uint32_t width, const uint32_t height)
    {
        vk::BufferImageCopy region;
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;

        region.imageOffset = vk::Offset3D(0, 0, 0);
        region.imageExtent = vk::Extent3D(width, height, 1);

        mCommandBuffer->copyBufferToImage(buffer.getVkBuffer().get(), image.getVkImage().get(), vk::ImageLayout::eTransferDstOptimal, region);
    }

    void Command::copyImageToBuffer(Image& image, Buffer& buffer, const uint32_t width, const uint32_t height)
    {
        vk::BufferImageCopy region;
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;

        region.imageOffset = vk::Offset3D(0, 0, 0);
        region.imageExtent = vk::Extent3D(width, height, 1);

        mCommandBuffer->copyImageToBuffer(image.getVkImage().get(), vk::ImageLayout::eTransferSrcOptimal, buffer.getVkBuffer().get(), region);
    }

    void Command::copyImage(Image& src, Image& dst, const vk::ImageCopy& region)
    {
        mCommandBuffer->copyImage(src.getVkImage().get(), vk::ImageLayout::eTransferSrcOptimal, dst.getVkImage().get(), vk::ImageLayout::eTransferDstOptimal, region);
    }

    void Command::copyImageToSwapchain(Image& src, Window& window, const vk::ImageCopy& region, const uint32_t frameBufferIndex)
    {
        auto swapchainImage = window.getVkImages().at(frameBufferIndex);
        transitionLayoutInternal(swapchainImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal);
        mCommandBuffer->copyImage(src.getVkImage().get(), vk::ImageLayout::eTransferSrcOptimal, swapchainImage, vk::ImageLayout::eTransferDstOptimal, region);
    }

    void Command::clearImage(Image& target, const vk::ImageLayout layout, const vk::ClearValue& clearValue, const vk::ArrayProxy<vk::ImageSubresourceRange>& ranges)
    {
        if (target.getVkAspectFlag() | vk::ImageAspectFlagBits::eColor)
        {
            mCommandBuffer->clearColorImage(target.getVkImage().get(), layout, clearValue.color, ranges);
        }
        else// depth
        {
            mCommandBuffer->clearDepthStencilImage(target.getVkImage().get(), layout, clearValue.depthStencil, ranges);
        }
    }

    void Command::drawImGui()
    {
        // ImGui command write
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mCommandBuffer.get());
    }

    void Command::execute(const Handle<Fence>& fence, const Handle<Semaphore>& wait, const Handle<Semaphore>& signal)
    {
        vk::SubmitInfo submitInfo(nullptr, nullptr, mCommandBuffer.get(), nullptr);

        if (wait)
        {
            // TODO: change wait stage
            constexpr vk::PipelineStageFlags waitStage(vk::PipelineStageFlagBits::eColorAttachmentOutput);

            submitInfo.setWaitSemaphores(wait->getVkSemaphore().get());
            submitInfo.setWaitDstStageMask(waitStage);
        }

        if (signal)
        {
            submitInfo.setSignalSemaphores(signal->getVkSemaphore().get());
        }

        if (fence)
        {
            mDevice.getVkGraphicsQueue().submit(submitInfo, fence->getVkFence().get());
        }
        else
        {
            mDevice.getVkGraphicsQueue().submit(submitInfo);
        }
    }

    const vk::UniqueCommandBuffer& Command::getVkCommandBuffer()
    {
        return mCommandBuffer;
    }

    inline void Command::transitionLayoutInternal(vk::Image image, vk::ImageAspectFlags flag, const vk::ImageLayout from, const vk::ImageLayout to)
    {
        vk::ImageMemoryBarrier barrier;
        barrier.oldLayout                       = from;
        barrier.newLayout                       = to;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = flag;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (from == vk::ImageLayout::eColorAttachmentOptimal && to == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage      = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else if (from == vk::ImageLayout::eShaderReadOnlyOptimal && to == vk::ImageLayout::eColorAttachmentOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

            sourceStage      = vk::PipelineStageFlagBits::eFragmentShader;
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        }
        else if (from == vk::ImageLayout::eTransferDstOptimal && to == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage      = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::eColorAttachmentOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

            sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        }
        else if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::eDepthStencilAttachmentOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

            sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        }
        else if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::eGeneral)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eNone;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        }
        else if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else if (from == vk::ImageLayout::eGeneral && to == vk::ImageLayout::eTransferSrcOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (from == vk::ImageLayout::eGeneral && to == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (from == vk::ImageLayout::ePresentSrcKHR && to == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (from == vk::ImageLayout::eTransferSrcOptimal && to == vk::ImageLayout::eGeneral)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eNone;

            sourceStage      = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        }
        else if (from == vk::ImageLayout::eTransferDstOptimal && to == vk::ImageLayout::eGeneral)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eNone;

            sourceStage      = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        }
        else if (from == vk::ImageLayout::eUndefined && to == vk::ImageLayout::ePresentSrcKHR)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eNone;

            sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
            destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        mCommandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
    }

}  // namespace vk2s