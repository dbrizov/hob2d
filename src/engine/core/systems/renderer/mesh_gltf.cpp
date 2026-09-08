#include "mesh_gltf.h"

#include <algorithm>
#include <format>
#include <utility>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "engine/math/matrix4x4.h"

namespace hob::mesh_gltf {
    namespace {
        struct ScopedData {
            cgltf_data* data = nullptr;

            ~ScopedData() {
                cgltf_free(data);
            }
        };

        const cgltf_node* find_first_mesh_node(const cgltf_data& data) {
            for (cgltf_size i = 0; i < data.nodes_count; ++i) {
                if (data.nodes[i].mesh != nullptr) {
                    return &data.nodes[i];
                }
            }

            return nullptr;
        }

        const cgltf_accessor* find_attribute(const cgltf_primitive& primitive, cgltf_attribute_type type) {
            for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
                if (primitive.attributes[i].type == type && primitive.attributes[i].index == 0) {
                    return primitive.attributes[i].data;
                }
            }

            return nullptr;
        }

        Matrix4x4 node_world_matrix(const cgltf_node& node) {
            Matrix4x4 matrix;
            cgltf_node_transform_world(&node, matrix.m.data());
            return matrix;
        }

        Vector3 to_left_handed(const Vector3& v) {
            return Vector3(v.x, v.y, -v.z);
        }

        void compute_flat_normals(MeshData& data) {
            for (size_t i = 0; i + 2 < data.indices.size(); i += 3) {
                MeshVertex& a = data.vertices[data.indices[i]];
                MeshVertex& b = data.vertices[data.indices[i + 1]];
                MeshVertex& c = data.vertices[data.indices[i + 2]];
                const Vector3 normal = Vector3::cross(b.position - a.position, c.position - a.position).normalized();
                a.normal = normal;
                b.normal = normal;
                c.normal = normal;
            }
        }
    } // namespace

    std::optional<MeshData> load(const std::filesystem::path& full_path, std::string& error) {
        const std::string path_string = full_path.string();
        cgltf_options options{};
        ScopedData scoped;
        cgltf_result result = cgltf_parse_file(&options, path_string.c_str(), &scoped.data);
        if (result != cgltf_result_success) {
            error = std::format("cgltf_parse_file failed with code {}", static_cast<int32_t>(result));
            return std::nullopt;
        }

        result = cgltf_load_buffers(&options, scoped.data, path_string.c_str());
        if (result != cgltf_result_success) {
            error = std::format("cgltf_load_buffers failed with code {}", static_cast<int32_t>(result));
            return std::nullopt;
        }

        const cgltf_data& data = *scoped.data;
        if (data.meshes_count == 0) {
            error = "the file contains no meshes";
            return std::nullopt;
        }

        const cgltf_node* node = find_first_mesh_node(data);
        const cgltf_mesh& mesh = node != nullptr ? *node->mesh : data.meshes[0];
        if (mesh.primitives_count == 0) {
            error = "the first mesh has no primitives";
            return std::nullopt;
        }

        const cgltf_primitive& primitive = mesh.primitives[0];
        if (primitive.type != cgltf_primitive_type_triangles) {
            error = "only triangle primitives are supported";
            return std::nullopt;
        }

        const cgltf_accessor* positions = find_attribute(primitive, cgltf_attribute_type_position);
        if (positions == nullptr) {
            error = "the first primitive has no POSITION attribute";
            return std::nullopt;
        }

        const cgltf_accessor* normals = find_attribute(primitive, cgltf_attribute_type_normal);
        const cgltf_accessor* uvs = find_attribute(primitive, cgltf_attribute_type_texcoord);
        const cgltf_accessor* tangents = find_attribute(primitive, cgltf_attribute_type_tangent);
        const Matrix4x4 world = node != nullptr ? node_world_matrix(*node) : Matrix4x4::identity();
        const Matrix4x4 normal_matrix = world.inverse().transpose();

        MeshData out;
        out.vertices.resize(positions->count);
        for (cgltf_size i = 0; i < positions->count; ++i) {
            MeshVertex& vertex = out.vertices[i];

            float position[3] = {};
            cgltf_accessor_read_float(positions, i, position, 3);
            vertex.position = to_left_handed(world.transform_point(Vector3(position[0], position[1], position[2])));

            if (normals != nullptr) {
                float normal[3] = {};
                cgltf_accessor_read_float(normals, i, normal, 3);
                vertex.normal = to_left_handed(
                    normal_matrix.transform_direction(Vector3(normal[0], normal[1], normal[2])).normalized());
            }

            if (uvs != nullptr) {
                float uv[2] = {};
                cgltf_accessor_read_float(uvs, i, uv, 2);
                vertex.uv = Vector2(uv[0], uv[1]);
            }

            if (tangents != nullptr) {
                float tangent[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                cgltf_accessor_read_float(tangents, i, tangent, 4);
                vertex.tangent =
                    to_left_handed(world.transform_direction(Vector3(tangent[0], tangent[1], tangent[2])).normalized());
                vertex.tangent_sign = -tangent[3];
            }
        }

        if (primitive.indices != nullptr) {
            out.indices.resize(primitive.indices->count);
            for (cgltf_size i = 0; i < primitive.indices->count; ++i) {
                out.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
            }
        }
        else {
            out.indices.resize(positions->count);
            for (cgltf_size i = 0; i < positions->count; ++i) {
                out.indices[i] = static_cast<uint32_t>(i);
            }
        }

        if (std::ranges::any_of(out.indices, [&out](uint32_t index) {
                return index >= out.vertices.size();
            })) {
            error = "an index is out of range";
            return std::nullopt;
        }

        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            std::swap(out.indices[i + 1], out.indices[i + 2]);
        }

        if (normals == nullptr) {
            compute_flat_normals(out);
        }

        if (tangents == nullptr) {
            out.compute_tangents();
        }

        out.recompute_bounds();
        return out;
    }
} // namespace hob::mesh_gltf
