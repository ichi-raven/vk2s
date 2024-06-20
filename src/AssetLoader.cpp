/*****************************************************************/ /**
 * \file   AssetLoader.cpp
 * \brief  implementation of AssetLoader.cpp
 * 
 * \author ichi-raven
 * \date   October 2023
 *********************************************************************/
#include "../include/vk2s/AssetLoader.hpp"

#include <iostream>
#include <unordered_map>
#include <regex>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>


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

namespace vk2s
{
    void AssetLoader::Skeleton::update(float second, size_t animationIndex)
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

    void AssetLoader::Skeleton::traverseNode(const float timeInAnim, const size_t animationIndex, const aiNode* node, const glm::mat4 parentTransform)
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
            if (std::string(pAnimation->mChannels[i]->mNodeName.data) == nodeName)
            {
                pAnimationNode = pAnimation->mChannels[i];
                break;
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

    AssetLoader::AssetLoader()
        : mBoneNum(0)
    {
    }

    AssetLoader::~AssetLoader()
    {
        if (mLoaded) unload();
    }

    const std::string& AssetLoader::getPath() const
    {
        return mPath;
    }

    void AssetLoader::unload()
    {
        for (auto& e : mTexturesLoaded)
        {
            if (!e.embedded)
            {
                stbi_image_free(e.pData);
            }
        }

        mLoaded = false;
    }

    void AssetLoader::load(const char* modelPath, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out)
    {
        mLoaded   = true;
        mSkeletal = false;

        mPath = std::string(modelPath);

        auto& scene = mpScenes.emplace_back();

        scene = mImporter.ReadFile(modelPath, aiProcess_Triangulate | aiProcess_FlipUVs);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << mImporter.GetErrorString() << "\n";
            throw std::runtime_error("failed to load asset with assimp!");
            return;
        }

        mDirectory = mPath.substr(0, mPath.find_last_of("/\\"));

        processNode(scene->mRootNode, meshes_out, materials_out);

        //std::cerr << "mesh count : " << mMeshes.size() << "\n";

        mLoaded = false;
    }

    //Skeletal
    void AssetLoader::loadSkeletal(const char* modelPath, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out, Skeleton& skeleton_out)
    {
        mLoaded   = true;
        mSkeletal = true;

        mPath = std::string(modelPath);

        auto&& scene = mpScenes.emplace_back();

        scene = mImporter.ReadFile(modelPath, aiProcess_Triangulate | aiProcess_FlipUVs);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << mImporter.GetErrorString() << "\n";
            assert(0);
            return;
        }

        mDirectory = mPath.substr(0, mPath.find_last_of("/\\"));

        mSkeleton.setGlobalInverse(glm::inverse(convert4x4(scene->mRootNode->mTransformation)));
        mSkeleton.setAiScene(scene);

        processNode(scene->mRootNode, meshes_out, materials_out);

        std::cerr << "bone num : " << mBoneNum << "\n";

        skeleton_out = mSkeleton;

        mLoaded = false;
    }

    void AssetLoader::processNode(const aiNode* node, std::vector<Mesh>& meshes_out, std::vector<Material>& materials_out)
    {
        //warning:debug!!!!!!!!!!!!!!!!!!!!!!!!
        auto& scene = mpScenes.back();

        for (uint32_t i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* pMesh = scene->mMeshes[node->mMeshes[i]];
            if (pMesh)
            {
                const auto [mesh, mat] = processMesh(node, pMesh);
                meshes_out.emplace_back(mesh);
                materials_out.emplace_back(mat);
            }
        }

        for (uint32_t i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], meshes_out, materials_out);
        }
    }

    inline std::string showColor(const aiColor3D& color)
    {
        return "(" + std::to_string(color.r) + ", " + std::to_string(color.g) + ", " + std::to_string(color.b) + ")";
    }

    inline std::string showColor(const aiColor4D& color)
    {
        return "(" + std::to_string(color.r) + ", " + std::to_string(color.g) + ", " + std::to_string(color.b) + ", " + std::to_string(color.a) + ")";
    }

    std::pair<AssetLoader::Mesh, AssetLoader::Material> AssetLoader::processMesh(const aiNode* node, const aiMesh* mesh)
    {
        auto& scene = mpScenes.back();

        std::cerr << "start process mesh\n";
        // Data to fill
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Walk through each of the mesh's vertices
        for (uint32_t i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;

            vertex.pos = convertVec3(mesh->mVertices[i]);

            if (mesh->mNormals)
                vertex.normal = convertVec3(mesh->mNormals[i]);
            else
                vertex.normal = glm::vec3(0);

            if (mesh->mTextureCoords[0])
            {
                vertex.uv.x = (float)mesh->mTextureCoords[0][i].x;
                vertex.uv.y = (float)mesh->mTextureCoords[0][i].y;
            }
            else
            {
                //assert(!"texture coord nothing!");
                vertex.uv.x = 0;
                vertex.uv.y = 0;
            }

            vertices.push_back(vertex);
        }

        std::vector<VertexBoneData> vbdata;
        mBoneNum += mesh->mNumBones;
        if (mSkeletal)
        {
            loadBones(node, mesh, vbdata);
            // add bone infomation to vertices
            for (uint32_t i = 0; i < vertices.size(); ++i)
            {
                vertices[i].joint  = glm::make_vec4(vbdata[i].id);
                vertices[i].weight = glm::make_vec4(vbdata[i].weights);
                //std::cerr << to_string(vertices[i].weight0) << "\n";
                // float sum = vertices[i].weight.x + vertices[i].weight.y + vertices[i].weight.z + vertices[i].weight.w;
                // assert(sum <= 1.01f);
                // assert(vertices[i].joint.x < mBoneNum);
                // assert(vertices[i].joint.y < mBoneNum);
                // assert(vertices[i].joint.z < mBoneNum);
                // assert(vertices[i].joint.w < mBoneNum);
            }
        }

        if (mesh->mFaces)
        {
            for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++) indices.push_back(face.mIndices[j]);
            }
        }

        std::vector<Texture> diffuseMaps;
        Material retMaterial;
        if (mesh->mMaterialIndex >= 0 && mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiMaterial* pMat = scene->mMaterials[mesh->mMaterialIndex];
            retMaterial.name = pMat->GetName().C_Str();
            
            diffuseMaps      = loadMaterialTextures(pMat, aiTextureType_DIFFUSE, "texture_diffuse");

            if (!diffuseMaps.empty())
            {
                retMaterial.diffuse = diffuseMaps.front();
            }

            std::cerr << "material index " << mesh->mMaterialIndex << " : " << pMat->GetName().C_Str() << "\n";
            std::cerr << "material num " << pMat->mNumProperties << "\n";

            {
                aiColor4D color;
                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_DIFFUSE, &color) && diffuseMaps.empty())
                {
                    std::cerr << "\tfound diffuse : " << showColor(color) << "\n";
                    retMaterial.diffuse = convertVec4(color);
                }
                else
                {
                    std::cerr << "\tdiffuse not found...\n";
                }

                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_SPECULAR, &color))
                {
                    std::cerr << "\tfound specular : " << showColor(color) << "\n";
                    retMaterial.specular = convertVec4(color);
                }
                else
                {
                    std::cerr << "\tspecular not found...\n";
                }

                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_EMISSIVE, &color))
                {
                    std::cerr << "\tfound emissive : " << showColor(color) << "\n";
                    retMaterial.emissive = convertVec4(color);
                }
                else
                {
                    std::cerr << "\temissive not found...\n";
                }

                if (AI_SUCCESS == aiGetMaterialColor(pMat, AI_MATKEY_COLOR_TRANSPARENT, &color))
                {
                    std::cerr << "\tfound transparent : " << showColor(color) << "\n";
                    retMaterial.transparent = convertVec4(color);
                }
                else
                {
                    std::cerr << "\transparent not found...\n";
                }

                float Ns = 0.f;
                int iNs  = 0;

                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_SHININESS, &Ns))
                {
                    std::cerr << "\tfloat shininess factor : " << Ns << "\n";
                    retMaterial.shininess = Ns;
                }
                else if (AI_SUCCESS == aiGetMaterialInteger(pMat, AI_MATKEY_SHININESS, &iNs))
                {
                    std::cerr << "\tint shininess factor : " << iNs << "\n";
                    retMaterial.shininess = static_cast<float>(iNs);
                }
                else
                {
                    std::cerr << "\tshininess not found...\n";
                }

                float reflect = 0.f;
                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFLECTIVITY, &reflect))
                {
                    std::cerr << "\treflectivity : " << reflect << "\n";
                }
                else
                {
                    std::cerr << "\treflectivity not found...\n";
                }

                float IOR = 0.f;
                if (AI_SUCCESS == aiGetMaterialFloat(pMat, AI_MATKEY_REFRACTI, &IOR))
                {
                    std::cerr << "\tIOR : " << IOR << "\n";
                    retMaterial.IOR = IOR;
                }
                else
                {
                    std::cerr << "\tIOR not found...\n";
                }
            }
        }

        //std::cerr << "materials\n";

        Mesh retMesh{
            .vertices = vertices,
            .indices  = indices,
            .nodeName = std::string(node->mName.C_Str()),
            .meshName = std::string(mesh->mName.C_Str()),
        };

        std::cerr << "end process mesh\n";

        return { retMesh, retMaterial };
    }

    void AssetLoader::loadBones(const aiNode* node, const aiMesh* mesh, std::vector<VertexBoneData>& vbdata_out)
    {
        vbdata_out.resize(mesh->mNumVertices);
        for (uint32_t i = 0; i < mesh->mNumBones; i++)
        {
            uint32_t boneIndex = 0;
            std::string boneName(mesh->mBones[i]->mName.data);

            if (mSkeleton.boneMap.count(boneName) <= 0)  // bone isn't registered
            {
                boneIndex = mSkeleton.bones.size();
                mSkeleton.bones.emplace_back();
            }
            else  // found
            {
                boneIndex = mSkeleton.boneMap[boneName];
            }

            mSkeleton.boneMap[boneName]       = boneIndex;
            mSkeleton.bones[boneIndex].offset = convert4x4(mesh->mBones[i]->mOffsetMatrix);
            // for(size_t i = 0; i < 3; ++i)
            //     if(abs(mSkeleton.bones[boneIndex].offset[i][3]) > std::numeric_limits<float>::epsilon())
            //     {
            //         std::cerr << boneIndex << "\n";
            //         std::cerr << glm::to_string(mSkeleton.bones[boneIndex].offset) << "\n";
            //         assert(!"invalid bone offset!");
            //     }
            mSkeleton.bones[boneIndex].transform = glm::mat4(1.f);
            //set verticess
            for (uint32_t j = 0; j < mesh->mBones[i]->mNumWeights; j++)
            {
                size_t vertexID = mesh->mBones[i]->mWeights[j].mVertexId;
                float weight    = mesh->mBones[i]->mWeights[j].mWeight;
                for (uint32_t k = 0; k < 4; k++)
                {
                    if (vbdata_out[vertexID].weights[k] == 0.0)
                    {
                        vbdata_out[vertexID].id[k]      = boneIndex;
                        vbdata_out[vertexID].weights[k] = weight;
                        break;
                    }

                    // if(k == 3)//bad
                    //     assert(!"invalid bone weight!");
                }
            }
        }
    }

    //from sample
    std::vector<AssetLoader::Texture> AssetLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
    {
        auto& scene = mpScenes.back();

        //const auto debugTexNum = mat->GetTextureCount(type);

        std::vector<Texture> textures;
        for (uint32_t i = 0; i < mat->GetTextureCount(type); ++i)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (uint32_t j = 0; j < mTexturesLoaded.size(); ++j)
            {
                if (std::strcmp(mTexturesLoaded[j].path.c_str(), str.C_Str()) == 0)
                {
                    textures.push_back(mTexturesLoaded[j]);
                    skip = true;  // A texture with the same filepath has already been loaded, continue to next one (optimization)
                    break;
                }
            }
            if (!skip)
            {  // If texture hasn't been loaded already, load it
                Texture texture;

                const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());

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

                    texture = loadTexture(filename.c_str(), typeName.c_str());
                }
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                mTexturesLoaded.emplace_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures
            }
        }
        return textures;
    }

    AssetLoader::Texture AssetLoader::loadTexture(const char* path, const char* type)
    {
        Texture texture;
        if (!path)
        {
            assert(!"invalid texture path(nullptr)!");
            return texture;
        }
        texture.path = std::string(path);
        if (type)
            texture.type = std::string(type);
        else
            texture.type = texture.path.substr(texture.path.find_last_of("/\\"), texture.path.size());

        texture.pData = reinterpret_cast<std::byte*>(stbi_load(path, &texture.width, &texture.height, &texture.bpp, STBI_rgb_alpha));

        if (!texture.pData)
        {
            std::cerr << "failed to load texture : " << path << "\n";
            throw std::runtime_error("failed to load texture file!");
        }

        return texture;
    }
}  // namespace vk2s
