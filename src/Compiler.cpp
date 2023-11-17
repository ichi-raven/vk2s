#include "../include/vk2s/Compiler.hpp"

#include <glslang/SPIRV/GlslangToSpv.h>

#include <spirv_reflect.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vk2s
{
    namespace Compiler
    {
        TBuiltInResource initResources()
        {
            TBuiltInResource Resources{};

            Resources.maxLights                                 = 32;
            Resources.maxClipPlanes                             = 6;
            Resources.maxTextureUnits                           = 32;
            Resources.maxTextureCoords                          = 32;
            Resources.maxVertexAttribs                          = 64;
            Resources.maxVertexUniformComponents                = 4096;
            Resources.maxVaryingFloats                          = 64;
            Resources.maxVertexTextureImageUnits                = 32;
            Resources.maxCombinedTextureImageUnits              = 80;
            Resources.maxTextureImageUnits                      = 32;
            Resources.maxFragmentUniformComponents              = 4096;
            Resources.maxDrawBuffers                            = 32;
            Resources.maxVertexUniformVectors                   = 128;
            Resources.maxVaryingVectors                         = 8;
            Resources.maxFragmentUniformVectors                 = 16;
            Resources.maxVertexOutputVectors                    = 16;
            Resources.maxFragmentInputVectors                   = 15;
            Resources.minProgramTexelOffset                     = -8;
            Resources.maxProgramTexelOffset                     = 7;
            Resources.maxClipDistances                          = 8;
            Resources.maxComputeWorkGroupCountX                 = 65535;
            Resources.maxComputeWorkGroupCountY                 = 65535;
            Resources.maxComputeWorkGroupCountZ                 = 65535;
            Resources.maxComputeWorkGroupSizeX                  = 1024;
            Resources.maxComputeWorkGroupSizeY                  = 1024;
            Resources.maxComputeWorkGroupSizeZ                  = 64;
            Resources.maxComputeUniformComponents               = 1024;
            Resources.maxComputeTextureImageUnits               = 16;
            Resources.maxComputeImageUniforms                   = 8;
            Resources.maxComputeAtomicCounters                  = 8;
            Resources.maxComputeAtomicCounterBuffers            = 1;
            Resources.maxVaryingComponents                      = 60;
            Resources.maxVertexOutputComponents                 = 64;
            Resources.maxGeometryInputComponents                = 64;
            Resources.maxGeometryOutputComponents               = 128;
            Resources.maxFragmentInputComponents                = 128;
            Resources.maxImageUnits                             = 8;
            Resources.maxCombinedImageUnitsAndFragmentOutputs   = 8;
            Resources.maxCombinedShaderOutputResources          = 8;
            Resources.maxImageSamples                           = 0;
            Resources.maxVertexImageUniforms                    = 0;
            Resources.maxTessControlImageUniforms               = 0;
            Resources.maxTessEvaluationImageUniforms            = 0;
            Resources.maxGeometryImageUniforms                  = 0;
            Resources.maxFragmentImageUniforms                  = 8;
            Resources.maxCombinedImageUniforms                  = 8;
            Resources.maxGeometryTextureImageUnits              = 16;
            Resources.maxGeometryOutputVertices                 = 256;
            Resources.maxGeometryTotalOutputComponents          = 1024;
            Resources.maxGeometryUniformComponents              = 1024;
            Resources.maxGeometryVaryingComponents              = 64;
            Resources.maxTessControlInputComponents             = 128;
            Resources.maxTessControlOutputComponents            = 128;
            Resources.maxTessControlTextureImageUnits           = 16;
            Resources.maxTessControlUniformComponents           = 1024;
            Resources.maxTessControlTotalOutputComponents       = 4096;
            Resources.maxTessEvaluationInputComponents          = 128;
            Resources.maxTessEvaluationOutputComponents         = 128;
            Resources.maxTessEvaluationTextureImageUnits        = 16;
            Resources.maxTessEvaluationUniformComponents        = 1024;
            Resources.maxTessPatchComponents                    = 120;
            Resources.maxPatchVertices                          = 32;
            Resources.maxTessGenLevel                           = 64;
            Resources.maxViewports                              = 16;
            Resources.maxVertexAtomicCounters                   = 0;
            Resources.maxTessControlAtomicCounters              = 0;
            Resources.maxTessEvaluationAtomicCounters           = 0;
            Resources.maxGeometryAtomicCounters                 = 0;
            Resources.maxFragmentAtomicCounters                 = 8;
            Resources.maxCombinedAtomicCounters                 = 8;
            Resources.maxAtomicCounterBindings                  = 1;
            Resources.maxVertexAtomicCounterBuffers             = 0;
            Resources.maxTessControlAtomicCounterBuffers        = 0;
            Resources.maxTessEvaluationAtomicCounterBuffers     = 0;
            Resources.maxGeometryAtomicCounterBuffers           = 0;
            Resources.maxFragmentAtomicCounterBuffers           = 1;
            Resources.maxCombinedAtomicCounterBuffers           = 1;
            Resources.maxAtomicCounterBufferSize                = 16384;
            Resources.maxTransformFeedbackBuffers               = 4;
            Resources.maxTransformFeedbackInterleavedComponents = 64;
            Resources.maxCullDistances                          = 8;
            Resources.maxCombinedClipAndCullDistances           = 8;
            Resources.maxSamples                                = 4;
            Resources.maxMeshOutputVerticesNV                   = 256;
            Resources.maxMeshOutputPrimitivesNV                 = 512;
            Resources.maxMeshWorkGroupSizeX_NV                  = 32;
            Resources.maxMeshWorkGroupSizeY_NV                  = 1;
            Resources.maxMeshWorkGroupSizeZ_NV                  = 1;
            Resources.maxTaskWorkGroupSizeX_NV                  = 32;
            Resources.maxTaskWorkGroupSizeY_NV                  = 1;
            Resources.maxTaskWorkGroupSizeZ_NV                  = 1;
            Resources.maxMeshViewCountNV                        = 4;

            Resources.limits.nonInductiveForLoops                 = 1;
            Resources.limits.whileLoops                           = 1;
            Resources.limits.doWhileLoops                         = 1;
            Resources.limits.generalUniformIndexing               = 1;
            Resources.limits.generalAttributeMatrixVectorIndexing = 1;
            Resources.limits.generalVaryingIndexing               = 1;
            Resources.limits.generalSamplerIndexing               = 1;
            Resources.limits.generalVariableIndexing              = 1;
            Resources.limits.generalConstantMatrixVectorIndexing  = 1;

            return Resources;
        }

        std::string readFile(std::string_view path)
        {
            std::ifstream file(path.data());
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + std::string(path));
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        EShLanguage translateShaderStage(std::string_view filepath)
        {
            if (filepath.ends_with("vert"))
                return EShLangVertex;
            else if (filepath.ends_with("frag"))
                return EShLangFragment;
            else if (filepath.ends_with("comp"))
                return EShLangCompute;
            else if (filepath.ends_with("rgen"))
                return EShLangRayGenNV;
            else if (filepath.ends_with("rmiss"))
                return EShLangMissNV;
            else if (filepath.ends_with("rchit"))
                return EShLangClosestHitNV;
            else if (filepath.ends_with("rahit"))
                return EShLangAnyHitNV;

            assert(!"Unknown shader stage");

            return EShLangVertex;
        }

        SPIRVCode compileText(EShLanguage stage, const std::string& glslShader)
        {
            glslang::InitializeProcess();

            const char* shaderStrings[1];
            shaderStrings[0] = glslShader.data();

            glslang::TShader shader(stage);

            shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_5);
            shader.setStrings(shaderStrings, 1);

            EShMessages messages  = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
            auto defaultResources = initResources();
            if (!shader.parse(&defaultResources, 100, false, messages))
            {
                throw std::runtime_error(glslShader + "\n" + shader.getInfoLog());
            }

            glslang::TProgram program;
            program.addShader(&shader);

            if (!program.link(messages))
            {
                throw std::runtime_error(glslShader + "\n" + shader.getInfoLog());
            }

            std::vector<uint32_t> spvShader;
            glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
            glslang::FinalizeProcess();
            return spvShader;
        }

        SPIRVCode compileFile(std::string_view path)
        {
            EShLanguage stage = translateShaderStage(path);
            return compileText(stage, readFile(path));
        }

        ReflectionResult getReflection(const SPIRVCode& fileData)
        {
            //load shader module
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(sizeof(fileData[0]) * fileData.size(), fileData.data(), &module);
            if (result != SPV_REFLECT_RESULT_SUCCESS)
            {
                throw std::runtime_error("failed to create SPIRV-Reflect shader module!");
            }

            if (!module.entry_points)
            {
                throw std::runtime_error("missing entry point from shader!");
            }

            //get desecriptor set layout
            std::vector<SpvReflectDescriptorSet*> sets;
            {
                uint32_t count = 0;
                result         = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
                assert(result == SPV_REFLECT_RESULT_SUCCESS);

                sets.resize(count);

                result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
                assert(result == SPV_REFLECT_RESULT_SUCCESS);
            }

            // get in/out variables ------------
            static auto lmdComp = [](const SpvReflectInterfaceVariable* l, const SpvReflectInterfaceVariable* r) -> bool { return l->location < r->location; };

            std::vector<SpvReflectInterfaceVariable*> inputVars;
            {
                uint32_t count = 0;
                result         = spvReflectEnumerateInputVariables(&module, &count, NULL);
                assert(result == SPV_REFLECT_RESULT_SUCCESS);
                inputVars.resize(count);
                result = spvReflectEnumerateInputVariables(&module, &count, inputVars.data());
                assert(result == SPV_REFLECT_RESULT_SUCCESS);
                if (!inputVars.empty()) std::sort(inputVars.begin(), inputVars.end(), lmdComp);
            }

            std::vector<SpvReflectInterfaceVariable*> outputVars;
            {
                uint32_t count = 0;
                result         = spvReflectEnumerateOutputVariables(&module, &count, NULL);
                assert(result == SPV_REFLECT_RESULT_SUCCESS);

                outputVars.resize(count);
                result = spvReflectEnumerateOutputVariables(&module, &count, outputVars.data());
                assert(result == SPV_REFLECT_RESULT_SUCCESS);
                if (!outputVars.empty()) std::sort(outputVars.begin(), outputVars.end(), lmdComp);
            }

            std::vector<vk::VertexInputAttributeDescription> inputAttributedDescs;
            std::map<std::pair<uint32_t, uint32_t>, vk::DescriptorType> resourcesMap;

            {  // input attribute
                std::uint32_t nowOffset = 0;

                for (std::size_t i = 0; i < inputVars.size(); ++i)
                {
                    // please optimize this
                    const auto format = inputVars[i];
                    auto& ia          = inputAttributedDescs.emplace_back();

                    ia.binding  = 0;
                    ia.location = static_cast<std::uint32_t>(i);
                    ia.format   = static_cast<vk::Format>(format->format);
                    ia.offset   = nowOffset;

                    nowOffset += getSizeOfFormat(ia.format);
                }
            }

            {  // descriptorSetLayouts
                for (size_t i = 0; i < sets.size(); ++i)
                {
                    for (size_t j = 0; j < sets[i]->binding_count; ++j)
                    {
                        const vk::DescriptorType srt = static_cast<vk::DescriptorType>(sets[i]->bindings[j]->descriptor_type);

                        resourcesMap.emplace(std::pair<uint8_t, uint8_t>(sets[i]->set, sets[i]->bindings[j]->binding), srt);
                    }
                }
            }

            spvReflectDestroyShaderModule(&module);
            return { inputAttributedDescs, resourcesMap };
        }

        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> createDescriptorSetLayoutBindings(const ShaderResourceMap& resourceMap)
        {
            std::vector<std::vector<vk::DescriptorSetLayoutBinding>> allBindings;
            allBindings.reserve(8);
            {  // HACK
                std::uint32_t nowSet = UINT32_MAX;

                for (const auto& [sb, srt] : resourceMap)
                {
                    if (sb.first != nowSet)
                    {
                        allBindings.emplace_back();
                        nowSet = sb.first;
                    }

                    auto&& b             = allBindings.back().emplace_back();
                    b.binding            = sb.second;
                    b.descriptorCount    = 1;
                    b.descriptorType     = srt;
                    b.pImmutableSamplers = nullptr;
                    b.stageFlags         = vk::ShaderStageFlagBits::eAll;
                }
            }

            return allBindings;
        }

        uint32_t getSizeOfFormat(const vk::Format format) noexcept
        {
            VkFormat internalFormat = static_cast<VkFormat>(format);

            switch (internalFormat)
            {
            case VK_FORMAT_UNDEFINED:
                return 0;
            case VK_FORMAT_R4G4_UNORM_PACK8:
                return 1;
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
                return 2;
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
                return 2;
            case VK_FORMAT_R5G6B5_UNORM_PACK16:
                return 2;
            case VK_FORMAT_B5G6R5_UNORM_PACK16:
                return 2;
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
                return 2;
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
                return 2;
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
                return 2;
            case VK_FORMAT_R8_UNORM:
                return 1;
            case VK_FORMAT_R8_SNORM:
                return 1;
            case VK_FORMAT_R8_USCALED:
                return 1;
            case VK_FORMAT_R8_SSCALED:
                return 1;
            case VK_FORMAT_R8_UINT:
                return 1;
            case VK_FORMAT_R8_SINT:
                return 1;
            case VK_FORMAT_R8_SRGB:
                return 1;
            case VK_FORMAT_R8G8_UNORM:
                return 2;
            case VK_FORMAT_R8G8_SNORM:
                return 2;
            case VK_FORMAT_R8G8_USCALED:
                return 2;
            case VK_FORMAT_R8G8_SSCALED:
                return 2;
            case VK_FORMAT_R8G8_UINT:
                return 2;
            case VK_FORMAT_R8G8_SINT:
                return 2;
            case VK_FORMAT_R8G8_SRGB:
                return 2;
            case VK_FORMAT_R8G8B8_UNORM:
                return 3;
            case VK_FORMAT_R8G8B8_SNORM:
                return 3;
            case VK_FORMAT_R8G8B8_USCALED:
                return 3;
            case VK_FORMAT_R8G8B8_SSCALED:
                return 3;
            case VK_FORMAT_R8G8B8_UINT:
                return 3;
            case VK_FORMAT_R8G8B8_SINT:
                return 3;
            case VK_FORMAT_R8G8B8_SRGB:
                return 3;
            case VK_FORMAT_B8G8R8_UNORM:
                return 3;
            case VK_FORMAT_B8G8R8_SNORM:
                return 3;
            case VK_FORMAT_B8G8R8_USCALED:
                return 3;
            case VK_FORMAT_B8G8R8_SSCALED:
                return 3;
            case VK_FORMAT_B8G8R8_UINT:
                return 3;
            case VK_FORMAT_B8G8R8_SINT:
                return 3;
            case VK_FORMAT_B8G8R8_SRGB:
                return 3;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return 4;
            case VK_FORMAT_R8G8B8A8_SNORM:
                return 4;
            case VK_FORMAT_R8G8B8A8_USCALED:
                return 4;
            case VK_FORMAT_R8G8B8A8_SSCALED:
                return 4;
            case VK_FORMAT_R8G8B8A8_UINT:
                return 4;
            case VK_FORMAT_R8G8B8A8_SINT:
                return 4;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return 4;
            case VK_FORMAT_B8G8R8A8_UNORM:
                return 4;
            case VK_FORMAT_B8G8R8A8_SNORM:
                return 4;
            case VK_FORMAT_B8G8R8A8_USCALED:
                return 4;
            case VK_FORMAT_B8G8R8A8_SSCALED:
                return 4;
            case VK_FORMAT_B8G8R8A8_UINT:
                return 4;
            case VK_FORMAT_B8G8R8A8_SINT:
                return 4;
            case VK_FORMAT_B8G8R8A8_SRGB:
                return 4;
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
                return 4;
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
                return 4;
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
                return 4;
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:
                return 4;
            case VK_FORMAT_R16_UNORM:
                return 2;
            case VK_FORMAT_R16_SNORM:
                return 2;
            case VK_FORMAT_R16_USCALED:
                return 2;
            case VK_FORMAT_R16_SSCALED:
                return 2;
            case VK_FORMAT_R16_UINT:
                return 2;
            case VK_FORMAT_R16_SINT:
                return 2;
            case VK_FORMAT_R16_SFLOAT:
                return 2;
            case VK_FORMAT_R16G16_UNORM:
                return 4;
            case VK_FORMAT_R16G16_SNORM:
                return 4;
            case VK_FORMAT_R16G16_USCALED:
                return 4;
            case VK_FORMAT_R16G16_SSCALED:
                return 4;
            case VK_FORMAT_R16G16_UINT:
                return 4;
            case VK_FORMAT_R16G16_SINT:
                return 4;
            case VK_FORMAT_R16G16_SFLOAT:
                return 4;
            case VK_FORMAT_R16G16B16_UNORM:
                return 6;
            case VK_FORMAT_R16G16B16_SNORM:
                return 6;
            case VK_FORMAT_R16G16B16_USCALED:
                return 6;
            case VK_FORMAT_R16G16B16_SSCALED:
                return 6;
            case VK_FORMAT_R16G16B16_UINT:
                return 6;
            case VK_FORMAT_R16G16B16_SINT:
                return 6;
            case VK_FORMAT_R16G16B16_SFLOAT:
                return 6;
            case VK_FORMAT_R16G16B16A16_UNORM:
                return 8;
            case VK_FORMAT_R16G16B16A16_SNORM:
                return 8;
            case VK_FORMAT_R16G16B16A16_USCALED:
                return 8;
            case VK_FORMAT_R16G16B16A16_SSCALED:
                return 8;
            case VK_FORMAT_R16G16B16A16_UINT:
                return 8;
            case VK_FORMAT_R16G16B16A16_SINT:
                return 8;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return 8;
            case VK_FORMAT_R32_UINT:
                return 4;
            case VK_FORMAT_R32_SINT:
                return 4;
            case VK_FORMAT_R32_SFLOAT:
                return 4;
            case VK_FORMAT_R32G32_UINT:
                return 8;
            case VK_FORMAT_R32G32_SINT:
                return 8;
            case VK_FORMAT_R32G32_SFLOAT:
                return 8;
            case VK_FORMAT_R32G32B32_UINT:
                return 12;
            case VK_FORMAT_R32G32B32_SINT:
                return 12;
            case VK_FORMAT_R32G32B32_SFLOAT:
                return 12;
            case VK_FORMAT_R32G32B32A32_UINT:
                return 16;
            case VK_FORMAT_R32G32B32A32_SINT:
                return 16;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return 16;
            case VK_FORMAT_R64_UINT:
                return 8;
            case VK_FORMAT_R64_SINT:
                return 8;
            case VK_FORMAT_R64_SFLOAT:
                return 8;
            case VK_FORMAT_R64G64_UINT:
                return 16;
            case VK_FORMAT_R64G64_SINT:
                return 16;
            case VK_FORMAT_R64G64_SFLOAT:
                return 16;
            case VK_FORMAT_R64G64B64_UINT:
                return 24;
            case VK_FORMAT_R64G64B64_SINT:
                return 24;
            case VK_FORMAT_R64G64B64_SFLOAT:
                return 24;
            case VK_FORMAT_R64G64B64A64_UINT:
                return 32;
            case VK_FORMAT_R64G64B64A64_SINT:
                return 32;
            case VK_FORMAT_R64G64B64A64_SFLOAT:
                return 32;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                return 4;
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
                return 4;
            case VK_FORMAT_D16_UNORM:
                return 2;
            case VK_FORMAT_X8_D24_UNORM_PACK32:
                return 4;
            case VK_FORMAT_D32_SFLOAT:
                return 4;
            case VK_FORMAT_S8_UINT:
                return 1;
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return 3;
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return 4;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return 8;
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
                return 8;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
                return 8;
            case VK_FORMAT_BC2_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_BC2_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_BC3_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_BC3_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_BC4_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_BC4_SNORM_BLOCK:
                return 8;
            case VK_FORMAT_BC5_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_BC5_SNORM_BLOCK:
                return 16;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
                return 16;
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
                return 16;
            case VK_FORMAT_BC7_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_BC7_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
                return 8;
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
                return 8;
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:
                return 8;
            case VK_FORMAT_EAC_R11_SNORM_BLOCK:
                return 8;
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
                return 16;
            case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
                return 16;
            case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
                return 8;
            case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
                return 8;
            case VK_FORMAT_R10X6_UNORM_PACK16:
                return 2;
            case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
                return 4;
            case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_R12X4_UNORM_PACK16:
                return 2;
            case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
                return 4;
            case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_G8B8G8R8_422_UNORM:
                return 4;
            case VK_FORMAT_B8G8R8G8_422_UNORM:
                return 4;
            case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
                return 8;
            case VK_FORMAT_G16B16G16R16_422_UNORM:
                return 8;
            case VK_FORMAT_B16G16R16G16_422_UNORM:
                return 8;
            case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
                return 6;
            case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
                return 6;
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
                return 12;
            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
                return 12;
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
                return 12;
            case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
                return 12;
            case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
                return 12;
            case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
                return 12;
            case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
                return 4;
            case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
                return 4;
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
                return 8;
            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
                return 8;
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
                return 8;
            case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
                return 8;
            case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
                return 8;
            case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
                return 8;
            case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
                return 3;
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
                return 6;
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
                return 6;
            case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
                return 6;
            default:
                assert(!"invalid format type!");
                break;
            }

            assert(!"invalid format type!");
            return 0;
        }

    }  // namespace Compiler
}  // namespace vk2s