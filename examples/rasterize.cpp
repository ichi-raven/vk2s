#include <iostream>

#include <vk2s/Device.hpp>

#include "utility.hpp"

struct SceneUB  // std430
{
    glm::mat4 view;
    glm::mat4 proj;
};

struct InstanceUB
{
    glm::mat4 model;
    uint32_t matIndex;
    float padding[3];
};

void rasterize(uint32_t windowWidth, uint32_t windowHeight, const uint32_t frameCount)
{
    try
    {
        vk2s::Device device;

        auto window = device.create<vk2s::Window>(windowWidth, windowHeight, frameCount, "rasterize window");

        // create depth buffer
        Handle<vk2s::Image> depthBuffer;
        {
            const auto format   = vk::Format::eD32Sfloat;
            const uint32_t size = windowWidth * windowHeight * vk2s::Compiler::getSizeOfFormat(format);

            vk::ImageCreateInfo ci;
            ci.arrayLayers   = 1;
            ci.extent        = vk::Extent3D(windowWidth, windowHeight, 1);
            ci.format        = format;
            ci.imageType     = vk::ImageType::e2D;
            ci.mipLevels     = 1;
            ci.usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            ci.initialLayout = vk::ImageLayout::eUndefined;

            depthBuffer = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eDepth);
        }

        auto renderpass = device.create<vk2s::RenderPass>(window.get(), vk::AttachmentLoadOp::eClear, depthBuffer);

        device.initImGui(frameCount, window.get(), renderpass.get());

        // load meshes and materials
        std::vector<MeshInstance> meshInstances;
        Handle<vk2s::Buffer> materialBuffer;
        std::vector<Handle<vk2s::Image>> materialTextures;
        auto sampler = device.create<vk2s::Sampler>(vk::SamplerCreateInfo());
        vk2s::AssetLoader loader;

        load("../../examples/resources/model/CornellBox/CornellBox-Sphere.obj", device, loader, meshInstances, materialBuffer, materialTextures);
        //load("../../examples/resources/model/plototype1/prototype1.obj", device, loader, meshInstances, materialBuffer, materialTextures);

        // create envmap
        Handle<vk2s::Image> envmap;
        Handle<vk2s::Sampler> envmapSampler;
        {
            const auto& hostTexture = loader.loadTexture("../../examples/resources/envmap1.png", "envmap");
            const auto width        = hostTexture.width;
            const auto height       = hostTexture.height;
            const auto size         = width * height * static_cast<uint32_t>(STBI_rgb_alpha);

            vk::ImageCreateInfo ci;
            ci.arrayLayers   = 1;
            ci.extent        = vk::Extent3D(width, height, 1);
            ci.format        = vk::Format::eR8G8B8A8Srgb;
            ci.imageType     = vk::ImageType::e2D;
            ci.mipLevels     = 1;
            ci.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
            ci.initialLayout = vk::ImageLayout::eUndefined;

            envmap = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            envmap->write(hostTexture.pData, size);

            vk::SamplerCreateInfo sci;
            sci.magFilter = vk::Filter::eLinear;
            envmapSampler = device.create<vk2s::Sampler>(sci);
        }

        // craete shaders
        auto vertexShader   = device.create<vk2s::Shader>("../../examples/shaders/rasterize/vertex.vert", "main");
        auto fragmentShader = device.create<vk2s::Shader>("../../examples/shaders/rasterize/fragment.frag", "main");

        std::vector bindings0 = { vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eAll),
                                  vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAll),
                                  vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, std::max(1ull, materialTextures.size()), vk::ShaderStageFlagBits::eAll),
                                  vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAll) };

        std::vector bindings1 = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAll),
        };

        auto sceneBindLayout    = device.create<vk2s::BindLayout>(bindings0);
        auto materialBindLayout = device.create<vk2s::BindLayout>(bindings1);
        std::array allLayouts   = { sceneBindLayout, materialBindLayout };

        vk::VertexInputBindingDescription inputBinding(0, sizeof(vk2s::AssetLoader::Vertex));
        vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.0f, 1.0f);
        vk::Rect2D scissor({ 0, 0 }, window->getVkSwapchainExtent());
        vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
        colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk2s::Pipeline::VkGraphicsPipelineInfo gpi{
            .vs            = vertexShader,
            .fs            = fragmentShader,
            .bindLayouts   = allLayouts,
            .renderPass    = renderpass,
            .inputState    = vk::PipelineVertexInputStateCreateInfo({}, inputBinding, std::get<0>(vertexShader->getReflection())),
            .inputAssembly = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList),
            .viewportState = vk::PipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor),
            .rasterizer    = vk::PipelineRasterizationStateCreateInfo({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f),
            .multiSampling = vk::PipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1, VK_FALSE),
            .depthStencil  = vk::PipelineDepthStencilStateCreateInfo({}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE),
            .colorBlending = vk::PipelineColorBlendStateCreateInfo({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment),
        };

        auto graphicsPipeline = device.create<vk2s::Pipeline>(gpi);

        // uniform buffer
        Handle<vk2s::DynamicBuffer> sceneBuffer;
        {
            const auto size = sizeof(SceneUB) * frameCount;
            sceneBuffer     = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, size, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameCount);
        }

        std::vector<Handle<vk2s::Buffer>> instanceBuffers(meshInstances.size());
        for (uint32_t i = 0; auto& mesh : meshInstances)
        {
            const auto size    = sizeof(InstanceUB) * frameCount;
            instanceBuffers[i] = device.create<vk2s::Buffer>(vk::BufferCreateInfo({}, size, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            InstanceUB data{
                .model    = glm::mat4(1.0),
                .matIndex = i,
                .padding  = { 0.0 },
            };

            instanceBuffers[i]->write(&data, sizeof(InstanceUB));

            ++i;
        }

        // create bindgroup
        auto sceneBindGroup = device.create<vk2s::BindGroup>(sceneBindLayout.get());

        sceneBindGroup->bind(0, vk::DescriptorType::eUniformBufferDynamic, sceneBuffer.get());
        // instance UB will bind at recording commands
        sceneBindGroup->bind(1, vk::DescriptorType::eStorageBuffer, materialBuffer.get());
        if (materialTextures.empty())
        {
            sceneBindGroup->bind(2, vk::DescriptorType::eCombinedImageSampler, envmap, sampler);  // dummy
        }
        else
        {
            sceneBindGroup->bind(2, vk::DescriptorType::eCombinedImageSampler, materialTextures, sampler);
        }
        sceneBindGroup->bind(3, vk::DescriptorType::eCombinedImageSampler, envmap, envmapSampler);

        std::vector<Handle<vk2s::BindGroup>> materialBindGroups;
        materialBindGroups.resize(instanceBuffers.size());

        for (uint32_t i = 0; auto& ib : instanceBuffers)
        {
            materialBindGroups[i] = device.create<vk2s::BindGroup>(materialBindLayout.get());
            materialBindGroups[i]->bind(0, vk::DescriptorType::eUniformBuffer, ib.get());
            ++i;
        }

        // create commands and sync objects
        std::vector<Handle<vk2s::Command>> commands(frameCount);
        std::vector<Handle<vk2s::Semaphore>> imageAvailableSems(frameCount);
        std::vector<Handle<vk2s::Semaphore>> renderCompletedSems(frameCount);
        std::vector<Handle<vk2s::Fence>> fences(frameCount);

        for (int i = 0; i < frameCount; ++i)
        {
            commands[i]            = device.create<vk2s::Command>();
            imageAvailableSems[i]  = device.create<vk2s::Semaphore>();
            renderCompletedSems[i] = device.create<vk2s::Semaphore>();
            fences[i]              = device.create<vk2s::Fence>();
        }

        const auto colorClearValue   = vk::ClearValue(std::array{ 0.2f, 0.2f, 0.2f, 0.0f });
        const auto depthClearValue   = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0));
        const std::array clearValues = { colorClearValue, depthClearValue };
        double lastTime              = 0;
        vk2s::Camera camera(60., 1. * windowWidth / windowHeight);
        camera.setPos(glm::vec3(0.0, 0.8, 3.0));
        camera.setLookAt(glm::vec3(0.0, 0.8, -2.0));
        int inputSpp = 1;
        bool resized = false;

        for (uint32_t now = 0; window->update() && !window->getKey(GLFW_KEY_ESCAPE); now = (now + 1) % frameCount)
        {
            // update time
            const double currentTime = glfwGetTime();
            float deltaTime          = static_cast<float>(currentTime - lastTime);
            lastTime                 = currentTime;

            // update camera
            const double speed      = 2.0f * deltaTime;
            const double mouseSpeed = 0.7f * deltaTime;
            camera.update(window->getpGLFWWindow(), speed, mouseSpeed);

            // ImGui
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("configuration");
            ImGui::Text("device = %s", device.getPhysicalDeviceName().data());
            ImGui::Text("fps = %lf", 1. / deltaTime);
            const auto& pos    = camera.getPos();
            const auto& lookAt = camera.getLookAt();
            ImGui::Text("pos = (%lf, %lf, %lf)", pos.x, pos.y, pos.z);
            ImGui::Text("lookat = (%lf, %lf, %lf)", lookAt.x, lookAt.y, lookAt.z);

            ImGui::End();

            ImGui::Render();

            // wait and reset fence
            fences[now]->wait();

            // acquire next image from swapchain(window)
            const auto [imageIndex, resize] = window->acquireNextImage(imageAvailableSems[now].get());

            {  // write data
                SceneUB sceneUBO{
                    .view = camera.getViewMatrix(),
                    .proj = camera.getProjectionMatrix(),
                };

                sceneBuffer->write(&sceneUBO, sizeof(SceneUB), now * sceneBuffer->getBlockSize());
            }

            if (resize || resized)
            {
                const auto [width, height] = window->getWindowSize();
                windowWidth                = width;
                windowHeight               = height;
                window->resize();
                {
                    const auto format   = vk::Format::eD32Sfloat;
                    const uint32_t size = width * height * vk2s::Compiler::getSizeOfFormat(format);

                    vk::ImageCreateInfo ci;
                    ci.arrayLayers   = 1;
                    ci.extent        = vk::Extent3D(width, height, 1);
                    ci.format        = format;
                    ci.imageType     = vk::ImageType::e2D;
                    ci.mipLevels     = 1;
                    ci.usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    ci.initialLayout = vk::ImageLayout::eUndefined;

                    depthBuffer = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eDepth);
                }
                renderpass->recreateFrameBuffers(window.get(), depthBuffer);
                resized = false;
                continue;
            }

            fences[now]->reset();

            auto& command = commands[now];
            // start writing command
            command->begin();
            command->beginRenderPass(renderpass.get(), imageIndex, vk::Rect2D({ 0, 0 }, { windowWidth, windowHeight }), clearValues);

            command->setPipeline(graphicsPipeline);
            command->setBindGroup(0, sceneBindGroup.get(), { now * static_cast<uint32_t>(sceneBuffer->getBlockSize()) });

            for (uint32_t i = 0; auto& mesh : meshInstances)
            {
                command->setBindGroup(1, materialBindGroups[i].get());
                command->bindVertexBuffer(mesh.vertexBuffer.get());
                command->bindIndexBuffer(mesh.indexBuffer.get());

                command->drawIndexed(mesh.hostMesh.indices.size(), 1, 0, 0, 1);

                ++i;
            }

            command->drawImGui();
            command->endRenderPass();

            // end writing commands
            command->end();

            // execute
            command->execute(fences[now], imageAvailableSems[now], renderCompletedSems[now]);
            // present swapchain(window) image
            resized = window->present(imageIndex, renderCompletedSems[now].get());
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
}
