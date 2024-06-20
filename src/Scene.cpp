/*****************************************************************//**
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
}

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
    {
        const aiScene* pScene = mImporter.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_FlipUVs);

        if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << mImporter.GetErrorString() << "\n";
            throw std::runtime_error("failed to load asset with assimp!");
            return;
        }

        processNode(pScene, pScene->mRootNode);

        
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

    void Scene::processNode(const aiScene* pScene, const aiNode* pNode)
    {
        for (uint32_t i = 0; i < pNode->mNumMeshes; i++)
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

        for (uint32_t i = 0; i < pNode->mNumChildren; i++)
        {
            processNode(pScene, pNode->mChildren[i]);
        }
    }

    bool Scene::processMesh(const aiScene* pScene, const aiNode* pNode, const aiMesh* pMesh)
    {
        // Data to fill
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Walk through each of the mesh's vertices
        for (uint32_t i = 0; i < pMesh->mNumVertices; i++)
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
                //assert(!"texture coord nothing!");
                vertex.uv.x = 0;
                vertex.uv.y = 0;
            }

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
            for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
            {
                aiFace face = pMesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++)
                {
                    indices.push_back(face.mIndices[j]);
                }
            }
        }

        Material& material = mMaterials.emplace_back();
        {
            std::vector<Texture> diffuseMaps;
            if (pMesh->mMaterialIndex >= 0 && pMesh->mMaterialIndex < pScene->mNumMaterials)
            {
                aiMaterial* pMat = pScene->mMaterials[pMesh->mMaterialIndex];
                material.name    = pMat->GetName().C_Str();

                diffuseMaps = loadMaterialTextures(pScene, pMat, aiTextureType_DIFFUSE, "texture_diffuse");

                if (!diffuseMaps.empty())
                {
                    material.diffuse = diffuseMaps.front();
                }

                //std::cerr << "material index " << pMesh->mMaterialIndex << " : " << pMat->GetName().C_Str() << "\n";
                //std::cerr << "material num " << pMat->mNumProperties << "\n";

                {
                    aiColor4D color;
                    if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_DIFFUSE, &color) && diffuseMaps.empty())
                    {
                        //std::cerr << "\tfound diffuse : " << showColor(color) << "\n";
                        material.diffuse = convertVec4(color);
                    }
                    else
                    {
                        //std::cerr << "\tdiffuse not found...\n";
                    }

                    if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_SPECULAR, &color))
                    {
                        //std::cerr << "\tfound specular : " << showColor(color) << "\n";
                        material.specular = convertVec4(color);
                    }
                    else
                    {
                        //std::cerr << "\tspecular not found...\n";
                    }

                    if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_EMISSIVE, &color))
                    {
                        //std::cerr << "\tfound emissive : " << showColor(color) << "\n";
                        material.emissive = convertVec4(color);
                    }
                    else
                    {
                        //std::cerr << "\temissive not found...\n";
                    }

                    if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_TRANSPARENT, &color))
                    {
                        //std::cerr << "\tfound transparent : " << showColor(color) << "\n";
                        material.transparent = convertVec4(color);
                    }
                    else
                    {
                        //std::cerr << "\transparent not found...\n";
                    }

                    float Ns = 0.f;
                    int iNs  = 0;

                    if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_SHININESS, &Ns))
                    {
                        //std::cerr << "\tfloat shininess factor : " << Ns << "\n";
                        material.shininess = Ns;
                    }
                    else if (AI_SUCCESS == aiGetMaterialInteger(pMat, AI_MATKEY_SHININESS, &iNs))
                    {
                        //std::cerr << "\tint shininess factor : " << iNs << "\n";
                        material.shininess = static_cast<float>(iNs);
                    }
                    else
                    {
                        //std::cerr << "\tshininess not found...\n";
                    }

                    float reflect = 0.f;
                    if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFLECTIVITY, &reflect))
                    {
                        //std::cerr << "\treflectivity : " << reflect << "\n";
                    }
                    else
                    {
                        //std::cerr << "\treflectivity not found...\n";
                    }

                    float IOR = 0.f;
                    if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFRACTI, &IOR))
                    {
                        //std::cerr << "\tIOR : " << IOR << "\n";
                        material.IOR = IOR;
                    }
                    else
                    {
                        //std::cerr << "\tIOR not found...\n";
                    }
                }
            }
        }

        mMeshes.emplace_back(Mesh{
            .vertices = vertices,
            .indices  = indices,
            .nodeName = std::string(pNode->mName.C_Str()),
            .meshName = std::string(pMesh->mName.C_Str()),
            .materialIdx = static_cast<uint32_t>(mMaterials.size() - 1)
        });
        
        return true;
    }

    Texture loadTexture(const char* path, const char* type)
    {
        Texture texture;
        if (!path)
        {
            assert(!"invalid texture path(nullptr)!");
            return texture;
        }

        texture.path = std::string(path);

        if (type)
        {
            texture.type = std::string(type);
        }
        else
        {
            texture.type = texture.path.substr(texture.path.find_last_of("/\\"), texture.path.size());
        }

        texture.pData = reinterpret_cast<std::byte*>(stbi_load(path, &texture.width, &texture.height, &texture.bpp, STBI_rgb_alpha));

        if (!texture.pData)
        {
            std::cerr << "failed to load texture : " << path << "\n";
            throw std::runtime_error("failed to load texture file!");
        }

        return texture;
    }

    std::vector<Texture> Scene::loadMaterialTextures(const aiScene* pScene, const aiMaterial* mat, const aiTextureType type, std::string_view typeName)
    {
        std::vector<Texture> textures;
        for (uint32_t i = 0; i < mat->GetTextureCount(type); ++i)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (uint32_t j = 0; j < mTextures.size(); ++j)
            {
                if (std::strcmp(mTextures[j].path.c_str(), str.C_Str()) == 0)
                {
                    textures.emplace_back(mTextures[j]);
                    skip = true;  // A texture with the same filepath has already been loaded, continue to next one (optimization)
                    break;
                }
            }
            if (!skip)
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
                }
                else
                {
                    std::string filename = std::regex_replace(str.C_Str(), std::regex("\\\\"), "/");
                    filename             = mDirectory + '/' + filename;

                    texture = loadTexture(filename.c_str(), typeName.data());
                }
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                mTextures.emplace_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures
            }
        }

        return textures;
    }

}

