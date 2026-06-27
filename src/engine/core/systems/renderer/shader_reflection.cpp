#include "shader_reflection.h"

#include <spirv-reflect/spirv_reflect.h>

namespace hob {
    namespace {
        // Flatten SPIR-V's (base scalar + vector/matrix traits) into one composite ShaderParamType.
        ShaderParamType to_param_type(const SpvReflectTypeDescription* type) {
            if (!type) {
                return ShaderParamType::Unknown;
            }

            if (type->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) {
                return ShaderParamType::Bool;
            }
            if (!(type->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)) {
                return ShaderParamType::Unknown;
            }

            if (type->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
                const uint32_t rows = type->traits.numeric.matrix.row_count;
                const uint32_t cols = type->traits.numeric.matrix.column_count;
                if (rows == 4 && cols == 4) {
                    return ShaderParamType::Float4x4;
                }
                if (rows == 3 && cols == 3) {
                    return ShaderParamType::Float3x3;
                }
                return ShaderParamType::Unknown;
            }

            const uint32_t comp_count =
                (type->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) ? type->traits.numeric.vector.component_count : 1;

            static constexpr ShaderParamType k_float[5] = {ShaderParamType::Unknown,
                                                           ShaderParamType::Float,
                                                           ShaderParamType::Float2,
                                                           ShaderParamType::Float3,
                                                           ShaderParamType::Float4};
            return (comp_count >= 1 && comp_count <= 4) ? k_float[comp_count] : ShaderParamType::Unknown;
        }

        uint32_t array_count(const SpvReflectBlockVariable& member) {
            uint32_t total = 0;
            for (uint32_t i = 0; i < member.array.dims_count; ++i) {
                total = (total == 0) ? member.array.dims[i] : total * member.array.dims[i];
            }
            return total;
        }
    } // namespace

    const char* to_string(ShaderParamType type) {
        switch (type) {
            case ShaderParamType::Bool:
                return "bool";
            case ShaderParamType::Float:
                return "float";
            case ShaderParamType::Float2:
                return "float2";
            case ShaderParamType::Float3:
                return "float3";
            case ShaderParamType::Float4:
                return "float4";
            case ShaderParamType::Float3x3:
                return "float3x3";
            case ShaderParamType::Float4x4:
                return "float4x4";
            case ShaderParamType::Unknown:
            default:
                return "unknown";
        }
    }

    uint32_t component_count(ShaderParamType type) {
        switch (type) {
            case ShaderParamType::Bool:
            case ShaderParamType::Float:
                return 1;
            case ShaderParamType::Float2:
                return 2;
            case ShaderParamType::Float3:
                return 3;
            case ShaderParamType::Float4:
                return 4;
            case ShaderParamType::Float3x3:
                return 9;
            case ShaderParamType::Float4x4:
                return 16;
            case ShaderParamType::Unknown:
            default:
                return 0;
        }
    }

    bool reflect_spirv(const void* spirv, size_t spirv_size, ShaderReflection& out) {
        out = {};

        SpvReflectShaderModule module{};
        if (spvReflectCreateShaderModule(spirv_size, spirv, &module) != SPV_REFLECT_RESULT_SUCCESS) {
            return false;
        }

        bool success = true;

        // Descriptor bindings: uniform buffers (cbuffers) and textures.
        uint32_t binding_count = 0;
        if (spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr) == SPV_REFLECT_RESULT_SUCCESS) {
            std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
            spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

            for (const SpvReflectDescriptorBinding* binding : bindings) {
                if (!binding) {
                    continue;
                }

                switch (binding->descriptor_type) {
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                        ShaderUniformBlock block;
                        block.name = binding->name ? binding->name : "";
                        block.type_name = (binding->type_description && binding->type_description->type_name)
                                              ? binding->type_description->type_name
                                              : "";
                        block.set = binding->set;
                        block.binding = binding->binding;
                        block.size = binding->block.size;
                        block.members.reserve(binding->block.member_count);
                        for (uint32_t i = 0; i < binding->block.member_count; ++i) {
                            const SpvReflectBlockVariable& m = binding->block.members[i];
                            ShaderUniformMember member;
                            member.name = m.name ? m.name : "";
                            member.type = to_param_type(m.type_description);
                            member.offset = m.offset;
                            member.size = m.size;
                            member.array_count = array_count(m);
                            block.members.push_back(std::move(member));
                        }
                        out.uniform_blocks.push_back(std::move(block));
                        break;
                    }
                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        ShaderTextureBinding tex;
                        tex.name = binding->name ? binding->name : "";
                        tex.set = binding->set;
                        tex.binding = binding->binding;
                        out.textures.push_back(std::move(tex));
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        else {
            success = false;
        }

        // Vertex input variables (only meaningful for the vertex stage).
        uint32_t input_count = 0;
        if (spvReflectEnumerateInputVariables(&module, &input_count, nullptr) == SPV_REFLECT_RESULT_SUCCESS) {
            std::vector<SpvReflectInterfaceVariable*> inputs(input_count);
            spvReflectEnumerateInputVariables(&module, &input_count, inputs.data());

            for (const SpvReflectInterfaceVariable* input : inputs) {
                if (!input) {
                    continue;
                }
                // Skip built-ins (SV_VertexID, etc.) — they have no location.
                if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
                    continue;
                }
                ShaderVertexInput vi;
                vi.name = input->name ? input->name : "";
                vi.type = to_param_type(input->type_description);
                vi.location = input->location;
                out.vertex_inputs.push_back(std::move(vi));
            }
        }
        else {
            success = false;
        }

        spvReflectDestroyShaderModule(&module);
        return success;
    }
} // namespace hob
