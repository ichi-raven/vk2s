#include "../include/vk2s/Device.hpp"

#include <GLFW/glfw3.h>

#include <iostream>
#include <set>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vk2s
{
    Device::Device(const bool useWindow)
        : Device(Extensions::useNothing(), useWindow)
    {
    }

    Device::Device(const Extensions extensions, const bool useWindow)
        : mQueriedExtensions(extensions)
        , mImGuiActive(false)
    {
        glfwInit();

        createInstance();
        setupDebugMessenger();
        pickAndCreateDevice(useWindow);
        createCommandPool();

        createDescriptorPoolForImGui();

        createDescriptorPool();
    }

    template <size_t N = 0, typename T>
    void iterateTupleAndClear(T& t)
    {
        if constexpr (N < std::tuple_size<T>::value)
        {
            auto& x = std::get<N>(t);
            x.clear();
            iterateTupleAndClear<N + 1>(t);
        }
    }

    Device::~Device()
    {
        mDevice->waitIdle();

        iterateTupleAndClear(mPools);

        destroyImGui();
        if (ImGui::GetCurrentContext())
        {
            ImGui::DestroyContext();
        }

        glfwTerminate();
    }

    void Device::waitIdle()
    {
        mDevice->waitIdle();
    }

    void Device::createDescriptorPoolForImGui()
    {
        vk::DescriptorPoolSize size(vk::DescriptorType::eCombinedImageSampler, 1);

        vk::DescriptorPoolCreateInfo ci(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, size);

        mDescriptorPoolForImGui = mDevice->createDescriptorPoolUnique(ci);
    }

    void Device::initImGui(Window& window, RenderPass& renderpass)
    {
        // if the context is not set, create a new one
        if (!ImGui::GetCurrentContext())
        {
            ImGui::CreateContext();
        }

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
        init_info.ImageCount                = window.getFrameCount();
        init_info.CheckVkResultFn           = nullptr;
        init_info.RenderPass                = renderpass.getVkRenderPass().get();
        ImGui_ImplVulkan_Init(&init_info);

        mImGuiActive = true;
    }

    void Device::destroyImGui()
    {
        if (mImGuiActive)
        {
            //ImGui_ImplVulkan_DestroyFontsTexture();
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            mImGuiActive = false;
        }
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

    const QueueFamilyIndices Device::getVkQueueFamilyIndices() const
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

    const std::pair<vk::DescriptorSet, size_t> Device::allocateVkDescriptorSet(const vk::DescriptorSetLayout& layout, const DescriptorPoolAllocationInfo& allocInfo)
    {
        size_t idx = 0;
        for (const auto& pool : mDescriptorPools)
        {
            if (pool.now.accelerationStructureNum < allocInfo.accelerationStructureNum || pool.now.combinedImageSamplerNum < allocInfo.combinedImageSamplerNum || pool.now.sampledImageNum < allocInfo.sampledImageNum ||
                pool.now.samplerNum < allocInfo.samplerNum || pool.now.storageBufferNum < allocInfo.storageBufferNum || pool.now.storageImageNum < allocInfo.storageImageNum || pool.now.uniformBufferNum < allocInfo.uniformBufferNum ||
                pool.now.uniformBufferDynamicNum < allocInfo.uniformBufferDynamicNum)
            {
                break;
            }
            ++idx;
        }

        if (idx == mDescriptorPools.size())
        {
            createDescriptorPool();
        }

        auto& allocatedPool = mDescriptorPools[idx];
        allocatedPool.now.accelerationStructureNum -= allocInfo.accelerationStructureNum;
        allocatedPool.now.combinedImageSamplerNum -= allocInfo.combinedImageSamplerNum;
        allocatedPool.now.sampledImageNum -= allocInfo.sampledImageNum;
        allocatedPool.now.samplerNum -= allocInfo.samplerNum;
        allocatedPool.now.storageBufferNum -= allocInfo.storageBufferNum;
        allocatedPool.now.storageImageNum -= allocInfo.storageImageNum;
        allocatedPool.now.uniformBufferNum -= allocInfo.uniformBufferNum;
        allocatedPool.now.uniformBufferDynamicNum -= allocInfo.uniformBufferDynamicNum;

        vk::DescriptorSetAllocateInfo ai(allocatedPool.descriptorPool.get(), layout);

        return { mDevice->allocateDescriptorSets(ai).front(), idx };
    }

    void Device::deallocateVkDescriptorSet(vk::DescriptorSet& set, const size_t poolIndex, const DescriptorPoolAllocationInfo& allocInfo)
    {
        auto& allocatedPool = mDescriptorPools[poolIndex];

        allocatedPool.now.accelerationStructureNum += allocInfo.accelerationStructureNum;
        allocatedPool.now.combinedImageSamplerNum += allocInfo.combinedImageSamplerNum;
        allocatedPool.now.sampledImageNum += allocInfo.sampledImageNum;
        allocatedPool.now.samplerNum += allocInfo.samplerNum;
        allocatedPool.now.storageBufferNum += allocInfo.storageBufferNum;
        allocatedPool.now.storageImageNum += allocInfo.storageImageNum;
        allocatedPool.now.uniformBufferNum += allocInfo.uniformBufferNum;
        allocatedPool.now.uniformBufferDynamicNum += allocInfo.uniformBufferDynamicNum;

        mDevice->freeDescriptorSets(allocatedPool.descriptorPool.get(), set);
    }

#if VK_HEADER_VERSION >= 301
    using VulkanDynamicLoader = vk::detail::DynamicLoader;
#else
    using VulkanDynamicLoader = vk::DynamicLoader;
#endif

    void Device::createInstance()
    {
        // TODO: change application name
        constexpr std::string_view applicationName = "vk2s application";

        // get the instance independent function pointers
        static VulkanDynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        if (enableValidationLayers && !(checkInstanceLayerSupport() && checkInstanceExtensionSupport()))
        {
            throw std::runtime_error("instance extension layers requested, but not available!");
        }

        vk::ApplicationInfo appInfo(applicationName.data(), VK_MAKE_VERSION(1, 0, 0), "vk2s", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

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

    void Device::pickAndCreateDevice(const bool useWindow)
    {
        // HACK: create test surface
        if (useWindow)
        {
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
        else
        {
            pickPhysicalDevice(vk::UniqueSurfaceKHR());

            createLogicalDevice(vk::UniqueSurfaceKHR());
        }
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

        vk::PhysicalDeviceFeatures deviceFeatures{};
        // for ray tracing
        vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR enabledBufferDeviceAddressFeatures(VK_TRUE);

        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures(VK_TRUE);
        enabledRayTracingPipelineFeatures.setRayTracingPipelineTraceRaysIndirect(VK_TRUE);
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

        // for NV motion blur extension
        vk::PhysicalDeviceRayTracingMotionBlurFeaturesNV enabledRTMotionBlurFeatures(VK_TRUE);

        vk::PhysicalDeviceRobustness2FeaturesEXT robustness2Features(VK_TRUE, VK_TRUE, VK_TRUE);

        vk::PhysicalDeviceVulkan13Features vk1_3features;
        vk1_3features.maintenance4 = VK_TRUE;
        vk1_3features.pNext        = &robustness2Features;

        vk::PhysicalDeviceFeatures features = mPhysicalDevice.getFeatures();

        vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2(features, &vk1_3features);

        vk::DeviceCreateInfo createInfo({}, queueCreateInfos, {}, {}, &deviceFeatures);

        std::vector<const char*> extensionNames(baseDeviceExtensions.begin(), baseDeviceExtensions.end());
        extensionNames.reserve(allDeviceExtensions.size());

        void** ppNext = &(robustness2Features.pNext);

        if (mQueriedExtensions.useExternalMemoryExt)
        {
            extensionNames.resize(extensionNames.size() + externalMemoryDeviceExtensions.size());
            std::copy(externalMemoryDeviceExtensions.begin(), externalMemoryDeviceExtensions.end(), extensionNames.end() - externalMemoryDeviceExtensions.size());
        }

        if (mQueriedExtensions.useRayTracingExt)
        {
            *ppNext = &enabledDescriptorIndexingFeatures;
            ppNext  = &(enabledBufferDeviceAddressFeatures.pNext);

            extensionNames.resize(extensionNames.size() + rayTracingDeviceExtensions.size());
            std::copy(rayTracingDeviceExtensions.begin(), rayTracingDeviceExtensions.end(), extensionNames.end() - rayTracingDeviceExtensions.size());

            if (mQueriedExtensions.useNVMotionBlurExt)
            {
                *ppNext = &enabledRTMotionBlurFeatures;
                ppNext = &(enabledRTMotionBlurFeatures.pNext);
                extensionNames.emplace_back(nvRayTracingMotionBlurExtension);
            }
        }

        createInfo.pNext                   = &physicalDeviceFeatures2;
        createInfo.pEnabledFeatures        = nullptr;
        createInfo.enabledExtensionCount   = extensionNames.size();
        createInfo.ppEnabledExtensionNames = extensionNames.data();

        if (enableValidationLayers)
        {
            createInfo.setPEnabledLayerNames(validationLayers);
        }

        mDevice = mPhysicalDevice.createDeviceUnique(createInfo);
        assert(mDevice || !"failed to create logical device!");

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
        std::vector poolSize = {
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, kMaxDescriptorNum), vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, kMaxDescriptorNum),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, kMaxDescriptorNum),        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, kMaxDescriptorNum),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, kMaxDescriptorNum),        vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, kMaxDescriptorNum),
        };

        if (!mQueriedExtensions.useRayTracingExt)
        {
            poolSize.pop_back();
        }

        auto& added = mDescriptorPools.emplace_back();

        vk::DescriptorPoolCreateInfo ci(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, kMaxDescriptorNum, poolSize);
        added.descriptorPool = mDevice->createDescriptorPoolUnique(ci);
        added.now            = DescriptorPoolAllocationInfo(kMaxDescriptorNum);
    }

    // utility----------------------------------------------

    bool Device::checkInstanceLayerSupport()
    {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

        std::vector<const char*> requestedLayers(validationLayers.begin(), validationLayers.end());

        for (const char* layerName : requestedLayers)
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

    bool Device::checkInstanceExtensionSupport()
    {
        const auto availableExtensions = vk::enumerateInstanceExtensionProperties();

        std::vector<const char*> requiredExtensions(allInstanceExtensions.begin(), allInstanceExtensions.end());

        for (const char* extension : requiredExtensions)
        {
            bool extFound = false;
            for (const auto& extensionProperties : availableExtensions)
            {
                if (strcmp(extension, extensionProperties.extensionName) == 0)
                {
                    extFound = true;
                    break;
                }
            }
            if (!extFound)
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

        if (mQueriedExtensions.useExternalMemoryExt)
        {
            std::copy(externalMemoryInstanceExtensions.begin(), externalMemoryInstanceExtensions.end(), std::back_inserter(extensions));
        }

        return extensions;
    }

    bool Device::isDeviceSuitable(vk::PhysicalDevice mDevice, const vk::UniqueSurfaceKHR& testSurface)
    {
        QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(mDevice, testSurface);

        bool extensionsSupported = checkDeviceExtensionSupport(mDevice);

        bool swapChainAdequate = false;

        if (!testSurface)
        {
            return indices.isComplete() && extensionsSupported;
        }

        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = SwapChainSupportDetails::querySwapChainSupport(mDevice, testSurface);
            swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        else
        {
            throw std::runtime_error("extension is not supported!");
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    QueueFamilyIndices QueueFamilyIndices::findQueueFamilies(vk::PhysicalDevice physDev, const vk::UniqueSurfaceKHR& testSurface)
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

            if (testSurface)
            {
                const auto presentSupport = physDev.getSurfaceSupportKHR(i, testSurface.get());
                if (presentSupport)
                {
                    indices.presentFamily = i;
                }
            }
            else
            {
                // don't care about presenting
                indices.presentFamily = indices.graphicsFamily;
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
        if (testSurface)
        {
            details.capabilities = physDev.getSurfaceCapabilitiesKHR(testSurface.get());
            details.formats      = physDev.getSurfaceFormatsKHR(testSurface.get());
            details.presentModes = physDev.getSurfacePresentModesKHR(testSurface.get());
        }

        return details;
    }

    bool Device::checkDeviceExtensionSupport(vk::PhysicalDevice mDevice) const
    {
        std::vector<vk::ExtensionProperties> availableExtensions = mDevice.enumerateDeviceExtensionProperties();

        std::set<std::string> requiredExtensions(baseDeviceExtensions.begin(), baseDeviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        if (mQueriedExtensions.useRayTracingExt)
        {
            std::set<std::string> requiredRTExtensions(rayTracingDeviceExtensions.begin(), rayTracingDeviceExtensions.end());
            for (const auto& extension : availableExtensions)
            {
                requiredRTExtensions.erase(extension.extensionName);
            }

            if (mQueriedExtensions.useNVMotionBlurExt)
            {
                for (const auto& extension : availableExtensions)
                {
                    if (std::strcmp(nvRayTracingMotionBlurExtension, extension.extensionName) == 0)
                    {
                        return requiredExtensions.empty() && requiredRTExtensions.empty();
                    }
                }

                return false;
            }

            return requiredExtensions.empty() && requiredRTExtensions.empty();
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

    Device::Extensions Device::getVkAvailableExtensions() const
    {
        return mQueriedExtensions;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Device::debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                                                       void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

}  // namespace vk2s