
#include <vk2s/Device.hpp>
#include <vk2s/Scene.hpp>
#include <vk2s/Camera.hpp>

#include <stb_image.h>
#include <glm/glm.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

struct InstanceMappingUB  // std140
{
    uint64_t VBAddress;
    uint64_t IBAddress;
    uint32_t materialIndex;
    uint32_t padding[3];
};

struct EmitterUB
{
    glm::vec3 sceneCenter;
    float sceneRadius;
    uint32_t triEmitterNum;
    uint32_t pointEmitterNum;
    uint32_t infiniteEmitterExists;  // false if 0, otherwise true
    uint32_t activeEmitterTypeNum;
};

struct MeshInstance
{
    vk2s::Mesh hostMesh;
    Handle<vk2s::Buffer> vertexBuffer;
    Handle<vk2s::Buffer> indexBuffer;

    Handle<vk2s::AccelerationStructure> blas;
};

inline void load(std::string_view path, vk2s::Device& device, std::vector<MeshInstance>& meshInstances, Handle<vk2s::Buffer>& materialUB, std::vector<Handle<vk2s::Image>>& materialTextures, Handle<vk2s::Buffer>& emitterUB,
                 Handle<vk2s::Buffer>& triEmitterUB, Handle<vk2s::Buffer>& infiniteEmitterUB)
{
    //std::vector<vk2s::Mesh> hostMeshes;
    //std::vector<vk2s::Material> hostMaterials;
    //loader.load(path.data(), hostMeshes, hostMaterials);

    vk2s::Scene scene(path);

    const std::vector<vk2s::Mesh>& hostMeshes       = scene.getMeshes();
    const std::vector<vk2s::Material>& materialData = scene.getMaterials();

    //hostMeshes.erase(hostMeshes.begin());
    //hostMeshes.erase(hostMeshes.begin());
    //hostMaterials.erase(hostMaterials.begin());
    //hostMaterials.erase(hostMaterials.begin());

    meshInstances.resize(hostMeshes.size());
    for (size_t i = 0; i < meshInstances.size(); ++i)
    {
        auto& mesh           = meshInstances[i];
        mesh.hostMesh        = hostMeshes[i];
        const auto& hostMesh = meshInstances[i].hostMesh;

        {  // vertex buffer
            const auto vbSize  = hostMesh.vertices.size() * sizeof(vk2s::Vertex);
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

    // materials
    {
        const auto ubSize = sizeof(vk2s::Material) * materialData.size();
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        materialUB = device.create<vk2s::Buffer>(ci, fb);
        materialUB->write(materialData.data(), ubSize);
    }

    std::vector<vk2s::Texture> textures = scene.getTextures();

    for (const auto& hostTex : textures)
    {
        auto& tex       = materialTextures.emplace_back();
        const auto size = hostTex.width * hostTex.height * static_cast<uint32_t>(STBI_rgb_alpha);

        vk::ImageCreateInfo ci;
        ci.arrayLayers   = 1;
        ci.extent        = vk::Extent3D(hostTex.width, hostTex.height, 1);
        ci.format        = vk::Format::eR8G8B8A8Srgb;
        ci.imageType     = vk::ImageType::e2D;
        ci.mipLevels     = 1;
        ci.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
        ci.initialLayout = vk::ImageLayout::eUndefined;

        tex = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

        tex->write(hostTex.pData, size);
    }

    // if textures are empty, add dummy texture
    if (materialTextures.empty())
    {
        auto& dummyTex    = materialTextures.emplace_back();
        const auto width  = 1;
        const auto height = 1;
        const auto size   = width * height * static_cast<uint32_t>(STBI_rgb_alpha);

        vk::ImageCreateInfo ci;
        ci.arrayLayers   = 1;
        ci.extent        = vk::Extent3D(width, height, 1);
        ci.format        = vk::Format::eR8G8B8A8Srgb;
        ci.imageType     = vk::ImageType::e2D;
        ci.mipLevels     = 1;
        ci.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
        ci.initialLayout = vk::ImageLayout::eUndefined;

        dummyTex = device.create<vk2s::Image>(ci, vk::MemoryPropertyFlagBits::eDeviceLocal, size, vk::ImageAspectFlagBits::eColor);

        uint8_t dummyData[4] = { 255, 0, 255, 255 };
        dummyTex->write(dummyData, size);
    }

    // emitter
    std::vector<vk2s::TriEmitter> triEmitters = scene.getTriEmitters();

    EmitterUB emitter;
    emitter.triEmitterNum   = triEmitters.size();
    emitter.pointEmitterNum = 0;  // TODO:
    // testing
    emitter.infiniteEmitterExists = 1;
    vk2s::InfiniteEmitter infEmitter{
        .constantEmissive = glm::vec4(0.3, 0.3, 0.3, 1.0),
        .envmapIdx        = 0xFFFFFFFF,
        .pdfIdx           = 0xFFFFFFFF,
        .padding          = { 0 },
    };

    emitter.sceneCenter = glm::vec3(0., 0., 0.);
    emitter.sceneRadius = 100000.f;

    emitter.activeEmitterTypeNum = 2;  // TODO for pointEmitter

    {
        const auto ubSize = sizeof(EmitterUB);
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        emitterUB = device.create<vk2s::Buffer>(ci, fb);
        emitterUB->write(&emitter, ubSize);
    }

    {
        const auto ubSize = sizeof(vk2s::TriEmitter) * std::max((size_t)1, triEmitters.size());
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        triEmitterUB = device.create<vk2s::Buffer>(ci, fb);
        if (!triEmitters.empty())
        {
            triEmitterUB->write(triEmitters.data(), ubSize);
        }
    }

    {
        const auto ubSize = sizeof(vk2s::InfiniteEmitter);
        vk::BufferCreateInfo ci({}, ubSize, vk::BufferUsageFlagBits::eStorageBuffer);
        vk::MemoryPropertyFlags fb = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        infiniteEmitterUB = device.create<vk2s::Buffer>(ci, fb);
        infiniteEmitterUB->write(&infEmitter, ubSize);
    }
}

inline vk::TransformMatrixKHR convert(const glm::mat4x3& m)
{
    vk::TransformMatrixKHR mtx;
    auto mT = glm::transpose(m);
    memcpy(&mtx.matrix[0], &mT[0], sizeof(float) * 4);
    memcpy(&mtx.matrix[1], &mT[1], sizeof(float) * 4);
    memcpy(&mtx.matrix[2], &mT[2], sizeof(float) * 4);

    return mtx;
};
