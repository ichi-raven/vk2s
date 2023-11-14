#include "../include/Device.hpp"

#include <GLFW/glfw3.h>

#include <iostream>
#include <set>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vkpt
{
    Device::Device()
        : mpImGuiContext(nullptr)
    {
        glfwInit();

        createInstance();
        setupDebugMessenger();
        pickAndCreateDevice();
        createCommandPool();

        createDescriptorPoolForImGui();

        createDescriptorPool();
    }

    template <size_t N = 0, typename T>
    void iterateTuple(T& t)
    {
        if constexpr (N < std::tuple_size<T>::value)
        {
            auto& x = std::get<N>(t);
            x.clear();
            iterateTuple<N + 1>(t);
        }
    }

    Device::~Device()
    {
        iterateTuple(mPools);
        
        if (mpImGuiContext)
        {
            //ImGui_ImplVulkan_DestroyFontsTexture();
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        
        glfwTerminate();
    }

    void Device::createDescriptorPoolForImGui()
    {
        vk::DescriptorPoolSize size(vk::DescriptorType::eCombinedImageSampler, 1);

        vk::DescriptorPoolCreateInfo ci({}, 1, size);

        mDescriptorPoolForImGui = mDevice->createDescriptorPoolUnique(ci);
    }

    void Device::initImGui(const uint32_t frameBufferNum, Window& window, RenderPass& renderpass)
    {
        mpImGuiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(mpImGuiContext);
        ImGui_ImplGlfw_InitForVulkan(window.getpGLFWWindow(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = mInstance.get();
        init_info.PhysicalDevice            = mPhysicalDevice;
        init_info.Device                    = mDevice.get();
        init_info.QueueFamily               = mQueueFamilyIndices.graphicsFamily.value();
        init_info.Queue                     = mGraphicsQueue;
        init_info.PipelineCache             = VK_NULL_HANDLE;
        init_info.DescriptorPool            = mDescriptorPoolForImGui.get();
        init_info.Allocator                 = VK_NULL_HANDLE;
        init_info.MinImageCount             = 2;
        init_info.ImageCount                = frameBufferNum;
        init_info.CheckVkResultFn           = nullptr;
        ImGui_ImplVulkan_Init(&init_info, renderpass.getVkRenderPass().get());

        uploadFontForImGui();
    }

    void Device::uploadFontForImGui()
    {
        //impl fonts
        //ImGui_ImplVulkan_CreateFontsTexture();
    }

    std::string_view Device::getPhysicalDeviceName() const
    {
        return mPhysicalDevice.getProperties().deviceName;
    }

    const vk::UniqueInstance& Device::getVkInstance()
    {
        return mInstance;
    }

    const vk::PhysicalDevice& Device::getVkPhysicalDevice()
    {
        return mPhysicalDevice;
    }

    const vk::UniqueDevice& Device::getVkDevice()
    {
        return mDevice;
    }

    const Device::QueueFamilyIndices Device::getVkQueueFamilyIndices()
    {
        return mQueueFamilyIndices;
    }

    const vk::Queue& Device::getVkGraphicsQueue()
    {
        return mGraphicsQueue;
    }

    const vk::Queue& Device::getVkPresentQueue()
    {
        return mPresentQueue;
    }

    const vk::UniqueCommandPool& Device::getVkCommandPool()
    {
        return mCommandPool;
    }

    const std::vector<vk::DescriptorSet> Device::allocateVkDescriptorSets(const std::vector<vk::DescriptorSetLayout>& layouts)
    {
        // TODO: implement descriptor allocator
        vk::DescriptorSetAllocateInfo ai(mDescriptorPool.get(), layouts);

        return mDevice->allocateDescriptorSets(ai);
    }

    void Device::createInstance()
    {
        // TODO: change application name
        constexpr std::string_view applicationName = "vkpt";

        // get the instance independent function pointers
        static vk::DynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        vk::ApplicationInfo appInfo(applicationName.data(), VK_MAKE_VERSION(1, 0, 0), "Vulkan Playground", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

        const auto extensions = getRequiredExtensions();

        if (enableValidationLayers)
        {
            // in debug mode, use the debugUtilsMessengerCallback
            vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

            vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

            vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> createInfo({ {}, &appInfo, validationLayers, extensions }, { {}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback });
            mInstance = vk::createInstanceUnique(createInfo.get<vk::InstanceCreateInfo>());
        }
        else
        {
            // in non-debug mode
            vk::InstanceCreateInfo createInfo({}, &appInfo, {}, extensions);
            mInstance = vk::createInstanceUnique(createInfo, nullptr);
        }

        // get all the other function pointers
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*mInstance);
    }

    void Device::setupDebugMessenger()
    {
        if (!enableValidationLayers)
        {
            return;
        }

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        mDebugUtilsMessenger = mInstance->createDebugUtilsMessengerEXTUnique(vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback));
    }

    void Device::pickAndCreateDevice()
    {
        // HACK: create test surface
        VkSurfaceKHR surface;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        const auto pTestWindow = glfwCreateWindow(1, 1, "surface test", nullptr, nullptr);
        auto res               = glfwCreateWindowSurface(VkInstance(mInstance.get()), pTestWindow, nullptr, &surface);
        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        const auto testSurface = vk::UniqueSurfaceKHR(surface, { mInstance.get() });

        pickPhysicalDevice(testSurface);

        createLogicalDevice(testSurface);

        glfwDestroyWindow(pTestWindow);
    }

    void Device::pickPhysicalDevice(const vk::UniqueSurfaceKHR& testSurface)
    {
        std::vector<vk::PhysicalDevice> physDevs = mInstance->enumeratePhysicalDevices();

        for (const auto& physDev : physDevs)
        {
            if (isDeviceSuitable(physDev, testSurface))
            {
                mPhysicalDevice = physDev;
                break;
            }
        }

        if (!mPhysicalDevice)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        mPhysMemProps = mPhysicalDevice.getMemoryProperties();
    }

    void Device::createLogicalDevice(const vk::UniqueSurfaceKHR& testSurface)
    {
        mQueueFamilyIndices = QueueFamilyIndices::findQueueFamilies(mPhysicalDevice, testSurface);

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { mQueueFamilyIndices.graphicsFamily.value(), mQueueFamilyIndices.presentFamily.value() };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1, &queuePriority);
            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::PhysicalDeviceFeatures mDeviceFeatures{};

        vk::DeviceCreateInfo createInfo({}, queueCreateInfos, {}, deviceExtensions, &mDeviceFeatures);

        // for ray tracing
        vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR enabledBufferDeviceAddressFeatures(VK_TRUE);

        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures(VK_TRUE);
        enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddressFeatures;

        vk::PhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerataionStuctureFeatures(VK_TRUE);
        enabledAccelerataionStuctureFeatures.pNext = &enabledRayTracingPipelineFeatures;

        vk::PhysicalDeviceDescriptorIndexingFeatures enabledDescriptorIndexingFeatures;
        enabledDescriptorIndexingFeatures.pNext                                      = &enabledAccelerataionStuctureFeatures;
        enabledDescriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
        enabledDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing  = VK_TRUE;
        enabledDescriptorIndexingFeatures.descriptorBindingPartiallyBound            = VK_TRUE;
        enabledDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount   = VK_TRUE;
        enabledDescriptorIndexingFeatures.runtimeDescriptorArray                     = VK_TRUE;

        vk::PhysicalDeviceFeatures features = mPhysicalDevice.getFeatures();

        vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2(features, &enabledDescriptorIndexingFeatures);

        createInfo.pNext            = &physicalDeviceFeatures2;
        createInfo.pEnabledFeatures = nullptr;

        if (enableValidationLayers)
        {
            createInfo.setPEnabledLayerNames(validationLayers);
        }

        mDevice        = mPhysicalDevice.createDeviceUnique(createInfo);
        mGraphicsQueue = mDevice->getQueue(mQueueFamilyIndices.graphicsFamily.value(), 0);
        mPresentQueue  = mDevice->getQueue(mQueueFamilyIndices.presentFamily.value(), 0);
    }

    void Device::createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo({}, mQueueFamilyIndices.graphicsFamily.value());
        poolInfo.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        mCommandPool = mDevice->createCommandPoolUnique(poolInfo);
    }

    void Device::createDescriptorPool()
    {
        std::array arr = {
            vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 32), vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 32),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 32),     vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 32),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 32),
        };

        vk::DescriptorPoolCreateInfo ci(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 32, arr);
        mDescriptorPool = mDevice->createDescriptorPoolUnique(ci);
    }

    // utility----------------------------------------------

    bool Device::checkValidationLayerSupport()
    {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

        for (const char* layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
            {
                return false;
            }
        }
        return true;
    }

    std::vector<const char*> Device::getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool Device::isDeviceSuitable(vk::PhysicalDevice mDevice, const vk::UniqueSurfaceKHR& testSurface)
    {
        QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(mDevice, testSurface);

        bool extensionsSupported = checkDeviceExtensionSupport(mDevice);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = SwapChainSupportDetails::querySwapChainSupport(mDevice, testSurface);
            swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    Device::QueueFamilyIndices Device::QueueFamilyIndices::findQueueFamilies(vk::PhysicalDevice physDev, const vk::UniqueSurfaceKHR& testSurface)
    {
        QueueFamilyIndices indices;

        std::vector<vk::QueueFamilyProperties> queueFamilies = physDev.getQueueFamilyProperties();

        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = physDev.getSurfaceSupportKHR(i, testSurface.get());
            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    Device::SwapChainSupportDetails Device::SwapChainSupportDetails::querySwapChainSupport(vk::PhysicalDevice physDev, const vk::UniqueSurfaceKHR& testSurface)
    {
        SwapChainSupportDetails details;
        details.capabilities = physDev.getSurfaceCapabilitiesKHR(testSurface.get());
        details.formats      = physDev.getSurfaceFormatsKHR(testSurface.get());
        details.presentModes = physDev.getSurfacePresentModesKHR(testSurface.get());

        return details;
    }

    bool Device::checkDeviceExtensionSupport(vk::PhysicalDevice mDevice)
    {
        std::vector<vk::ExtensionProperties> availableExtensions = mDevice.enumerateDeviceExtensionProperties();

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    uint32_t Device::getVkMemoryTypeIndex(uint32_t requestBits, vk::MemoryPropertyFlags requestProps) const
    {
        std::uint32_t result = 0;
        for (std::uint32_t i = 0; i < mPhysMemProps.memoryTypeCount; ++i)
        {
            if (requestBits & 1)
            {
                const auto& types = mPhysMemProps.memoryTypes[i];
                if ((types.propertyFlags & requestProps) == requestProps)
                {
                    result = i;
                    break;
                }
            }

            requestBits >>= 1;
        }
        return result;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Device::debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                                                       void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

}  // namespace vkpt