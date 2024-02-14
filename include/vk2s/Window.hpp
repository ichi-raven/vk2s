/*****************************************************************/ /**
 * @file   Window.hpp
 * @brief  header file of window class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/
#ifndef VK2S_INCLUDE_WINDOW_HPP_
#define VK2S_INCLUDE_WINDOW_HPP_

#include "Macro.hpp"

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace vk2s
{
    class Device;
    class Semaphore;

    class Window
    {
    public:
        Window(Device& device, const uint32_t width, const uint32_t height, const uint32_t frameNum, std::string_view windowName, bool fullScreen = false);

        ~Window();

        NONCOPYABLE(Window);
        NONMOVABLE(Window);

        bool update();

        int getKey(const int key) const;

        std::pair<double, double> getMousePos() const;

        uint32_t acquireNextImage(Semaphore& signalSem);

        std::pair<uint32_t, uint32_t> getWindowSize() const;

        uint32_t getFrameCount() const;

        void present(const uint32_t frameBufferIndex, Semaphore& waitSem);

        void setFullScreen();

        void setWindowed();

        // internal-------------

        GLFWwindow* getpGLFWWindow();

        const std::vector<vk::Image>& getVkImages();

        const std::vector<vk::UniqueImageView>& getVkImageViews();

        vk::Format getVkSwapchainImageFormat() const;

        vk::Extent2D getVkSwapchainExtent() const;

    private:  // methods
        void initWindow(const bool fullScreen);

        void createSurface();

        void createSwapChain();

        void createImageViews();

        void createDepthBuffer();

        void setImageLayouts();

        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);

        vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);

        vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    private:  // member variables
        const uint32_t mWindowWidth;
        const uint32_t mWindowHeight;
        const uint32_t mMaxFramesInFlight;

        std::string_view mWindowName;

        Device& mDevice;

        GLFWwindow* mpWindow;
        vk::UniqueSurfaceKHR mSurface;
        vk::UniqueSwapchainKHR mSwapChain;
        std::vector<vk::Image> mSwapChainImages;
        vk::Format mSwapChainImageFormat;
        vk::Extent2D mSwapChainExtent;
        std::vector<vk::UniqueImageView> mSwapChainImageViews;
    };
}  // namespace vk2s

#endif