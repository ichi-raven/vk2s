#include "../include/RenderPass.hpp"

#include "../include/Device.hpp"

#include <array>

namespace vkpt
{
    RenderPass::RenderPass(Device& device, const vk::ArrayProxy<Handle<Image>>& colorTargets, const Handle<Image> depthTarget)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        // TODO: control clear and dontcare

        std::vector<vk::AttachmentDescription> attachments;
        std::vector<vk::AttachmentReference> colorAttachmentRefs;
        attachments.reserve(colorTargets.size());
        colorAttachmentRefs.reserve(colorTargets.size());

        for (const auto& ct : colorTargets)
        {
            attachments.emplace_back(vk::AttachmentDescription({}, colorTargets.front()->getVkFormat(), vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
                                                               vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal));
            colorAttachmentRefs.emplace_back(vk::AttachmentReference(colorAttachmentRefs.size(), vk::ImageLayout::eColorAttachmentOptimal));
        }

        if (depthTarget)
        {
            vk::AttachmentDescription depthAttachment({}, depthTarget->getVkFormat(), vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare,
                                                      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            attachments.emplace_back(depthAttachment);

            vk::AttachmentReference depthAttachmentRef(colorAttachmentRefs.size(), vk::ImageLayout::eDepthStencilAttachmentOptimal);

            vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRefs, {}, &depthAttachmentRef);
            const auto srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
            const auto dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
            const auto dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, srcStageMask, dstStageMask, {}, dstAccessMask);
            vk::RenderPassCreateInfo renderPassInfo({}, attachments, subpass, dependency);
            mRenderPass = vkDevice->createRenderPassUnique(renderPassInfo);
        }
        else
        {
            vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRefs);
            const auto srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            const auto dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            const auto dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, srcStageMask, dstStageMask, {}, dstAccessMask);
            vk::RenderPassCreateInfo renderPassInfo({}, attachments, subpass, dependency);
            mRenderPass = vkDevice->createRenderPassUnique(renderPassInfo);
        }

        const auto extent = colorTargets.front()->getVkExtent();
        std::vector<vk::ImageView> views;
        views.reserve(colorTargets.size() + 1);
        for (const auto& ct : colorTargets)
        {
            views.emplace_back(ct->getVkImageView().get());
        }
        if (depthTarget)
        {
            views.emplace_back(depthTarget->getVkImageView().get());
        }

        vk::FramebufferCreateInfo framebufferInfo({}, mRenderPass.get(), views, extent.width, extent.height, 1);
        mFrameBuffers.push_back(vkDevice->createFramebufferUnique(framebufferInfo));
    }

    RenderPass::RenderPass(Device& device, Window& window, const Handle<Image> depthTarget)
        : mDevice(device)
    {
        const auto& vkDevice = mDevice.getVkDevice();

        vk::AttachmentDescription colorAttachment({}, window.getVkSwapchainImageFormat(), vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
                                                  vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

        if (depthTarget)
        {
            vk::AttachmentDescription depthAttachment({}, depthTarget->getVkFormat(), vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare,
                                                      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

            vk::AttachmentReference depthAttachmentRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

            vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef, {}, &depthAttachmentRef);
            const auto srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
            const auto dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
            const auto dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, srcStageMask, dstStageMask, {}, dstAccessMask);
            const std::array attachments = { colorAttachment, depthAttachment };
            vk::RenderPassCreateInfo renderPassInfo({}, attachments, subpass, dependency);
            mRenderPass = vkDevice->createRenderPassUnique(renderPassInfo);
        }
        else
        {
            vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef);
            const auto srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            const auto dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            const auto dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, srcStageMask, dstStageMask, {}, dstAccessMask);
            vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);
            mRenderPass = vkDevice->createRenderPassUnique(renderPassInfo);
        }

        const auto& swapchainImageViews = window.getVkImageViews();
        const auto extent               = window.getVkSwapchainExtent();

        mFrameBuffers.reserve(swapchainImageViews.size());

        for (const auto& view : swapchainImageViews)
        {
            std::vector attachments = { view.get() };
            if (depthTarget)
            {
                attachments.emplace_back(depthTarget->getVkImageView().get());
            }

            vk::FramebufferCreateInfo framebufferInfo({}, mRenderPass.get(), attachments, extent.width, extent.height, 1);
            mFrameBuffers.push_back(vkDevice->createFramebufferUnique(framebufferInfo));
        }
    }

    RenderPass::~RenderPass()
    {
    }

    const vk::UniqueRenderPass& RenderPass::getVkRenderPass()
    {
        return mRenderPass;
    }

    const std::vector<vk::UniqueFramebuffer>& RenderPass::getVkFrameBuffers()
    {
        return mFrameBuffers;
    }

    vk::Format RenderPass::findSupportedFormat(const vk::ArrayProxy<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
    {
        const auto& physDev = mDevice.getVkPhysicalDevice();

        for (const auto format : candidates)
        {
            vk::FormatProperties props = physDev.getFormatProperties(format);
            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        assert(!"failed to find supported format!");
        return candidates.front();
    }
}  // namespace vkpt