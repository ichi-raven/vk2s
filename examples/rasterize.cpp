//#include <iostream>
//
//#include <vk2s/Device.hpp>
//
//#include "utility.hpp"
//
//void rasterize(const uint32_t windowWidth, const uint32_t windowHeight, const uint32_t frameCount)
//{
//    vk2s::Device device;
//
//    auto window = device.create<vk2s::Window>(windowWidth, windowHeight, frameCount, "rasterize window");
//
//    auto renderpass = device.create<vk2s::RenderPass>(window.get());
//
//    device.initImGui(frameCount, window.get(), renderpass.get());
//
//    auto vertexShader = device.create<vk2s::Shader>("../shaders/");
//
//  /*  vk2s::Pipeline::VkGraphicsPipelineInfo{
//        .vs = 
//    };*/
//
//    // create commands and sync objects
//    std::vector<Handle<vk2s::Command>> commands(frameCount);
//    std::vector<Handle<vk2s::Semaphore>> imageAvailableSems(frameCount);
//    std::vector<Handle<vk2s::Semaphore>> renderCompletedSems(frameCount);
//
//    for (int i = 0; i < frameCount; ++i)
//    {
//        commands[i]            = device.create<vk2s::Command>();
//        imageAvailableSems[i]  = device.create<vk2s::Semaphore>();
//        renderCompletedSems[i] = device.create<vk2s::Semaphore>();
//    }
//
//    const auto clearValue = vk::ClearValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
//    double lastTime       = 0;
//    vk2s::Camera camera(60., 1. * windowWidth / windowHeight);
//    camera.setPos(glm::vec3(0.0, 0.8, 3.0));
//    camera.setLookAt(glm::vec3(0.0, 0.8, -2.0));
//    int inputSpp  = 1;
//
//    for (uint32_t now = 0; window->update() && !window->getKey(GLFW_KEY_ESCAPE); now = (now + 1) % frameCount)
//    {
//        // update time
//        const double currentTime = glfwGetTime();
//        float deltaTime          = static_cast<float>(currentTime - lastTime);
//        lastTime                 = currentTime;
//
//        // update camera
//        const double speed      = 2.0f * deltaTime;
//        const double mouseSpeed = 0.7f * deltaTime;
//        camera.update(window->getpGLFWWindow(), speed, mouseSpeed);
//
//        // ImGui
//        ImGui_ImplVulkan_NewFrame();
//        ImGui_ImplGlfw_NewFrame();
//        ImGui::NewFrame();
//        ImGui::Begin("configuration");
//        ImGui::Text("device = %s", device.getPhysicalDeviceName().data());
//        ImGui::Text("fps = %lf", 1. / deltaTime);
//        const auto& pos    = camera.getPos();
//        const auto& lookAt = camera.getLookAt();
//        ImGui::Text("pos = (%lf, %lf, %lf)", pos.x, pos.y, pos.z);
//        ImGui::Text("lookat = (%lf, %lf, %lf)", lookAt.x, lookAt.y, lookAt.z);
//
//        ImGui::End();
//
//        ImGui::Render();
//
//        {  // write data
//            //clamp spp
//
//            SceneUB sceneUBO{
//                .view        = camera.getViewMatrix(),
//                .proj        = camera.getProjectionMatrix(),
//                .viewInv     = glm::inverse(sceneUBO.view),
//                .projInv     = glm::inverse(sceneUBO.proj),
//                .elapsedTime = static_cast<float>(currentTime),
//                .spp         = static_cast<uint32_t>(inputSpp),
//                .seedMode    = static_cast<uint32_t>(timeSeed),
//                .padding     = 0.f,
//            };
//
//            sceneBuffer->write(&sceneUBO, sizeof(SceneUB), now * sceneBuffer->getBlockSize());
//        }
//
//        // acquire next image from swapchain(window)
//        const uint32_t imageIndex = window->acquireNextImage(imageAvailableSems[now].get());
//
//        auto& command = commands[now];
//        // start writing command
//        command->begin();
//
//        {  // trace ray
//            command->setPipeline(raytracePipeline);
//            command->setBindGroup(bindGroup.get(), { static_cast<uint32_t>(now * sceneBuffer->getBlockSize()) });
//            command->traceRays(shaderBindingTable.get(), windowWidth, windowHeight, 1);
//        }
//
//        {  // present
//            const auto region = vk::ImageCopy()
//                                    .setExtent({ windowWidth, windowHeight, 1 })
//                                    .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
//                                    .setSrcOffset({ 0, 0, 0 })
//                                    .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
//                                    .setDstOffset({ 0, 0, 0 });
//
//            // copy path tracing result image to swapchain(window)
//            command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
//            command->copyImageToSwapchain(resultImage.get(), window.get(), region, imageIndex);
//            command->transitionImageLayout(resultImage.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
//
//            // render GUI (ImGui)
//            command->beginRenderPass(renderpass.get(), imageIndex, vk::Rect2D({ 0, 0 }, { windowWidth, windowHeight }), clearValue);
//            command->drawImGui();
//            command->endRenderPass();
//        }
//        // end writing commands
//        command->end();
//
//        // execute
//        command->execute(imageAvailableSems[now], renderCompletedSems[now]);
//        // present swapchain(window) image
//        window->present(imageIndex, renderCompletedSems[now].get());
//    }
//}
