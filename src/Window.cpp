
#include "../include/Window.hpp"

#include "../include/Device.hpp"

namespace vk2s
{
    Window::Window(Device& device, uint32_t width, uint32_t height, uint32_t frameNum, std::string_view windowName)
        : mWindowWidth(width)
        , mWindowHeight(height)
        , mMaxFramesInFlight(frameNum)
        , mWindowName(windowName)
        , mDevice(device)
    {
        initWindow();
        createSurface();
        createSwapChain();
        createImageViews();
        setImageLayouts();
    }

    Window::~Window()
    {
        mDevice.getVkDevice()->waitIdle();
        glfwDestroyWindow(mpWindow);
    }

    bool Window::update()
    {
        glfwPollEvents();

        return !glfwWindowShouldClose(mpWindow);
    }

    int Window::getKey(const int key) const
    {
        return glfwGetKey(mpWindow, key);
    }

    std::pair<double, double> Window::getMousePos() const
    {
        static double x, y;
        glfwGetCursorPos(mpWindow, &x, &y);
        return { x, y };
    }

    uint32_t Window::acquireNextImage(Semaphore& signalSem)
    {
        std::uint32_t rtnIndex = 0;

        vkAcquireNextImageKHR(mDevice.getVkDevice().get(), mSwapChain.get(), std::numeric_limits<std::uint64_t>::max(), signalSem.getVkSemaphore().get(), VK_NULL_HANDLE, &rtnIndex);

        return rtnIndex;
    }

    void Window::present(const uint32_t frameBufferIndex, Semaphore& waitSem)
    {
        assert(frameBufferIndex < mMaxFramesInFlight || !"invalid framebuffer index!");

        vk::PresentInfoKHR presentInfo(waitSem.getVkSemaphore().get(), mSwapChain.get(), frameBufferIndex);

        const auto res = mDevice.getVkGraphicsQueue().presentKHR(presentInfo);
        if (res != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present!");
        }
    }

    GLFWwindow* Window::getpGLFWWindow()
    {
        return mpWindow;
    }

    const std::vector<vk::Image>& Window::getVkImages()
    {
        return mSwapChainImages;
    }

    const std::vector<vk::UniqueImageView>& Window::getVkImageViews()
    {
        return mSwapChainImageViews;
    }

    vk::Format Window::getVkSwapchainImageFormat() const
    {
        return mSwapChainImageFormat;
    }

    vk::Extent2D Window::getVkSwapchainExtent() const
    {
        return mSwapChainExtent;
    }

    void Window::initWindow()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        mpWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, mWindowName.data(), nullptr, nullptr);

        // hide cursor
        glfwSetInputMode(mpWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    void Window::createSurface()
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(VkInstance(mDevice.getVkInstance().get()), mpWindow, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create mpWindow mSurface!");
        }
        mSurface = vk::UniqueSurfaceKHR(surface, { mDevice.getVkInstance().get() });
    }

    void Window::createSwapChain()
    {
        const auto& physDev = mDevice.getVkPhysicalDevice();

        Device::SwapChainSupportDetails details = Device::SwapChainSupportDetails::querySwapChainSupport(physDev, mSurface);

        vk::SurfaceFormatKHR mSurfaceFormat = chooseSwapSurfaceFormat(details.formats);
        vk::PresentModeKHR presentMode      = chooseSwapPresentMode(details.presentModes);
        vk::Extent2D extent                 = chooseSwapExtent(details.capabilities);

        uint32_t imageCount = details.capabilities.minImageCount + 1;

        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
        {
            imageCount = details.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo({}, mSurface.get(), imageCount, mSurfaceFormat.format, mSurfaceFormat.colorSpace, extent,
                                              /* imageArrayLayers = */ 1, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive, {}, details.capabilities.currentTransform,
                                              vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode,
                                              /* clipped = */ VK_TRUE, nullptr);

        Device::QueueFamilyIndices indices = Device::QueueFamilyIndices::findQueueFamilies(physDev, mSurface);
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
            std::array familyIndices = { indices.graphicsFamily.value(), indices.presentFamily.value() };
            createInfo.setQueueFamilyIndices(familyIndices);
        }

        const auto& device = mDevice.getVkDevice();

        mSwapChain            = device->createSwapchainKHRUnique(createInfo);
        mSwapChainImages      = device->getSwapchainImagesKHR(mSwapChain.get());
        mSwapChainImageFormat = mSurfaceFormat.format;
        mSwapChainExtent      = extent;
    }

    void Window::createImageViews()
    {
        const auto& device = mDevice.getVkDevice();

        mSwapChainImageViews.resize(mSwapChainImages.size());

        vk::ComponentMapping components(vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity);
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        for (size_t i = 0; i < mSwapChainImages.size(); i++)
        {
            vk::ImageViewCreateInfo createInfo({}, mSwapChainImages[i], vk::ImageViewType::e2D, mSwapChainImageFormat, components, subresourceRange);
            mSwapChainImageViews[i] = device->createImageViewUnique(createInfo);
        }
    }

    void Window::setImageLayouts()
    {
        const auto& vkDevice      = mDevice.getVkDevice();
        const auto& vkCommandPool = mDevice.getVkCommandPool();

        vk::CommandBufferAllocateInfo allocInfo(vkCommandPool.get(), vk::CommandBufferLevel::ePrimary, 1);

        auto commandBuffers = vkDevice->allocateCommandBuffers(allocInfo);
        auto& commandBuffer = commandBuffers.back();

        vk::ImageMemoryBarrier barrier;
        barrier.oldLayout                       = vk::ImageLayout::eUndefined;
        barrier.newLayout                       = vk::ImageLayout::ePresentSrcKHR;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask                   = vk::AccessFlagBits::eNone;

        const auto sourceStage      = vk::PipelineStageFlagBits::eAllCommands;
        const auto destinationStage = vk::PipelineStageFlagBits::eAllCommands;

        commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        for (auto& swapChainImage : mSwapChainImages)
        {
            barrier.image = swapChainImage;
            commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
        }
        commandBuffer.end();
        vk::SubmitInfo submitInfo(nullptr, nullptr, commandBuffer, nullptr);

        mDevice.getVkGraphicsQueue().submit(submitInfo);

        vkDevice->waitIdle();
        vkDevice->freeCommandBuffers(vkCommandPool.get(), commandBuffer);
    }

    vk::SurfaceFormatKHR Window::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == vk::Format::eR8G8B8A8Unorm)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR Window::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == vk::PresentModeKHR::eFifoRelaxed)
            {
                return availablePresentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Window::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(mpWindow, &width, &height);

            vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

}  // namespace vk2s
