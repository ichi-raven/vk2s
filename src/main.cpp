
#include "../include/Device.hpp"
#include  "../include/AssetLoader.hpp"
#include "../include/Camera.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <iostream>

struct SceneUB  // std430
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewInv;
    glm::mat4 projInv;
    float elapsedTime;
    uint32_t spp;
    uint32_t seedMode;
    float padding;
};

enum class MaterialType : int32_t
{
    eLambert    = 0,
    eConductor  = 1,
    eDielectric = 2,
    eEmitter    = 3,
    eMaterialNum
};

struct MaterialUB  // std430
{
    glm::vec4 albedo;
    int32_t texIndex;
    int32_t materialType;
    float alpha;
    float IOR;
};

struct InstanceMappingUB  // std430
{
    uint64_t VBAddress;
    uint64_t IBAddress;
    uint32_t materialIndex;
    uint32_t padding[3];
};

struct MeshInstance
{
    AssetLoader::Mesh hostMesh;
    Handle<vk2s::Buffer> vertexBuffer;
    Handle<vk2s::Buffer> indexBuffer;

    Handle<vk2s::AccelerationStructure> blas;
};

struct FilterUB  // std430
{
    float sigma;
    float h;
    uint32_t filterMode;
    int32_t kernelSize;
    int32_t windowSize;
    float threshold;
    float padding[2];
};

void load(std::string_view path, vk2s::Device& device, AssetLoader& loader, std::vector<MeshInstance>& meshInstances, Handle<vk2s::Buffer>& materialUB, Handle<vk2s::Buffer>& instanceMapBuffer,
          std::vector<Handle<vk2s::Image>>& materialTextures)
{
    std::vector<AssetLoader::Mesh> hostMeshes;
    std::vector<AssetLoader::Material> hostMaterials;
    loader.load(path.data(), hostMeshes, hostMaterials);

    meshInstances.resize(hostMeshes.size());
    for (size_t i = 0; i < meshInstances.size(); ++i)
    {
        auto& mesh           = meshInstances[i];
        mesh.hostMesh        = std::move(hostMeshes[i]);
        const auto& hostMesh = meshInstances[i].hostMesh;

        {  // vertex buffer
            const auto vbSize  = hostMesh.vertices.size() * sizeof(AssetLoader::Vertex);
            const auto vbUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer;
            vk::BufferCreateInfo ci({}, vbSize, vbUsage);
            vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

            mesh.vertexBuffer = device.create<vk2s::Buffer>(ci, fb);
            mesh.vertexBuffer->write(hostMesh.vertices.data(), vbSize);
        }

        {  // index buffer

            const auto ibSize  = hostMesh.indices.size() * sizeof(uint32_t);
            const auto ibUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer;

            vk::BufferCreateInfo ci({}, ibSize, ibUsage);
            vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

            mesh.indexBuffer = device.create<vk2s::Buffer>(ci, fb);
            mesh.indexBuffer->write(hostMesh.indices.data(), ibSize);
        }
    }

    // instance mapping
    std::vector<InstanceMappingUB> meshMappings;
    meshMappings.reserve(meshInstances.size());

    for (int i = 0; i < meshInstances.size(); ++i)
    {
        const auto& mesh      = meshInstances[i];
        auto& mapping         = meshMappings.emplace_back();
        mapping.VBAddress     = mesh.vertexBuffer->getVkDeviceAddress();
        mapping.IBAddress     = mesh.indexBuffer->getVkDeviceAddress();
        mapping.materialIndex = i;  //simple
    }

    {
        const auto ubSize = sizeof(InstanceMappingUB) * meshInstances.size();
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        instanceMapBuffer = device.create<vk2s::Buffer>(ci, fb);
        instanceMapBuffer->write(meshMappings.data(), ubSize);
    }

    // materials

    constexpr double threshold = 1.;
    std::vector<MaterialUB> materialData;
    materialData.reserve(hostMaterials.size());
    for (const auto& hostMat : hostMaterials)
    {
        auto& mat        = materialData.emplace_back();
        mat.materialType = static_cast<uint32_t>(MaterialType::eLambert);  // default
        if (std::holds_alternative<glm::vec4>(hostMat.diffuse))
        {
            mat.albedo   = std::get<glm::vec4>(hostMat.diffuse);
            mat.texIndex = -1;
        }
        else
        {
            mat.albedo   = glm::vec4(0.3f, 0.3f, 0.3f, 1.f);  // DEBUG COLOR
            mat.texIndex = materialTextures.size();

            auto& texture           = materialTextures.emplace_back();
            const auto& hostTexture = std::get<AssetLoader::Texture>(hostMat.diffuse);
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

            texture = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            texture->write(hostTexture.pData, size);
        }

        if (hostMat.specular && hostMat.shininess && glm::length(*hostMat.specular) > threshold)
        {
            mat.materialType = static_cast<uint32_t>(MaterialType::eConductor);
            mat.albedo       = *hostMat.specular;
            mat.alpha        = 1. - *hostMat.shininess / 1000.;
        }

        if (hostMat.IOR && *hostMat.IOR > 1.0)
        {
            mat.materialType = static_cast<uint32_t>(MaterialType::eDielectric);
            mat.albedo       = glm::vec4(1.0);
            mat.IOR          = *hostMat.IOR;
        }

        if (hostMat.emissive && glm::length(*hostMat.emissive) > threshold)
        {
            mat.materialType = static_cast<uint32_t>(MaterialType::eEmitter);
            mat.albedo       = *hostMat.emissive;
        }
    }

    {
        const auto ubSize = sizeof(MaterialUB) * materialData.size();
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        materialUB = device.create<vk2s::Buffer>(ci, fb);
        materialUB->write(materialData.data(), ubSize);
    }
}

vk::TransformMatrixKHR convert(const glm::mat4x3& m)
{
    vk::TransformMatrixKHR mtx;
    auto mT = glm::transpose(m);
    memcpy(&mtx.matrix[0], &mT[0], sizeof(float) * 4);
    memcpy(&mtx.matrix[1], &mT[1], sizeof(float) * 4);
    memcpy(&mtx.matrix[2], &mT[2], sizeof(float) * 4);

    return mtx;
};


int main()
{
    constexpr uint32_t width  = 1200;
    constexpr uint32_t height = 1000;

        try
    {
        vk2s::Device device;

        auto window = device.create<vk2s::Window>(1200, 1000, 3, "test window");

        const auto extent   = window->getVkSwapchainExtent();
        const auto frameNum = window->getVkImageViews().size();

        auto renderpass = device.create<vk2s::RenderPass>(window.get());

        device.initImGui(frameNum, window.get(), renderpass.get());

        std::vector<MeshInstance> meshInstances;
        Handle<vk2s::Buffer> materialBuffer;
        Handle<vk2s::Buffer> instanceMapBuffer;
        std::vector<Handle<vk2s::Image>> materialTextures;
        auto sampler = device.create<vk2s::Sampler>(vk::SamplerCreateInfo());
        AssetLoader loader;

        load("../../resources/model/CornellBox/CornellBox-Water.obj", device, loader, meshInstances, materialBuffer, instanceMapBuffer, materialTextures);
        //load("../../resources/model/viking_room.obj", device, loader, meshInstances, materialBuffer, instanceMapBuffer, materialTextures);

        // create scene UB
        Handle<vk2s::DynamicBuffer> sceneBuffer;
        {
            const auto ubSize = sizeof(SceneUB) * frameNum;
            sceneBuffer       = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, ubSize, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameNum);
        }

        // create film (compute) UB
        Handle<vk2s::DynamicBuffer> filterBuffer;
        {
            const auto ubSize = sizeof(FilterUB) * frameNum;
            filterBuffer      = device.create<vk2s::DynamicBuffer>(vk::BufferCreateInfo({}, ubSize, vk::BufferUsageFlagBits::eUniformBuffer), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, frameNum);
        }

        //create result image, event image and compute result image
        Handle<vk2s::Image> resultImage;
        Handle<vk2s::Image> computeResultImage;
        Handle<vk2s::Image> eventImage;
        {
            const auto format   = window->getVkSwapchainImageFormat();
            const uint32_t size = extent.width * extent.height * Compiler::getSizeOfFormat(format);

            vk::ImageCreateInfo ci;
            ci.arrayLayers   = 1;
            ci.extent        = vk::Extent3D(extent, 1);
            ci.format        = format;
            ci.imageType     = vk::ImageType::e2D;
            ci.mipLevels     = 1;
            ci.usage         = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
            ci.initialLayout = vk::ImageLayout::eUndefined;

            resultImage        = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);
            computeResultImage = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);
            // event format
            ci.format  = vk::Format::eR32Sfloat;
            eventImage = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

            UniqueHandle<vk2s::Command> cmd = device.create<vk2s::Command>();
            cmd->begin(true);
            cmd->transitionImageLayout(resultImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->transitionImageLayout(computeResultImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->transitionImageLayout(eventImage.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            cmd->end();
            cmd->execute();
        }

        // create envmap
        Handle<vk2s::Image> envmap;
        Handle<vk2s::Sampler> envmapSampler;
        {
            const auto& hostTexture = loader.loadTexture("../../resources/envmap1.png", "envmap");
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
            mesh.blas = device.create<vk2s::AccelerationStructure>(mesh.hostMesh.vertices.size(), sizeof(AssetLoader::Vertex), mesh.vertexBuffer.get(), mesh.hostMesh.indices.size() / 3, mesh.indexBuffer.get());
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
            asInstance.instanceShaderBindingTableRecordOffset = 0;  //static_cast<uint32_t>(AppHitShaderGroups::eHitShader);
            asInstances.emplace_back(asInstance);
        }

        // create TLAS
        auto tlas = device.create<vk2s::AccelerationStructure>(asInstances);

        // load shaders
        const auto raygenShader  = device.create<vk2s::Shader>("../../shaders/PathTracing/raygen.rgen", "main");
        const auto missShader    = device.create<vk2s::Shader>("../../shaders/PathTracing/miss.rmiss", "main");
        const auto chitShader    = device.create<vk2s::Shader>("../../shaders/PathTracing/closesthit.rchit", "main");
        const auto computeShader = device.create<vk2s::Shader>("../../shaders/PathTracing/compute.comp", "main");

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

        std::array compBindings = {
            // 0 : input image
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
            // 1 : event image
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
            // 2 : film dynamic uniform buffer
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eCompute),
            // 3 : result image
            vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
        };

        auto computeBindLayout = device.create<vk2s::BindLayout>(compBindings);

        // create shader groups
        constexpr int indexRaygen     = 0;
        constexpr int indexMiss       = 1;
        constexpr int indexClosestHit = 2;

        // create pipeline
        vk2s::Pipeline::VkRayTracingPipelineInfo rpi{
            .raygenShader = raygenShader,
            .missShader   = missShader,
            .chitShader   = chitShader,
            .bindLayout   = bindLayout,
            .shaderGroups = { vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, indexRaygen, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                              vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral, indexMiss, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR),
                              vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, indexClosestHit, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR) },
        };

        auto raytracePipeline = device.create<vk2s::Pipeline>(rpi);

        vk2s::Pipeline::ComputePipelineInfo cpi{
            .cs         = computeShader,
            .bindLayout = computeBindLayout,
        };

        auto computePipeline = device.create<vk2s::Pipeline>(cpi);

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
            bindGroup->bind(0, 5, vk::DescriptorType::eCombinedImageSampler, envmap, sampler);  // dummy
        else
            bindGroup->bind(0, 5, vk::DescriptorType::eCombinedImageSampler, materialTextures, sampler);
        bindGroup->bind(0, 6, vk::DescriptorType::eCombinedImageSampler, envmap, envmapSampler);

        auto computeBindGroup = device.create<vk2s::BindGroup>(computeBindLayout.get());
        computeBindGroup->bind(0, 0, vk::DescriptorType::eStorageImage, resultImage);
        computeBindGroup->bind(0, 1, vk::DescriptorType::eStorageImage, eventImage);
        computeBindGroup->bind(0, 2, vk::DescriptorType::eUniformBufferDynamic, filterBuffer.get());
        computeBindGroup->bind(0, 3, vk::DescriptorType::eStorageImage, computeResultImage);

        std::vector<Handle<vk2s::Command>> commands(frameNum);
        std::vector<Handle<vk2s::Semaphore>> imageAvailableSems(frameNum);
        std::vector<Handle<vk2s::Semaphore>> renderCompletedSems(frameNum);

        for (int i = 0; i < frameNum; ++i)
        {
            commands[i]            = device.create<vk2s::Command>();
            imageAvailableSems[i]  = device.create<vk2s::Semaphore>();
            renderCompletedSems[i] = device.create<vk2s::Semaphore>();
        }

        auto clearValue = vk::ClearValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });

        uint32_t now    = 0;
        int test        = 0;
        double lastTime = 0;
        Camera camera(60., 1. * extent.width / extent.height);
        int inputSpp         = 1;
        float inputSigma     = 0.2;
        int inputKernel      = 4;
        int inputWindow      = 2;
        float inputThreshold = 1.0;
        bool timeSeed        = false;
        bool isEventCamera   = false;
        bool applyFilter     = false;
        constexpr int maxSpp = 4096;

        while (window->update() && !window->getKey(GLFW_KEY_ESCAPE))
        {
            const double currentTime = glfwGetTime();
            float deltaTime          = static_cast<float>(currentTime - lastTime);
            lastTime                 = currentTime;

            const double speed      = 2.0f * deltaTime;
            const double mouseSpeed = 0.7f * deltaTime;
            camera.update(window->getpGLFWWindow(), speed, mouseSpeed);

            const uint32_t imageIndex = window->acquireNextImage(imageAvailableSems[now].get());

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

                ImGui::TreePop();
                ImGui::Spacing();
            }

            ImGui::SetNextItemOpen(true);
            if (ImGui::TreeNode("post process(compute)"))
            {
                ImGui::InputFloat("sigma", &inputSigma, 0.05f);
                ImGui::InputInt("kernel size", &inputKernel);
                ImGui::InputInt("window size", &inputWindow);
                ImGui::InputFloat("threshold", &inputThreshold, 0.05f);

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

                if (ImGui::Button(isEventCamera ? "change to RGB camera" : "change to event camera"))
                {
                    isEventCamera = !isEventCamera;
                }
                if (isEventCamera)
                {
                    ImGui::Text("now : event camera");
                }
                else
                {
                    ImGui::Text("now : RGB camera");
                }

                ImGui::TreePop();
                ImGui::Spacing();
            }
            ImGui::End();

            ImGui::Render();

            {  // write data

                inputSpp    = std::min(maxSpp, std::max(1, inputSpp));
                inputKernel = std::max(inputWindow, inputKernel);
                inputWindow = std::min(inputWindow, inputKernel);
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

                FilterUB filterUBO{
                    .sigma = inputSigma,
                    .h     = inputSigma,
                    // 0 : do nothing, 1 : filter only, 2 : event camera only, 3 : both
                    .filterMode = static_cast<uint32_t>(isEventCamera) << 1 | static_cast<uint32_t>(applyFilter),
                    .kernelSize = inputKernel,
                    .windowSize = inputWindow,
                    .threshold  = inputThreshold,
                    .padding    = { 0.f },
                };

                sceneBuffer->write(&sceneUBO, sizeof(SceneUB), now * sceneBuffer->getBlockSize());
                filterBuffer->write(&filterUBO, sizeof(FilterUB), now * filterBuffer->getBlockSize());
            }

            auto& command = commands[now];
            command->begin();

            {  // trace ray
                command->setPipeline(raytracePipeline);
                command->setBindGroup(bindGroup.get(), { static_cast<uint32_t>(now * sceneBuffer->getBlockSize()) });
                command->traceRays(shaderBindingTable.get(), extent.width, extent.height, 1);
            }

            if (applyFilter || isEventCamera)
            {  // compute
                command->setPipeline(computePipeline);
                command->setBindGroup(computeBindGroup.get(), { static_cast<uint32_t>(now * filterBuffer->getBlockSize()) });
                command->dispatch(extent.width / 16 + 1, extent.height / 16 + 1, 1);
            }

            {  // present
                const auto region = vk::ImageCopy()
                                        .setExtent({ extent.width, extent.height, 1 })
                                        .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setSrcOffset({ 0, 0, 0 })
                                        .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                                        .setDstOffset({ 0, 0, 0 });

                if (applyFilter || isEventCamera)
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

                command->beginRenderPass(renderpass.get(), imageIndex, vk::Rect2D({ 0, 0 }, extent), clearValue);
                command->drawImGui();
                command->endRenderPass();
            }
            command->end();

            command->execute(imageAvailableSems[now], renderCompletedSems[now]);

            window->present(imageIndex, renderCompletedSems[now].get());
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}