/*****************************************************************/ /**
 * @file   Scene.hpp
 * @brief  header file of scene class 
 * 
 * @author ichi-raven
 * @date   June 2024
 *********************************************************************/
#ifndef VK2S_SCENE_HPP_
#define VK2S_SCENE_HPP_

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <optional>
#include <variant>

#include <cstdint>

namespace vk2s
{

    struct VertexBoneData
    {
        VertexBoneData()
        {
            weights[0] = weights[1] = weights[2] = weights[3] = 0;
            id[0] = id[1] = id[2] = id[3] = 0;
        }

        float weights[4];
        float id[4];
    };

#define EQ(MEMBER) (MEMBER == another.MEMBER)
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 joint;
        glm::vec4 weight;

        bool operator==(const Vertex& another) const
        {
            return EQ(pos) && EQ(normal) && EQ(uv) && EQ(joint) && EQ(weight);
        }
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string nodeName;
        std::string meshName;
        uint32_t materialIdx;
    };

    enum class EmitterType
    {
        eTri,
        ePoint,
        eInfinite
    };

    // std430
    struct TriEmitter
    {
        glm::vec3 p[3];
        glm::vec3 normal;
        glm::vec3 emissive;
        float area;
    };

    // std430
    struct PointEmitter
    {
        glm::vec4 p;
        glm::vec3 emissive;
        float padding;
    };

    // std430
    struct InfiniteEmitter
    {
        uint32_t envmapIdx;
        uint32_t pdfIdx;
        uint32_t padding[2];
    };

    struct Texture
    {
        enum class Type
        {
            eAlbedo,
            eRoughness,
            eNormal,
            eEnvmap
        };

        std::byte* pData = nullptr;
        bool embedded    = false;
        int width        = 0;
        int height       = 0;
        int bpp          = 0;
        std::string path;
        Type type;
    };

    // std430
    struct Material
    {
        enum class Type : uint32_t
        {
            eDiffuse = 0,
            eConductor = 1,
            eDielectric = 2,
        };

        Type type;
        glm::vec4 albedo;
        glm::vec3 eta; // Re(IOR)
        glm::vec3 k; // Im(IOR)
        glm::vec2 roughness;
        glm::vec4 emissive; // cause problem with NEE
        uint32_t albedoTex;
        uint32_t rougnnessTex;
        uint32_t normalMapTex;
    };

    struct Bone
    {
        glm::mat4 offset;
        glm::mat4 transform;
    };

    struct Skeleton
    {
        Skeleton()
            : defaultAxis(glm::mat4(1.f))
            , pScene(nullptr)
            , mGlobalInverse(glm::mat4(1.f))
        {
        }

        ~Skeleton()
        {
        }

        void setAiScene(const aiScene* ps)
        {
            pScene = ps;
        }

        void setGlobalInverse(const glm::mat4& inv)
        {
            mGlobalInverse = inv;
        }

        void update(float second, size_t animationIndex = 0);

        std::vector<Bone> bones;
        std::map<std::string, size_t> boneMap;
        glm::mat4 defaultAxis;

    private:

        void traverseNode(const float timeInAnim, const size_t animationIndex, const aiNode* node, const glm::mat4 parentTransform);
        
        const aiScene* pScene;
        glm::mat4 mGlobalInverse;
    };

    class Scene
    {
    public:

        Scene(std::string_view path);

        const std::vector<Mesh>& getMeshes() const;

        const std::vector<Material>& getMaterials() const;

        const std::vector<Texture>& getTextures() const;

        const std::vector<TriEmitter>& getTriEmitters() const;

        const InfiniteEmitter& getInfiniteEmitter() const;

    private:

        void processNode(const aiScene* pScene, const aiNode* node);

        bool processMesh(const aiScene* pScene, const aiNode* pNode, const aiMesh* pMesh);

        std::pair<uint32_t, bool> addMaterial(const aiScene* pScene, const aiMesh* pMesh);

        void addEmitter(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const uint32_t matIdx);

        uint32_t loadMaterialTexture(const aiScene* pScene, const aiMaterial* mat, const aiTextureType aiType, const Texture::Type type);

    private:

        std::string mDirectory;
        std::string mPath;

        std::vector<Mesh> mMeshes;
        std::vector<Material> mMaterials;
        std::vector<Texture> mTextures;

        std::vector<TriEmitter> mTriEmitters;
        std::vector<PointEmitter> mPointEmitters;
        InfiniteEmitter mInfiniteEmitter;

        // path to index
        std::unordered_map<std::string, uint32_t> mTextureMap;

        Assimp::Importer mImporter;
    };
}  // namespace vk2s

#endif
