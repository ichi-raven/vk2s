#include <iostream>

#include "utility.hpp"

struct SceneUB  // std430
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewInv;
    glm::mat4 projInv;
    uint32_t elapsedFrame;
    uint32_t spp;
    uint32_t prevSpp;
    float padding;
};

struct FilterUB  // std430
{
    float sigma;
    float h;
    uint32_t filterMode;
    int32_t kernelSize;
    int32_t windowSize;
    float padding[3];
};

void pathtracing(const uint32_t windowWidth, const uint32_t windowHeight, const uint32_t frameCount)
{
    constexpr int kMaxSpp    = 4096;
    constexpr int kMaxSumSpp = std::numeric_limits<int>::max() - 1;

    try
    {
        vk2s::Device device;

        auto window = device.create<vk2s::Window>(windowWidth, windowHeight, frameCount, "path tracer");

        auto renderpass = device.create<vk2s::RenderPass>(window.get(), vk::AttachmentLoadOp::eLoad);

        device.initImGui(frameCount, window.get(), renderpass.get());

        std::vector<MeshInstance> meshInstances;
        Handle<vk2s::Buffer> materialBuffer;
        Handle<vk2s::Buffer> emitterBuffer;
        Handle<vk2s::Buffer> triEmitterBuffer;
        std::vector<Handle<vk2s::Image>> materialTextures;
        auto sampler = device.create<vk2s::Sampler>(vk::SamplerCreateInfo());

        //load("../../examples/resources/model/CornellBox/CornellBox-Sphere.obj", device, meshInstances, materialBuffer, materialTextures, emitterBuffer, triEmitterBuffer);
        load("../../examples/resources/model/fireplace-room/fireplace_room.obj", device, meshInstances, materialBuffer, materialTextures, emitterBuffer, triEmitterBuffer);

        // create scene UB
        Handle<vk2s::DynamicBuffer> sceneBuffer;
        {
            const auto size = sizeof(SceneUB) * frameCount;
            sceneBuffer     = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, size, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameCount);
        }

        // create filter (compute) UB
        Handle<vk2s::DynamicBuffer> filterBuffer;
        {
            const auto ubSize = sizeof(FilterUB) * frameCount;
            filterBuffer      = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, ubSize, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameCount);
        }

        // create instance mapping UB
        Handle<vk2s::Buffer> instanceMapBuffer;
        {
            // instance mapping
            std::vector<InstanceMappingUB> meshMappings;
            meshMappings.reserve(meshInstances.size());

            for (int i = 0; i < meshInstances.size(); ++i)
            {
                const auto& mesh      = meshInstances[i];
                auto& mapping         = meshMappings.emplace_back();
                mapping.VBAddress     = mesh.vertexBuffer->getVkDeviceAddress();
                mapping.IBAddress     = mesh.indexBuffer->getVkDeviceAddress();
                mapping.materialIndex = i;  // WARNING: simple
            }

            const auto ubSize = sizeof(InstanceMappingUB) * meshInstances.size();
            vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
            vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

            instanceMapBuffer = device.create<vk2s::Buffer>(ci, fb);
            instanceMapBuffer->write(meshMappings.data(), ubSize);
        }

        //create result image
        Handle<vk2s::Image> resultImage;
        Handle<vk2s::Image> poolImage;
        Handle<vk2s::Image> computeResultImage;
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

            resultImage        = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);
            computeResultImage = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            // change format to pooling
            ci.format = vk::Format::eR32G32B32A32Sfloat;
            poolImage = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            UniqueHandle<vk2s::Command> cmd = device.create<vk2s::Command>();
            cmd->begin(true);
            cmd->transitionImageLayout(resultImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->transitionImageLayout(poolImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->transitionImageLayout(computeResultImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->end();
            cmd->execute();
        }

        // create BLAS
        for (auto& mesh : meshInstances)
        {
            mesh.blas = device.create<vk2s::AccelerationStructure>(mesh.hostMesh.vertices.size(), sizeof(vk2s::Vertex), mesh.vertexBuffer.get(), mesh.hostMesh.indices.size() / 3, mesh.indexBuffer.get());
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
        const auto raygenShader  = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/raygen.rgen", "main");
        const auto missShader    = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/miss.rmiss", "main");
        const auto shadowShader  = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/shadow.rmiss", "main");
        const auto chitShader    = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/closesthit.rchit", "main");
        const auto computeShader = device.create<vk2s::Shader>("../../examples/shaders/pathtracing/compute.comp", "main");

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
            // 6 : emitters information buffer
            vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAll),
            // 7 : triangle emitters buffer
            vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAll),
            // 8: sampling pool image
            vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eAll),
        };

        auto bindLayout = device.create<vk2s::BindLayout>(bindings);

        std::array compBindings = {
            // 0 : input image
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
            // 1 : film dynamic uniform buffer
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eCompute),
            // 2 : result image
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
        };

        auto computeBindLayout = device.create<vk2s::BindLayout>(compBindings);

        // shader groups
        constexpr int kIndexRaygen     = 0;
        constexpr int kIndexMiss       = 1;
        constexpr int kIndexShadow     = 2;
        constexpr int kIndexClosestHit = 3;

        // create ray tracing pipeline
        vk2s::Pipeline::RayTracingPipelineInfo rpi{
            .raygenShaders = { raygenShader },
            .missShaders   = { missShader, shadowShader },
            .chitShaders   = { chitShader },
            .bindLayouts   = bindLayout,
            .shaderGroups  = { vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, kIndexRaygen, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                               vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, kIndexMiss, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                               vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, kIndexShadow, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                               vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, kIndexClosestHit, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR) },
        };

        auto raytracePipeline = device.create<vk2s::Pipeline>(rpi);

        // create shader binding table
        auto shaderBindingTable = device.create<vk2s::ShaderBindingTable>(raytracePipeline.get(), 1, 2, 1, 0, rpi.shaderGroups);

        vk2s::Pipeline::ComputePipelineInfo cpi{
            .cs          = computeShader,
            .bindLayouts = computeBindLayout,
        };

        auto computePipeline = device.create<vk2s::Pipeline>(cpi);

        // create bindgroup
        auto bindGroup = device.create<vk2s::BindGroup>(bindLayout.get());
        bindGroup->bind(0, tlas.get());
        bindGroup->bind(1, vk::DescriptorType::eStorageImage, resultImage);
        bindGroup->bind(2, vk::DescriptorType::eUniformBufferDynamic, sceneBuffer.get());
        bindGroup->bind(3, vk::DescriptorType::eStorageBuffer, instanceMapBuffer.get());
        bindGroup->bind(4, vk::DescriptorType::eStorageBuffer, materialBuffer.get());
        if (!materialTextures.empty())
        {
            bindGroup->bind(5, vk::DescriptorType::eCombinedImageSampler, materialTextures, sampler);
        }
        bindGroup->bind(6, vk::DescriptorType::eStorageBuffer, emitterBuffer.get());
        bindGroup->bind(7, vk::DescriptorType::eStorageBuffer, triEmitterBuffer.get());
        bindGroup->bind(8, vk::DescriptorType::eStorageImage, poolImage);

        auto computeBindGroup = device.create<vk2s::BindGroup>(computeBindLayout.get());
        computeBindGroup->bind(0, vk::DescriptorType::eStorageImage, resultImage);
        computeBindGroup->bind(1, vk::DescriptorType::eUniformBufferDynamic, filterBuffer.get());
        computeBindGroup->bind(2, vk::DescriptorType::eStorageImage, computeResultImage);

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

        const auto clearValue = vk::ClearValue(std::array{ 0.2f, 0.2f, 0.2f, 1.0f });
        double lastTime       = 0;
        vk2s::Camera camera(60., 1. * windowWidth / windowHeight);
        camera.setPos(glm::vec3(0.0, 0.8, 3.0));
        camera.setLookAt(glm::vec3(0.0, 0.8, -2.0));
        bool timeSeed      = true;
        int inputSpp       = 1;
        int accumulatedSpp = 0;
        bool addSample     = false;
        bool showGUI       = true;
        float inputSigma   = 0.2;
        int inputKernel    = 4;
        int inputWindow    = 2;
        bool applyFilter   = false;

        for (uint32_t now = 0, accumulatedFrame = 0; window->update() && !window->getKey(GLFW_KEY_ESCAPE); now = (++accumulatedFrame) % frameCount)
        {
            // update time
            const double currentTime = glfwGetTime();
            float deltaTime          = static_cast<float>(currentTime - lastTime);
            lastTime                 = currentTime;

            // update camera
            const double speed      = 2.0f * deltaTime;
            const double mouseSpeed = 0.7f * deltaTime;
            camera.update(window->getpGLFWWindow(), speed, mouseSpeed);

            if (!camera.moved())
            {
                if (window->getKey(GLFW_KEY_ENTER))
                {
                    accumulatedSpp += addSample && !window->getKey(GLFW_KEY_RIGHT_CONTROL) ? 0 : inputSpp;
                    addSample = true;
                }
                else
                {
                    addSample = false;
                }
            }
            else
            {
                accumulatedSpp = 0;
            }

            if (window->getKey(GLFW_KEY_SPACE))
            {
                if (window->getKey(GLFW_KEY_LEFT_CONTROL))
                {
                    showGUI = false;
                }
                else
                {
                    showGUI = true;
                }
            }

            if (window->getKey(GLFW_KEY_F))
            {
                window->setFullScreen();
            }

            if (window->getKey(GLFW_KEY_G))
            {
                window->setWindowed();
            }

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
            ImGui::SetNextItemOpen(true);
            if (ImGui::TreeNode("path tracing"))
            {
                ImGui::InputInt("spp per frame", &inputSpp, inputSpp, 10);
                ImGui::Text("total spp : %d", accumulatedSpp);
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

                ImGui::TreePop();
                ImGui::Spacing();
            }

            ImGui::SetNextItemOpen(true);
            if (ImGui::TreeNode("post process(compute)"))
            {
                ImGui::SliderFloat("sigma", &inputSigma, 0.001f, 3.0f);
                ImGui::InputInt("kernel size", &inputKernel);
                ImGui::InputInt("window size", &inputWindow);

                if (ImGui::Button(applyFilter ? "remove filter" : "apply filter"))
                {
                    applyFilter = !applyFilter;
                }
                if (applyFilter)
                {
                    ImGui::Text("now : NLM filter applied");
                }
                else
                {
                    ImGui::Text("now : no filter(raw)");
                }

                ImGui::TreePop();
                ImGui::Spacing();
            }
            ImGui::End();

            ImGui::Render();

            // wait and reset fence
            fences[now]->wait();

            {  // write data
                //clamp spp
                inputSpp = std::min(kMaxSpp, std::max(1, inputSpp));

                SceneUB sceneUBO{ .view         = camera.getViewMatrix(),
                                  .proj         = camera.getProjectionMatrix(),
                                  .viewInv      = glm::inverse(sceneUBO.view),
                                  .projInv      = glm::inverse(sceneUBO.proj),
                                  .elapsedFrame = timeSeed ? accumulatedFrame : 1,
                                  .spp          = static_cast<uint32_t>(inputSpp),
                                  .prevSpp      = static_cast<uint32_t>(accumulatedSpp),
                                  .padding      = 0.f };

                FilterUB filterUBO{
                    .sigma = inputSigma,
                    .h     = inputSigma,
                    // 0 : do nothing, 1 : filter only, 2 : event camera only, 3 : both
                    .filterMode = static_cast<uint32_t>(applyFilter),
                    .kernelSize = inputKernel,
                    .windowSize = inputWindow,
                    .padding    = { 0.f },
                };

                sceneBuffer->write(&sceneUBO, sizeof(SceneUB), now * sceneBuffer->getBlockSize());
                filterBuffer->write(&filterUBO, sizeof(FilterUB), now * filterBuffer->getBlockSize());
            }

            // acquire next image from swapchain(window)
            const auto [imageIndex, resized] = window->acquireNextImage(imageAvailableSems[now].get());

            if (resized)
            {
                window->resize();
                renderpass->recreateFrameBuffers(window.get());
                continue;
            }

            fences[now]->reset();

            auto& command = commands[now];
            // start writing command
            command->begin();

            {  // trace ray
                command->setPipeline(raytracePipeline);
                command->setBindGroup(0, bindGroup.get(), { static_cast<uint32_t>(now * sceneBuffer->getBlockSize()) });
                command->traceRays(shaderBindingTable.get(), windowWidth, windowHeight, 1);
            }

            if (applyFilter)
            {  // compute
                command->setPipeline(computePipeline);
                command->setBindGroup(0, computeBindGroup.get(), { static_cast<uint32_t>(now * filterBuffer->getBlockSize()) });
                command->dispatch(windowWidth / 16 + 1, windowHeight / 16 + 1, 1);
            }

            {  // present
                const auto region = vk::ImageCopy()
                                        .setExtent({ windowWidth, windowHeight, 1 })
                                        .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setSrcOffset({ 0, 0, 0 })
                                        .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setDstOffset({ 0, 0, 0 });

                // copy path tracing result image to swapchain(window)
                if (applyFilter)
                {
                    command->transitionImageLayout(computeResultImage.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
                    command->copyImageToSwapchain(computeResultImage.get(), window.get(), region, imageIndex);
                    command->transitionImageLayout(computeResultImage.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
                }
                else
                {
                    command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
                    command->copyImageToSwapchain(resultImage.get(), window.get(), region, imageIndex);
                    command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
                }

                // render GUI (ImGui)

                command->beginRenderPass(renderpass.get(), imageIndex, vk::Rect2D({ 0, 0 }, { windowWidth, windowHeight }), clearValue);
                if (showGUI)
                {
                    command->drawImGui();
                }
                command->endRenderPass();
            }
            // end writing commands
            command->end();

            // execute
            command->execute(fences[now], imageAvailableSems[now], renderCompletedSems[now]);
            // present swapchain(window) image
            if (window->present(imageIndex, renderCompletedSems[now].get()))
            {
                window->resize();
                renderpass->recreateFrameBuffers(window.get());
                continue;
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "CAUGHT EXCEPTION : " << e.what() << "\n";
    }

    return;
}
