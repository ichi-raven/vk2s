/*****************************************************************/ /**
 * @file   Scene.cpp
 * @brief  source file of scene class
 * 
 * @author ichi-raven
 * @date   June 2024
 *********************************************************************/

#include "../include/vk2s/Scene.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <regex>

namespace
{
    inline glm::mat4 convert4x4(const aiMatrix4x4& from)
    {
        glm::mat4 to{};

        //transpose
        for (uint8_t i = 0; i < 4; ++i)
            for (uint8_t j = 0; j < 4; ++j) to[i][j] = from[j][i];

        return to;
    }

    inline glm::quat convertQuat(const aiQuaternion& from)
    {
        glm::quat to{};
        to.w = from.w;
        to.x = from.x;
        to.y = from.y;
        to.z = from.z;
        return to;
    }

    inline glm::vec3 convertVec3(const aiVector3D& from)
    {
        glm::vec3 to{};
        to.x = from.x;
        to.y = from.y;
        to.z = from.z;
        return to;
    }

    inline glm::vec4 convertVec4(const aiColor4D& from)
    {
        glm::vec4 to{};
        to.x = from.r;
        to.y = from.g;
        to.z = from.b;
        to.w = from.a;
        return to;
    }

    inline glm::vec2 convertVec2(const aiVector2D& from)
    {
        glm::vec2 to{};
        to.x = from.x;
        to.y = from.y;
        return to;
    }

    inline size_t findScale(float time, aiNodeAnim* pAnimationNode)
    {
        if (pAnimationNode->mNumScalingKeys == 1) return 0;

        for (size_t i = 0; i < pAnimationNode->mNumScalingKeys - 1; ++i)
            if (time < static_cast<float>(pAnimationNode->mScalingKeys[i + 1].mTime)) return i;

        assert(!"failed to find scale index!");
        return 0;
    }

    inline size_t findRotation(float time, aiNodeAnim* pAnimationNode)
    {
        if (pAnimationNode->mNumRotationKeys == 1) return 0;

        for (size_t i = 0; i < pAnimationNode->mNumRotationKeys - 1; ++i)
            if (time < static_cast<float>(pAnimationNode->mRotationKeys[i + 1].mTime)) return i;

        assert(!"failed to find rotation index!");
        return 0;
    }

    inline size_t findPosition(float time, aiNodeAnim* pAnimationNode)
    {
        if (pAnimationNode->mNumPositionKeys == 1) return 0;

        for (size_t i = 0; i < pAnimationNode->mNumPositionKeys - 1; ++i)
            if (time < static_cast<float>(pAnimationNode->mPositionKeys[i + 1].mTime)) return i;

        assert(!"failed to find position index!");
        return 0;
    }
}  // namespace

namespace vk2s
{
    void Skeleton::update(float second, size_t animationIndex)
    {
        if (animationIndex >= pScene->mNumAnimations)
        {
            assert(!"invalid animation index!");
            return;
        }

        float updateTime = pScene->mAnimations[animationIndex]->mTicksPerSecond;
        if (updateTime == 0)
        {
            assert("zero update time!");
            updateTime = 25.f;  //?
        }

        traverseNode(fmod(second * updateTime, pScene->mAnimations[animationIndex]->mDuration), animationIndex, pScene->mRootNode, glm::mat4(1.f));
    }

    void Skeleton::traverseNode(const float timeInAnim, const size_t animationIndex, const aiNode* node, const glm::mat4 parentTransform)
    {
        //std::cerr << "\n\n\nprev parentTransform\n" << glm::to_string(parentTransform) << "\n";

        std::string nodeName = std::string(node->mName.C_Str());

        const aiAnimation* pAnimation = pScene->mAnimations[animationIndex];

        glm::mat4 transform = convert4x4(node->mTransformation);

        // if(nodeName == "Z_UP")
        // {
        //     transform = glm::mat4(1.f);
        // }

        //std::cerr << "original transform\n" << glm::to_string(transform) << "\n";

        aiNodeAnim* pAnimationNode = nullptr;

        //find animation node
        for (size_t i = 0; i < pAnimation->mNumChannels; ++i)
        {
            if (std::string(pAnimation->mChannels[i]->mNodeName.data) == nodeName)
            {
                pAnimationNode = pAnimation->mChannels[i];
                break;
            }
        }

        if (pAnimationNode)
        {
            //check
            //std::cerr << pAnimationNode->mNumScalingKeys << "\n";
            //std::cerr << pAnimationNode->mNumRotationKeys << "\n";
            //std::cerr << pAnimationNode->mNumPositionKeys << "\n";
            assert(pAnimationNode->mNumScalingKeys >= 1);
            assert(pAnimationNode->mNumRotationKeys >= 1);
            assert(pAnimationNode->mNumPositionKeys >= 1);

            //find right time scaling key
            size_t scaleIndex    = findScale(timeInAnim, pAnimationNode);
            size_t rotationIndex = findRotation(timeInAnim, pAnimationNode);
            size_t positionIndex = findPosition(timeInAnim, pAnimationNode);

            //std::cerr << "keys : \n";
            // std::cerr << scaleIndex << "\n";
            // std::cerr << rotationIndex << "\n";
            // std::cerr << positionIndex << "\n\n";

            glm::mat4 scale, rotation, translate;

            //scale
            if (pAnimationNode->mNumScalingKeys == 1)
                scale = glm::scale(glm::mat4(1.f), convertVec3(pAnimationNode->mScalingKeys[scaleIndex].mValue));
            else
            {
                glm::vec3&& start = convertVec3(pAnimationNode->mScalingKeys[scaleIndex].mValue);
                glm::vec3&& end   = convertVec3(pAnimationNode->mScalingKeys[scaleIndex + 1].mValue);
                float dt          = pAnimationNode->mScalingKeys[scaleIndex + 1].mTime - pAnimationNode->mScalingKeys[scaleIndex].mTime;

                glm::vec3&& lerped = glm::mix(start, end, (timeInAnim - static_cast<float>(pAnimationNode->mScalingKeys[scaleIndex].mTime)) / dt);
                scale              = glm::scale(glm::mat4(1.f), lerped);
                //std::cerr << "scale\n" << glm::to_string(scale) << "\n";
            }

            //rotation
            {
                glm::quat&& start = convertQuat(pAnimationNode->mRotationKeys[rotationIndex].mValue);
                glm::quat&& end   = convertQuat(pAnimationNode->mRotationKeys[rotationIndex + 1].mValue);
                float dt          = pAnimationNode->mRotationKeys[rotationIndex + 1].mTime - pAnimationNode->mRotationKeys[rotationIndex].mTime;

                glm::quat&& slerped = glm::mix(start, end, (timeInAnim - static_cast<float>(pAnimationNode->mRotationKeys[rotationIndex].mTime)) / dt);
                rotation            = glm::toMat4(glm::normalize(slerped));
                //std::cerr << "rotation\n" << glm::to_string(rotation) << "\n";
            }

            //position(translate)
            {
                glm::vec3&& start = convertVec3(pAnimationNode->mPositionKeys[positionIndex].mValue);
                glm::vec3&& end   = convertVec3(pAnimationNode->mPositionKeys[positionIndex + 1].mValue);
                float dt          = pAnimationNode->mPositionKeys[positionIndex + 1].mTime - pAnimationNode->mPositionKeys[positionIndex].mTime;

                glm::vec3&& lerped = glm::mix(start, end, (timeInAnim - static_cast<float>(pAnimationNode->mPositionKeys[positionIndex].mTime)) / dt);
                translate          = glm::translate(glm::mat4(1.f), lerped);
                //std::cerr << "translate\n" << glm::to_string(translate) << "\n";
            }

            transform = translate * rotation * scale;
        }

        glm::mat4 globalTransform = parentTransform * transform;

        if (boneMap.count(nodeName) > 0)
        {
            size_t boneIndex = boneMap[nodeName];
            // std::cerr << "boneIndex " << boneIndex << "\n";
            // std::cerr << "transform\n" << glm::to_string(transform) << "\n";
            // std::cerr << "parentTransform\n" << glm::to_string(parentTransform) << "\n";
            // std::cerr << "globalTransform\n" << glm::to_string(globalTransform) << "\n";
            // std::cerr << "boneOffset\n" << glm::to_string(bones[boneIndex].offset) << "\n";
            bones[boneIndex].transform = defaultAxis * mGlobalInverse * globalTransform * bones[boneIndex].offset;
            //std::cerr << "boneTransform\n" << glm::to_string(bones[boneIndex].transform) << "\n";
        }

        for (size_t i = 0; i < node->mNumChildren; ++i)
        {
            traverseNode(timeInAnim, animationIndex, node->mChildren[i], globalTransform);
        }
    }

    Scene::Scene(std::string_view path)
        : mPath(path)
        , mDirectory(path.substr(0, path.find_last_of("/\\")))
        , mTopLeftBack(-INFINITY, -INFINITY, -INFINITY)
        , mBottomRightFront(INFINITY, INFINITY, INFINITY)

    {
        const aiScene* pScene = mImporter.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_FlipUVs);

        if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << mImporter.GetErrorString() << "\n";
            throw std::runtime_error("failed to load asset with assimp!");
            return;
        }

        // before traversaling mesh nodes, search envmap first
        for (unsigned int i = 0; i < pScene->mNumMaterials; i++)
        {
            aiMaterial* pMat = pScene->mMaterials[i];

            aiString texturePath;
            if (pMat->GetTexture(aiTextureType_AMBIENT, 0, &texturePath) == AI_SUCCESS)
            {
                loadMaterialTexture(pScene, pMat, aiTextureType::aiTextureType_AMBIENT, Texture::Type::eEnvmap);
                // TODO: load pdf map?
            }
        }

        processNode(pScene, pScene->mRootNode);

        mSceneBoundSphere.first  = 0.5f * glm::vec3(mTopLeftBack.x + mBottomRightFront.x, mTopLeftBack.y + mBottomRightFront.y, mTopLeftBack.z + mBottomRightFront.z);
        mSceneBoundSphere.second = 10000.f * (mSceneBoundSphere.first.x - mBottomRightFront.x);
    }

    const std::pair<glm::vec3, float> Scene::getSceneBoundSphere() const
    {
        return mSceneBoundSphere;
    }

    const std::vector<Mesh>& Scene::getMeshes() const
    {
        return mMeshes;
    }

    const std::vector<Material>& Scene::getMaterials() const
    {
        return mMaterials;
    }

    const std::vector<Texture>& Scene::getTextures() const
    {
        return mTextures;
    }

    const std::vector<TriEmitter>& Scene::getTriEmitters() const
    {
        return mTriEmitters;
    }

    const InfiniteEmitter& Scene::getInfiniteEmitter() const
    {
        return mInfiniteEmitter;
    }

    void Scene::processNode(const aiScene* pScene, const aiNode* pNode)
    {
        for (uint32_t i = 0; i < pNode->mNumMeshes; ++i)
        {
            aiMesh* pMesh = pScene->mMeshes[pNode->mMeshes[i]];
            if (pMesh)
            {
                if (!processMesh(pScene, pNode, pMesh))
                {
                    std::cerr << "failed to load mesh!\n";
                    continue;
                }
            }
        }

        for (uint32_t i = 0; i < pNode->mNumChildren; ++i)
        {
            processNode(pScene, pNode->mChildren[i]);
        }
    }

    bool Scene::processMesh(const aiScene* pScene, const aiNode* pNode, const aiMesh* pMesh)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // walk through each of the mesh's vertices
        for (uint32_t i = 0; i < pMesh->mNumVertices; ++i)
        {
            Vertex vertex;

            vertex.pos = convertVec3(pMesh->mVertices[i]);

            if (pMesh->mNormals)
            {
                vertex.normal = convertVec3(pMesh->mNormals[i]);
            }
            else
            {
                vertex.normal = glm::vec3(0);
            }

            if (pMesh->mTextureCoords[0])
            {
                vertex.uv.x = static_cast<float>(pMesh->mTextureCoords[0][i].x);
                vertex.uv.y = static_cast<float>(pMesh->mTextureCoords[0][i].y);
            }
            else
            {
                vertex.uv.x = 0;
                vertex.uv.y = 0;
            }

            mTopLeftBack.x = std::max(mTopLeftBack.x, vertex.pos.x);
            mTopLeftBack.y = std::max(mTopLeftBack.y, vertex.pos.y);
            mTopLeftBack.z = std::max(mTopLeftBack.z, vertex.pos.z);

            mBottomRightFront.x = std::min(mBottomRightFront.x, vertex.pos.x);
            mBottomRightFront.y = std::min(mBottomRightFront.y, vertex.pos.y);
            mBottomRightFront.z = std::min(mBottomRightFront.z, vertex.pos.z);

            vertices.push_back(vertex);
        }

        //std::vector<VertexBoneData> vbdata;
        //mBoneNum += mesh->mNumBones;
        //if (mSkeletal)
        //{
        //    loadBones(node, mesh, vbdata);
        //    // add bone infomation to vertices
        //    for (uint32_t i = 0; i < vertices.size(); ++i)
        //    {
        //        vertices[i].joint  = glm::make_vec4(vbdata[i].id);
        //        vertices[i].weight = glm::make_vec4(vbdata[i].weights);
        //        //std::cerr << to_string(vertices[i].weight0) << "\n";
        //        // float sum = vertices[i].weight.x + vertices[i].weight.y + vertices[i].weight.z + vertices[i].weight.w;
        //        // assert(sum <= 1.01f);
        //        // assert(vertices[i].joint.x < mBoneNum);
        //        // assert(vertices[i].joint.y < mBoneNum);
        //        // assert(vertices[i].joint.z < mBoneNum);
        //        // assert(vertices[i].joint.w < mBoneNum);
        //    }
        //}

        if (pMesh->mFaces)
        {
            for (uint32_t i = 0; i < pMesh->mNumFaces; ++i)
            {
                aiFace face = pMesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; ++j)
                {
                    indices.push_back(face.mIndices[j]);
                }
            }
        }

        // load and add material
        auto [matIdx, hasEmissive] = addMaterial(pScene, pMesh);

        // if the material has emissive component, treat as emitter
        if (hasEmissive)
        {
            addEmitter(vertices, indices, matIdx);
        }

        mMeshes.emplace_back(Mesh{ .vertices = std::move(vertices), .indices = std::move(indices), .nodeName = std::string(pNode->mName.C_Str()), .meshName = std::string(pMesh->mName.C_Str()), .materialIdx = matIdx });

        return true;
    }

    std::pair<uint32_t, bool> Scene::addMaterial(const aiScene* pScene, const aiMesh* pMesh)
    {
        Material& material = mMaterials.emplace_back();
        bool hasEmissive   = false;

        material.type = Material::Type::eDiffuse;

        if (pMesh->mMaterialIndex >= 0 && pMesh->mMaterialIndex < pScene->mNumMaterials)
        {
            aiMaterial* pMat = pScene->mMaterials[pMesh->mMaterialIndex];

            material.albedoTex    = loadMaterialTexture(pScene, pMat, aiTextureType::aiTextureType_DIFFUSE, Texture::Type::eAlbedo);
            material.rougnnessTex = loadMaterialTexture(pScene, pMat, aiTextureType::aiTextureType_DIFFUSE_ROUGHNESS, Texture::Type::eRoughness);
            material.normalMapTex = loadMaterialTexture(pScene, pMat, aiTextureType::aiTextureType_NORMALS, Texture::Type::eNormal);

            {
                aiColor4D color;
                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_DIFFUSE, &color))
                {
                    //std::cerr << "\tfound diffuse : " << showColor(color) << "\n";
                    material.albedo = convertVec4(color);
                }

                float IOR = 1.f;
                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFRACTI, &IOR) && IOR != 1.f)
                {
                    material.eta  = glm::vec3(IOR);
                    material.type = Material::Type::eDielectric;
                }
                else
                {
                    material.eta = glm::vec3(1.0);
                }

                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_SPECULAR, &color))
                {
                    //std::cerr << "\tfound specular : " << showColor(color) << "\n";
                    material.eta = glm::vec3(1.0);
                    material.k   = convertVec4(color) + glm::vec4(1.0);
                    if (color.r > 0.5f && color.g > 0.5f && color.b > 0.5f)
                    {
                        material.type = Material::Type::eConductor;
                    }
                }

                float metallicFactor = 0.f;
                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_METALLIC_FACTOR, &metallicFactor))
                {
                    if (metallicFactor > 0.5f)
                    {
                        material.type = Material::Type::eConductor;
                    }
                }

                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_EMISSIVE, &color))
                {
                    material.emissive = convertVec4(color);
                    hasEmissive       = !color.IsBlack();
                }

                float Ns = 0.f;
                int iNs  = 0;

                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_SHININESS, &Ns))
                {
                    material.roughness = glm::vec2((1000.f - Ns) / 1000.f);
                    material.type      = Material::Type::eConductor;
                }
                else if (AI_SUCCESS == aiGetMaterialInteger(pMat, AI_MATKEY_SHININESS, &iNs))
                {
                    material.roughness = glm::vec2((1000.f - 1.f * iNs) / 1000.f);
                    material.type      = Material::Type::eConductor;
                }

                float reflect = 0.f;
                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFLECTIVITY, &reflect))
                {
                    //std::cerr << "\treflectivity : " << reflect << "\n";
                }
            }
        }

        uint32_t matIdx = static_cast<uint32_t>(mMaterials.size() - 1);

        return { matIdx, hasEmissive };
    }

    void Scene::addEmitter(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const uint32_t matIdx)
    {
        assert(indices.size() % 3 == 0 || !"invalid indices size! (not divisible by 3)");

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            auto& triEmitter = mTriEmitters.emplace_back();

            glm::vec3 p0 = vertices[indices[i]].pos;
            glm::vec3 p1 = vertices[indices[i + 1]].pos;
            glm::vec3 p2 = vertices[indices[i + 2]].pos;

            triEmitter.p[0] = glm::vec4(p0, 1.0);
            triEmitter.p[1] = glm::vec4(p1, 1.0);
            triEmitter.p[2] = glm::vec4(p2, 1.0);

            triEmitter.normal = (vertices[indices[i]].normal + vertices[indices[i + 1]].normal + vertices[indices[i + 2]].normal) / 3.f;

            triEmitter.emissive = mMaterials[matIdx].emissive;
            triEmitter.area     = 0.5f * glm::length(glm::cross(p2 - p0, p1 - p0));
        }
    }

    Texture loadTexture(const char* path, const Texture::Type type)
    {
        Texture texture;
        if (!path)
        {
            assert(!"invalid texture path(nullptr)!");
            return texture;
        }

        texture.path = std::string(path);

        texture.type = type;

        texture.pData = reinterpret_cast<std::byte*>(stbi_load(path, &texture.width, &texture.height, &texture.bpp, STBI_rgb_alpha));

        if (!texture.pData)
        {
            std::cerr << "failed to load texture : " << path << "\n";
            throw std::runtime_error("failed to load texture file!");
        }

        return texture;
    }

    uint32_t Scene::loadMaterialTexture(const aiScene* pScene, const aiMaterial* mat, const aiTextureType aiType, const Texture::Type type)
    {
        aiString str;
        mat->GetTexture(aiType, 0, &str);

        if (str.length == 0)
        {
            return -1;
        }

        const auto& itr = mTextureMap.find(std::string(str.C_Str()));
        if (itr != mTextureMap.end())
        {
            return itr->second;
        }

        {  // If texture hasn't been loaded already, load it
            Texture texture;

            const aiTexture* embeddedTexture = pScene->GetEmbeddedTexture(str.C_Str());

            if (embeddedTexture != nullptr)
            {
                texture.pData    = reinterpret_cast<std::byte*>(embeddedTexture->pcData);
                texture.width    = embeddedTexture->mWidth;
                texture.height   = embeddedTexture->mHeight;
                texture.bpp      = 4;  // RGBA8888
                texture.embedded = true;
                texture.type     = type;
            }
            else
            {
                std::string filename = std::regex_replace(str.C_Str(), std::regex("\\\\"), "/");
                filename             = mDirectory + '/' + filename;

                texture = loadTexture(filename.c_str(), type);
            }
            texture.type = type;
            texture.path = str.C_Str();

            mTextures.emplace_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures
            mTextureMap.emplace(std::string(str.C_Str()), mTextures.size() - 1);

            return mTextures.size() - 1;
        }

        return -1;
    }

}  // namespace vk2s
