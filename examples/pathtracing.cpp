#include <iostream>

#include "utility.hpp"

void pathtracing(const uint32_t windowWidth, const uint32_t windowHeight, const uint32_t frameCount)
{
    constexpr int kMaxSpp = 4096;

    try
    {
        vk2s::Device device;

        auto window = device.create<vk2s::Window>(windowWidth, windowHeight, frameCount, "pathtracing window");

        auto renderpass = device.create<vk2s::RenderPass>(window.get());

        device.initImGui(frameCount, window.get(), renderpass.get());

        std::vector<MeshInstance> meshInstances;
        Handle<vk2s::Buffer> materialBuffer;
        Handle<vk2s::Buffer> instanceMapBuffer;
        std::vector<Handle<vk2s::Image>> materialTextures;
        auto sampler = device.create<vk2s::Sampler>(vk::SamplerCreateInfo());
        vk2s::AssetLoader loader;

        load("../../examples/resources/model/CornellBox/CornellBox-Sphere.obj", device, loader, meshInstances, materialBuffer, instanceMapBuffer, materialTextures);

        // create scene UB
        Handle<vk2s::DynamicBuffer> sceneBuffer;
        {
            const auto size = sizeof(SceneUB) * frameCount;
            sceneBuffer     = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, size, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameCount);
        }

        //create result image
        Handle<vk2s::Image> resultImage;
        {
            const auto format   = window->getVkSwapchainImageFormat();
            const uint32_t size = windowWidth * windowHeight * vk2s::Compiler::getSizeOfFormat(format);

            vk::ImageCreateInfo ci;
            ci.arrayLayers   = 1;
            ci.extent        = vk::Extent3D(windowWidth, windowHeight, 1);
            ci.format        = format;
            ci.imageType     = vk::ImageType::e2D;
            ci.mipLevels     = 1;
            ci.usage         = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
            ci.initialLayout = vk::ImageLayout::eUndefined;

            resultImage = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            UniqueHandle<vk2s::Command> cmd = device.create<vk2s::Command>();
            cmd->begin(true);
            cmd->transitionImageLayout(resultImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->end();
            cmd->execute();
        }

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

        // create BLAS
        for (auto& mesh : meshInstances)
        {
            mesh.blas = device.create<vk2s::AccelerationStructure>(mesh.hostMesh.vertices.size(), sizeof(vk2s::AssetLoader::Vertex), mesh.vertexBuffer.get(), mesh.hostMesh.indices.size() / 3, mesh.indexBuffer.get());
        }

        // deploy instances
        vk::AccelerationStructureInstanceKHR templateDesc{};
        templateDesc.instanceCustomIndex = 0;
        templateDesc.mask                = 0xFF;
        templateDesc.flags               = 0;

        std::vector<vk::AccelerationStructureInstanceKHR> asInstances;
        asInstances.reserve(meshInstances.size());
        for (size_t i = 0; i < meshInstances.size(); ++i)
        {
            const auto& mesh = meshInstances[i];

            const auto& blas                                  = mesh.blas;
            const auto transform                              = glm::mat4(1.f);
            vk::TransformMatrixKHR mtxTransform               = convert(transform);
            vk::AccelerationStructureInstanceKHR asInstance   = templateDesc;
            asInstance.transform                              = mtxTransform;
            asInstance.accelerationStructureReference         = blas->getVkDeviceAddress();
            asInstance.instanceShaderBindingTableRecordOffset = 0;
            asInstances.emplace_back(asInstance);
        }

        // create TLAS
        auto tlas = device.create<vk2s::AccelerationStructure>(asInstances);

        // load shaders
        const auto raygenShader = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/raygen.rgen", "main");
        const auto missShader   = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/miss.rmiss", "main");
        const auto chitShader   = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/closesthit.rchit", "main");

        // create bind layout
        std::array bindings = {
            // 0 : TLAS
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eAll),
            // 1 : result image
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eAll),
            // 2 : scene parameters
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eAll),
            // 3 : instance mappings
            vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAll),
            // 4 : material parameters
            vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAll),
            // 5 : material textures
            vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eCombinedImageSampler, std::max(1ull, materialTextures.size()), vk::ShaderStageFlagBits::eAll),
            // 6 : envmap texture
            vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAll),
        };

        auto bindLayout = device.create<vk2s::BindLayout>(bindings);

        // shader groups
        constexpr int kIndexRaygen     = 0;
        constexpr int kIndexMiss       = 1;
        constexpr int kIndexClosestHit = 2;

        // create ray tracing pipeline
        vk2s::Pipeline::VkRayTracingPipelineInfo rpi{
            .raygenShader = raygenShader,
            .missShader   = missShader,
            .chitShader   = chitShader,
            .bindLayout   = bindLayout,
            .shaderGroups = { vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, kIndexRaygen, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                              vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, kIndexMiss, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                              vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, kIndexClosestHit, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR) },
        };

        auto raytracePipeline = device.create<vk2s::Pipeline>(rpi);

        // create shader binding table
        auto shaderBindingTable = device.create<vk2s::ShaderBindingTable>(raytracePipeline.get(), 1, 1, 1, 0, rpi.shaderGroups);

        // create bindgroup
        auto bindGroup = device.create<vk2s::BindGroup>(bindLayout.get());
        bindGroup->bind(0, 0, tlas.get());
        bindGroup->bind(0, 1, vk::DescriptorType::eStorageImage, resultImage);
        bindGroup->bind(0, 2, vk::DescriptorType::eUniformBufferDynamic, sceneBuffer.get());
        bindGroup->bind(0, 3, vk::DescriptorType::eStorageBuffer, instanceMapBuffer.get());
        bindGroup->bind(0, 4, vk::DescriptorType::eStorageBuffer, materialBuffer.get());
        if (materialTextures.empty())
        {
            bindGroup->bind(0, 5, vk::DescriptorType::eCombinedImageSampler, envmap, sampler);  // dummy
        }
        else
        {
            bindGroup->bind(0, 5, vk::DescriptorType::eCombinedImageSampler, materialTextures, sampler);
        }
        bindGroup->bind(0, 6, vk::DescriptorType::eCombinedImageSampler, envmap, envmapSampler);

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

        const auto clearValue = vk::ClearValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
        double lastTime       = 0;
        vk2s::Camera camera(60., 1. * windowWidth / windowHeight);
        camera.setPos(glm::vec3(0.0, 0.8, 3.0));
        camera.setLookAt(glm::vec3(0.0, 0.8, -2.0));
        bool timeSeed = true;
        int inputSpp  = 1;

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
            ImGui::InputInt("spp", &inputSpp, inputSpp, 10);
            if (ImGui::Button(timeSeed ? "exclude time from the seed" : "include time in the seed"))
            {
                timeSeed = !timeSeed;
            }
            if (timeSeed)
            {
                ImGui::Text("now : seed with time");
            }
            else
            {
                ImGui::Text("now : seed without time");
            }

            ImGui::End();

            ImGui::Render();

            // wait and reset fence
            fences[now]->wait();
            fences[now]->reset();

            {  // write data
                //clamp spp
                inputSpp = std::min(kMaxSpp, std::max(1, inputSpp));

                SceneUB sceneUBO{
                    .view        = camera.getViewMatrix(),
                    .proj        = camera.getProjectionMatrix(),
                    .viewInv     = glm::inverse(sceneUBO.view),
                    .projInv     = glm::inverse(sceneUBO.proj),
                    .elapsedTime = static_cast<float>(currentTime),
                    .spp         = static_cast<uint32_t>(inputSpp),
                    .seedMode    = static_cast<uint32_t>(timeSeed),
                    .padding     = 0.f,
                };

                sceneBuffer->write(&sceneUBO, sizeof(SceneUB), now * sceneBuffer->getBlockSize());
            }

            // acquire next image from swapchain(window)
            const uint32_t imageIndex = window->acquireNextImage(imageAvailableSems[now].get());

            auto& command = commands[now];
            // start writing command
            command->begin();

            {  // trace ray
                command->setPipeline(raytracePipeline);
                command->setBindGroup(bindGroup.get(), { static_cast<uint32_t>(now * sceneBuffer->getBlockSize()) });
                command->traceRays(shaderBindingTable.get(), windowWidth, windowHeight, 1);
            }

            {  // present
                const auto region = vk::ImageCopy()
                                        .setExtent({ windowWidth, windowHeight, 1 })
                                        .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setSrcOffset({ 0, 0, 0 })
                                        .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setDstOffset({ 0, 0, 0 });

                // copy path tracing result image to swapchain(window)
                command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
                command->copyImageToSwapchain(resultImage.get(), window.get(), region, imageIndex);
                command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);

                // render GUI (ImGui)
                command->beginRenderPass(renderpass.get(), imageIndex, vk::Rect2D({ 0, 0 }, { windowWidth, windowHeight }), clearValue);
                command->drawImGui();
                command->endRenderPass();
            }
            // end writing commands
            command->end();

            // execute
            command->execute(fences[now], imageAvailableSems[now], renderCompletedSems[now]);
            // present swapchain(window) image
            window->present(imageIndex, renderCompletedSems[now].get());
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return;
}
