#include "../include/vk2s/Compiler.hpp"


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

        class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
        {
        public:

            ShaderIncluder(std::string_view directory)
                : mDirectory(directory)
                , shaderc::CompileOptions::IncluderInterface()
            {

            }

            shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
            {
                //std::cout << std::string(mDirectory) + "/" + std::string(requested_source) << "\n";
                //std::cout << std::to_string(type) << "\n";
                //std::cout << requesting_source << "\n";
                //std::cout << include_depth << "\n";

                const std::string name     = std::string(mDirectory) + "/" + std::string(requested_source);
                const std::string contents = readFile(name);

                auto container  = new std::array<std::string, 2>;
                (*container)[0] = name;
                (*container)[1] = contents;

                auto data = new shaderc_include_result;

                data->user_data = container;

                data->source_name        = (*container)[0].data();
                data->source_name_length = (*container)[0].size();

                data->content        = (*container)[1].data();
                data->content_length = (*container)[1].size();

                return data;
            };

            void ReleaseInclude(shaderc_include_result* data) override
            {
                delete static_cast<std::array<std::string, 2>*>(data->user_data);
                delete data;
            };

            std::string_view mDirectory;
        };

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

        shaderc_shader_kind getShaderStage(std::string_view filepath)
        {
            if (filepath.ends_with("vert"))
                return shaderc_shader_kind::shaderc_vertex_shader;
            else if (filepath.ends_with("frag"))
                return shaderc_shader_kind::shaderc_fragment_shader;
            else if (filepath.ends_with("comp"))
                return shaderc_shader_kind::shaderc_compute_shader;
            else if (filepath.ends_with("rgen"))
                return shaderc_shader_kind::shaderc_raygen_shader;
            else if (filepath.ends_with("rmiss"))
                return shaderc_shader_kind::shaderc_miss_shader;
            else if (filepath.ends_with("rchit"))
                return shaderc_shader_kind::shaderc_closesthit_shader;
            else if (filepath.ends_with("rahit"))
                return shaderc_shader_kind::shaderc_anyhit_shader;

            assert(!"Unknown shader stage!");

            return shaderc_shader_kind::shaderc_vertex_shader;
        }

        SPIRVCode compileFile(std::string_view path, const bool optimize)
        {
            const auto shaderSource = readFile(path);
            const auto kind         = getShaderStage(path);

            const char* const shaderName = "compiling_shader";

            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            options.SetTargetSpirv(shaderc_spirv_version_1_6);
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            const std::string directory = std::string(path).substr(0, path.find_last_of("/\\"));
            options.SetIncluder(std::make_unique<ShaderIncluder>(directory));

            // Preprocessing
            shaderc::PreprocessedSourceCompilationResult result = compiler.PreprocessGlsl(shaderSource, kind, shaderName, options);

            if (result.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                std::cerr << result.GetErrorMessage();
                return SPIRVCode();
            }

            std::string preprocessed = { result.cbegin(), result.cend() };

            // Compiling

            if (optimize)
            {
                options.SetOptimizationLevel(shaderc_optimization_level_performance);
            }

            // disassemble
            //result = compiler.CompileGlslToSpvAssembly(preprocessed, kind, shaderName, options);

            //if (result.GetCompilationStatus() != shaderc_compilation_status_success)
            //{
            //    std::cerr << result.GetErrorMessage();
            //    return SPIRVCode();
            //}
            //std::string assembly = { result.cbegin(), result.cend() };
            //std::cout << "SPIR-V assembly:" << std::endl << assembly << std::endl;

            shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(preprocessed, kind, shaderName, options);

            if (module.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                std::cerr << module.GetErrorMessage();
                return SPIRVCode();
            }

            //std::cout << "Compiled to an binary module with " << spirv.size() << " words." << std::endl;

            return { module.cbegin(), module.cend() };
        }

        ReflectionResult getReflection(const SPIRVCode& fileData)
        {
            //load shader module
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(sizeof(fileData[0]) * fileData.size(), fileData.data(), &module);
            if (result != SPV_REFLECT_RESULT_SUCCESS)
            {
                assert(!"failed to create SPIRV-Reflect shader module!");
            }

            if (!module.entry_points)
            {
                assert(!"missing entry point from shader!");
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