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
        std::uint32_t materialIdx;
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

    class Scene
    {
    public:

        Scene(std::string_view path);

        const std::vector<Mesh>& getMeshes() const;

        const std::vector<Material>& getMaterials() const;

        const std::vector<Texture>& getTextures() const;

    private:

        void processNode(const aiScene* pScene, const aiNode* node);

        bool processMesh(const aiScene* pScene, const aiNode* pNode, const aiMesh* pMesh);

        std::vector<Texture> loadMaterialTextures(const aiScene* pScene, const aiMaterial* mat, const aiTextureType type, std::string_view typeName);

    private:

        std::string mDirectory;
        std::string mPath;

        std::vector<Mesh> mMeshes;
        std::vector<Material> mMaterials;
        std::vector<Texture> mTextures;

        std::unordered_map<std::string, uint32_t> mTextureMap;

        Assimp::Importer mImporter;
    };
}  // namespace vk2s

#endif
