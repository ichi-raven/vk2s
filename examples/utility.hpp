
#include <vk2s/Device.hpp>
#include <vk2s/AssetLoader.hpp>
#include <vk2s/Camera.hpp>

#include <stb_image.h>
#include <glm/glm.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
    vk2s::AssetLoader::Mesh hostMesh;
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

inline void load(std::string_view path, vk2s::Device& device, vk2s::AssetLoader& loader, std::vector<MeshInstance>& meshInstances, Handle<vk2s::Buffer>& materialUB, Handle<vk2s::Buffer>& instanceMapBuffer,
          std::vector<Handle<vk2s::Image>>& materialTextures)
{
    std::vector<vk2s::AssetLoader::Mesh> hostMeshes;
    std::vector<vk2s::AssetLoader::Material> hostMaterials;
    loader.load(path.data(), hostMeshes, hostMaterials);

    meshInstances.resize(hostMeshes.size());
    for (size_t i = 0; i < meshInstances.size(); ++i)
    {
        auto& mesh           = meshInstances[i];
        mesh.hostMesh        = std::move(hostMeshes[i]);
        const auto& hostMesh = meshInstances[i].hostMesh;

        {  // vertex buffer
            const auto vbSize  = hostMesh.vertices.size() * sizeof(vk2s::AssetLoader::Vertex);
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
            const auto& hostTexture = std::get<vk2s::AssetLoader::Texture>(hostMat.diffuse);
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

inline vk::TransformMatrixKHR convert(const glm::mat4x3& m)
{
    vk::TransformMatrixKHR mtx;
    auto mT = glm::transpose(m);
    memcpy(&mtx.matrix[0], &mT[0], sizeof(float) * 4);
    memcpy(&mtx.matrix[1], &mT[1], sizeof(float) * 4);
    memcpy(&mtx.matrix[2], &mT[2], sizeof(float) * 4);

    return mtx;
};
