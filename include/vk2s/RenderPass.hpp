/*****************************************************************/ /**
 * \file   RenderPass.hpp
 * \brief  header file of RenderPass class
 * 
 * \author ichi-raven
 * \date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_RENDERPASS_HPP_
#define VK2S_INCLUDE_RENDERPASS_HPP_

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include "SlotMap.hpp"
#include "Macro.hpp"

namespace vk2s
{
    class Device;

    class Image;
    class Window;

    class RenderPass
    {
    public:  // methods

        RenderPass(Device& device, const vk::ArrayProxy<Handle<Image>>& colorTargets, const Handle<Image>& depthTarget = Handle<Image>(), const vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear);
        
        RenderPass(Device& device, const vk::ArrayProxy<Handle<Image>>& colorTargets, const vk::ArrayProxy<vk::AttachmentLoadOp>& loadOps, const Handle<Image>& depthTarget = Handle<Image>());

        RenderPass(Device& device, Window& window, const vk::AttachmentLoadOp colorLoadOp, const Handle<Image>& depthTarget = Handle<Image>(), const vk::AttachmentLoadOp depthLoadOp = vk::AttachmentLoadOp::eClear);

        ~RenderPass();

        NONCOPYABLE(RenderPass);
        NONMOVABLE(RenderPass);

        void recreateFrameBuffers(Window& window, const Handle<Image> depthTarget = Handle<Image>());

        const vk::UniqueRenderPass& getVkRenderPass();

        const std::vector<vk::UniqueFramebuffer>& getVkFrameBuffers();

    private: // methods

        //void createRenderPass(const vk::ArrayProxy<Handle<Image>>& colorTargets, const std::optional<Handle<Image>>& depthTargets);

        //void createFrameBuffer(const vk::ArrayProxy<Handle<Image>>& colorTargets, const std::optional<Handle<Image>>& depthTargets);

        vk::Format findSupportedFormat(const vk::ArrayProxy<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    private:  // member variables
        Device& mDevice;

        vk::UniqueRenderPass mRenderPass;
        std::vector<vk::UniqueFramebuffer> mFrameBuffers;
    };
}  // namespace vk2s

#endif