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
    /**
     * @brief  correspondence data of each vertex to the bone
     */
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

    //! macro for convenience
#define EQ(MEMBER) (MEMBER == another.MEMBER)
    
    //! standard vertex type 
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

    //! standard mesh type
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string nodeName;
        std::string meshName;
        uint32_t materialIdx;
    };

    //! emitter type (can be loaded from scene)
    enum class EmitterType
    {
        eTri,
        ePoint,
        eInfinite
    };

    //! triangle emittertype, std140
    struct TriEmitter
    {
        glm::vec4 p[3];

        glm::vec3 normal;
        float area;

        glm::vec4 emissive;
    };

    //! point emitter type, std140
    struct PointEmitter
    {
        glm::vec4 p;
        glm::vec3 emissive;
        float padding;
    };

    //! infinite emitter type, std140
    struct InfiniteEmitter
    {
        glm::vec4 constantEmissive; // if constant
        uint32_t envmapIdx; // 0xFFFFFFFF if constant (else valid texture index)
        uint32_t pdfIdx;    // 0xFFFFFFFF if constant (else valid texture index)
        uint32_t padding[2];
    };

    //! type representing the texture on the host (CPU) side
    struct Texture
    {
        enum class Type
        {
            eAlbedo,
            eRoughness,
            eMetalness,
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

    //! standard material type, std140
    struct Material
    {
        enum Type : uint32_t
        {
            eDiffuse = 0,
            eConductor = 1,
            eDielectric = 2,
        };

        uint32_t type;
        int32_t roughnessTex;
        glm::vec2 roughness;

        glm::vec3 albedo;
        int32_t albedoTex;

        glm::vec3 eta; // Re(IOR)
        int32_t metalnessTex;

        glm::vec3 k; // Im(IOR)
        int32_t normalMapTex;

        glm::vec4 emissive; // cause problem with NEE
    };

    //! bone type, std140
    struct Bone
    {
        glm::mat4 offset;
        glm::mat4 transform;
    };

    /**
     * @brief  class that controls an instance consisting of a combination of bones
     */
    struct Skeleton
    {
        /**
         * @brief  constructor
         */
        Skeleton()
            : defaultAxis(glm::mat4(1.f))
            , pScene(nullptr)
            , mGlobalInverse(glm::mat4(1.f))
        {
        }

        /**
         * @brief  destructor
         */
        ~Skeleton()
        {
        }

        /**
         * @brief  set assimp scene (for loading bone animation)
         */
        void setAiScene(const aiScene* ps)
        {
            pScene = ps;
        }

        /**
         * @brief  Inverse matrix to convert to root bone hierarchy
         */
        void setGlobalInverse(const glm::mat4& inv)
        {
            mGlobalInverse = inv;
        }

        /**
         * @brief  update to the animation result at the specified second.
         */
        void update(float second, size_t animationIndex = 0);

        //! bones
        std::vector<Bone> bones;
        //! mapping node name to bone index
        std::map<std::string, size_t> boneMap;
        //! Represents a default axis transformation of the entire model (e.g., Y-UP -> Z-UP)
        glm::mat4 defaultAxis;

    private:

        /**
         * @brief  explore each node
         */
        void traverseNode(const float timeInAnim, const size_t animationIndex, const aiNode* node, const glm::mat4 parentTransform);
        
        //! assimp scene
        const aiScene* pScene;
        //! global inverse matrix
        glm::mat4 mGlobalInverse;
    };

    /**
     * @brief  class representing a scene file
     */
    class Scene
    {
    public:

        /**
         * @brief  constructor (loads a file at the specified path)
         */
        Scene(std::string_view path);

        /**
         * @brief  get the smallest sphere that include the scene
         */
        const std::pair<glm::vec3, float> getSceneBoundSphere() const;

        /**
         * @brief  get all meshes in the scene
         */
        const std::vector<Mesh>& getMeshes() const;

        /**
         * @brief  get all materials in the scene
         */
        const std::vector<Material>& getMaterials() const;

        /**
         * @brief  get all textures in the scene
         */
        const std::vector<Texture>& getTextures() const;

        /**
         * @brief  get all triangle emitters in the scene
         */
        const std::vector<TriEmitter>& getTriEmitters() const;

        /**
         * @brief  get infinite emitter in the scene
         */
        const InfiniteEmitter& getInfiniteEmitter() const;

    private:

        /**
         * @brief  per-node processing (recursive)
         */
        void processNode(const aiScene* pScene, const aiNode* node);

        /**
         * @brief  loading of the mesh attached to the node
         */
        bool processMesh(const aiScene* pScene, const aiNode* pNode, const aiMesh* pMesh);

        /**
         * @brief loading of materials corresponding to the mesh 
         */
        std::pair<uint32_t, bool> addMaterial(const aiScene* pScene, const aiMesh* pMesh);

        /**
         * @brief  adding a luminous mesh as an emitter
         */
        void addEmitter(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const uint32_t matIdx);

        /**
         * @brief  load the textures that come with the material
         */
        uint32_t loadMaterialTexture(const aiScene* pScene, const aiMaterial* mat, const aiTextureType aiType, const Texture::Type type);

    private:
        //! directory of read files
        std::string mDirectory;
        //! path of read files
        std::string mPath;

        //! min point in the 3D space this scene
        glm::vec3 mBottomRightFront;
        //! max point in the 3D space this scene
        glm::vec3 mTopLeftBack;

        //! smallest sphere containing a scene
        std::pair<glm::vec3, float> mSceneBoundSphere;

        //! meshes of the scene
        std::vector<Mesh> mMeshes;
        //! materials of the scene
        std::vector<Material> mMaterials;
        //! textures of the scene
        std::vector<Texture> mTextures;

        //! triangle emitters of the scene
        std::vector<TriEmitter> mTriEmitters;
        //! point emitters of the scene
        std::vector<PointEmitter> mPointEmitters;
        //! infinite emitter of the scene
        InfiniteEmitter mInfiniteEmitter;

        //! mapping path to index
        std::unordered_map<std::string, uint32_t> mTextureMap;

        //! Assimp importer
        Assimp::Importer mImporter;
    };
}  // namespace vk2s

#endif
