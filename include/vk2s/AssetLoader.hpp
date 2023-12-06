/*****************************************************************/ /**
 * @file   AssetLoader.hpp
 * @brief  asset loader class
 * 
 * @author ichi-raven
 * @date   October 2023
 *********************************************************************/
#ifndef VK2S_ASSETLOADER_HPP_
#define VK2S_ASSETLOADER_HPP_

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <memory>
#include <map>
#include <optional>
#include <variant>

#include <cstdint>

namespace vk2s
{

class AssetLoader
{
public:
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
    };

    struct Texture
    {
        std::byte* pData = nullptr;
        bool embedded    = false;
        int width        = 0;
        int height       = 0;
        int bpp          = 0;
        std::string path;
        std::string type;
    };

    struct Material
    {
        std::string name;
        std::variant<glm::vec4, Texture> diffuse;
        std::optional<glm::vec4> specular;
        std::optional<glm::vec4> emissive;
        std::optional<glm::vec4> transparent;
        std::optional<float> shininess;
        std::optional<float> IOR;
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

    AssetLoader();

    ~AssetLoader();

    void load(const char* path, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out);

    void loadSkeletal(const char* path, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out, Skeleton& skeleton_out);

    Texture loadTexture(const char* path, const char* type);

private:
    void unload();

    void processNode(const aiNode* node, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out);

    std::pair<Mesh, Material> processMesh(const aiNode* node, const aiMesh* mesh);

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);

    void loadBones(const aiNode* node, const aiMesh* mesh, std::vector<VertexBoneData>& vbdata_out);

    bool mLoaded;
    bool mSkeletal;
    std::string mDirectory;
    std::string mPath;

    uint32_t mBoneNum;
    Skeleton mSkeleton;

    std::vector<Texture> mTexturesLoaded;

    Assimp::Importer mImporter;
    std::vector<const aiScene*> mpScenes;
};
}
#endif
