/*****************************************************************/ /**
 * @file   Device.hpp
 * @brief  header file of Device class
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/

#ifndef VK2S_INCLUDE_DEVICE_HPP_
#define VK2S_INCLUDE_DEVICE_HPP_

#include "SlotMap.hpp"
#include "Macro.hpp"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Window.hpp"
#include "Buffer.hpp"
#include "DynamicBuffer.hpp"
#include "Fence.hpp"
#include "Image.hpp"
#include "Sampler.hpp"
#include "RenderPass.hpp"
#include "Shader.hpp"
#include "BindLayout.hpp"
#include "Pipeline.hpp"
#include "BindGroup.hpp"
#include "Command.hpp"
#include "Semaphore.hpp"
#include "AccelerationStructure.hpp"
#include "ShaderBindingTable.hpp"

#include <optional>
#include <array>
#include <utility>
#include <tuple>
#include <type_traits>

namespace vk2s
{
    template <class T, class TypeList>
    struct IsContainedIn;

    template <class T, class... Ts>
    struct IsContainedIn<T, std::tuple<Ts...>> : std::bool_constant<(... || std::is_same<T, Ts>{})>
    {
    };

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }

        QueueFamilyIndices static findQueueFamilies(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& testSurface);
    };

    class Device
    {
    public:  // types
        struct SwapChainSupportDetails
        {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> presentModes;

            SwapChainSupportDetails static querySwapChainSupport(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& testSurface);
        };

    public:  // methods
        Device(const bool supportRayTracing = true);

        ~Device();

        NONCOPYABLE(Device);

        // TODO: BAD
        template <typename T, size_t PageSize = kDefaultPageSize, typename Allocator = DefaultAllocator, typename... Args>
        Handle<T, PageSize, DefaultAllocator> create(Args&&... args)
        {
            static_assert(IsContainedIn<Pool<T, PageSize, DefaultAllocator>, decltype(mPools)>::value, "invalid type of pool!");
            return std::get<Pool<T, PageSize, DefaultAllocator>>(mPools).allocate(*this, std::forward<Args>(args)...);
        }

        // TODO: BAD2
        template <typename T, size_t PageSize = kDefaultPageSize, typename Allocator = DefaultAllocator>
        bool destroy(Handle<T, PageSize, DefaultAllocator>& handle)
        {
            static_assert(IsContainedIn<Pool<T, PageSize, DefaultAllocator>, decltype(mPools)>::value, "invalid type of pool!");
            return std::get<Pool<T, PageSize, DefaultAllocator>>(mPools).deallocate(handle);
        }

        void initImGui(const uint32_t frameBufferNum, Window& window, RenderPass& renderpass);

        void destroyImGui();

        void waitIdle();

        std::string_view getPhysicalDeviceName() const;

        uint32_t getVkMemoryTypeIndex(uint32_t requestBits, vk::MemoryPropertyFlags requestProps) const;

        const vk::UniqueInstance& getVkInstance();

        const vk::PhysicalDevice& getVkPhysicalDevice();

        const vk::UniqueDevice& getVkDevice();

        const QueueFamilyIndices getVkQueueFamilyIndices();

        const vk::Queue& getVkGraphicsQueue();

        const vk::Queue& getVkPresentQueue();

        const vk::UniqueCommandPool& getVkCommandPool();

        // to trace allocation
        const std::pair<vk::DescriptorSet, size_t> allocateVkDescriptorSet(const vk::DescriptorSetLayout& layout, const DescriptorPoolAllocationInfo& allocInfo);

        void deallocateVkDescriptorSet(vk::DescriptorSet& set, const size_t poolIndex, const DescriptorPoolAllocationInfo& allocInfo);

        template <class T>
        T align(T size, uint32_t align)
        {
            return (size + align - 1) & ~static_cast<T>((align - 1));
        }

    private:  // types
        struct DescriptorPool
        {
            vk::UniqueDescriptorPool descriptorPool;
            DescriptorPoolAllocationInfo now;
        };

    private:  // compile time constant
#ifdef NDEBUG
        constexpr static bool enableValidationLayers = false;
#else
        constexpr static bool enableValidationLayers = true;
#endif

        constexpr static std::array validationLayers = { "VK_LAYER_KHRONOS_validation" };

        constexpr static std::array deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME };

        constexpr static std::array rayTracingDeviceExtensions = { VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };

        constexpr static std::array allExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                                      VK_KHR_RAY_QUERY_EXTENSION_NAME,
                                                      VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };

        constexpr static uint32_t kMaxDescriptorNum = 256;

    private:  // methods
        void createInstance();

        void setupDebugMessenger();

        void pickAndCreateDevice();

        void pickPhysicalDevice(const vk::UniqueSurfaceKHR& testSurface);

        void createLogicalDevice(const vk::UniqueSurfaceKHR& testSurface);

        void createCommandPool();

        void createDescriptorPool();

        // imgui--------------
        void initImGui();
        void uploadFontForImGui();
        void createDescriptorPoolForImGui();

        // utility
        std::vector<const char*> getRequiredExtensions();
        bool checkValidationLayerSupport();
        bool isDeviceSuitable(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& testSurface);
        bool checkDeviceExtensionSupport(vk::PhysicalDevice device) const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                                                          void* pUserData);

    private:  // member variables
        vk::UniqueInstance mInstance;
        vk::UniqueDebugUtilsMessengerEXT mDebugUtilsMessenger;

        const bool mRayTracingSupported;
        vk::PhysicalDevice mPhysicalDevice;
        vk::PhysicalDeviceMemoryProperties mPhysMemProps;
        vk::UniqueDevice mDevice;

        QueueFamilyIndices mQueueFamilyIndices;
        vk::Queue mGraphicsQueue;
        vk::Queue mPresentQueue;

        vk::UniqueCommandPool mCommandPool;

        std::vector<DescriptorPool> mDescriptorPools;

        // imgui--------------
        vk::UniqueDescriptorPool mDescriptorPoolForImGui;
        ImGuiContext* mpImGuiContext;

    private:  // pools
        std::tuple<Pool<Window>, Pool<Buffer>, Pool<Image>, Pool<Sampler>, Pool<RenderPass>, Pool<Shader>, Pool<BindLayout>, Pool<BindGroup>, Pool<Pipeline>, Pool<Semaphore>, Pool<Fence>, Pool<Command>, Pool<AccelerationStructure>,
                   Pool<ShaderBindingTable>, Pool<DynamicBuffer>>
            mPools;
    };
}  // namespace vk2s

#endif