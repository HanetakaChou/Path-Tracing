//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "asset_import.h"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <assert.h>
#include "../third-party/cgltf/cgltf.h"

extern "C" void *cgltf_tf_alloc(void *, cgltf_size size);
extern "C" void cgltf_tf_free(void *, void *ptr);
extern "C" cgltf_result cgltf_fs_read_file(const struct cgltf_memory_options *memory_options, const struct cgltf_file_options *, const char *path, cgltf_size *size, void **data);
extern "C" void cgltf_fs_file_release(const struct cgltf_memory_options *memory_options, const struct cgltf_file_options *, void *data);

static void asset_import_gltf_mesh(mesh_asset_data *out_mesh_data, int32_t *out_max_joint_index, cgltf_data const *data, cgltf_mesh const *mesh);

static void asset_import_gltf_mesh_instances(std::vector<cgltf_node const *> &out_mesh_instance_nodes, std::vector<DirectX::XMFLOAT4X4> &out_mesh_instance_node_world_transforms, cgltf_data const *data, cgltf_mesh const *mesh);

static void asset_import_gltf_animation(animated_skeleton *out_animated_skeleton, float frame_rate, cgltf_data const *data, cgltf_skin const *skin, cgltf_animation const *animation);

extern void asset_import_gltf(std::vector<mesh_asset_data> &out_mesh_data, std::vector<std::vector<mesh_instance_data>> &out_mesh_instance_data, float frame_rate, char const *path)
{
    cgltf_data *data = NULL;
    {
        cgltf_options options = {};
        options.memory.alloc_func = cgltf_tf_alloc;
        options.memory.free_func = cgltf_tf_free;
        options.file.read = cgltf_fs_read_file;
        options.file.release = cgltf_fs_file_release;

        cgltf_result result_parse_file = cgltf_parse_file(&options, path, &data);
        assert(cgltf_result_success == result_parse_file);

        cgltf_result result_load_buffers = cgltf_load_buffers(&options, data, path);
        assert(cgltf_result_success == result_load_buffers);
    }

    for (size_t mesh_index = 0; mesh_index < data->meshes_count; ++mesh_index)
    {
        out_mesh_data.push_back({});
        mesh_asset_data &mesh_data = out_mesh_data.back();
        int32_t max_joint_index;
        asset_import_gltf_mesh(&mesh_data, &max_joint_index, data, &data->meshes[mesh_index]);

        std::vector<cgltf_node const *> mesh_instance_nodes;
        std::vector<DirectX::XMFLOAT4X4> mesh_instance_node_world_transforms;
        asset_import_gltf_mesh_instances(mesh_instance_nodes, mesh_instance_node_world_transforms, data, &data->meshes[mesh_index]);

        size_t const mesh_instance_count = mesh_instance_nodes.size();
        assert(mesh_instance_node_world_transforms.size() == mesh_instance_count);

        out_mesh_instance_data.push_back({});
        std::vector<mesh_instance_data> &mesh_instance_data = out_mesh_instance_data.back();
        if (!mesh_data.m_skinned)
        {
            assert(max_joint_index < 0);

            for (size_t mesh_instance_index = 0; mesh_instance_index < mesh_instance_count; ++mesh_instance_index)
            {
                assert(NULL == mesh_instance_nodes[mesh_instance_index]->skin);
                mesh_instance_data.push_back({mesh_instance_node_world_transforms[mesh_instance_index]});
            }
        }
        else
        {
            assert(max_joint_index >= 0);

            for (size_t mesh_instance_index = 0; mesh_instance_index < mesh_instance_count; ++mesh_instance_index)
            {
                cgltf_skin const *skin = mesh_instance_nodes[mesh_instance_index]->skin;
                if (NULL != skin)
                {
                    // TODO: support multiple animations
                    cgltf_animation const *animation = ((data->animations_count > 0) ? (&data->animations[0]) : NULL);

#ifndef NDEBUG
                    // glTF Validator
                    // NODE_SKINNED_MESH_NON_ROOT
                    // NODE_SKINNED_MESH_LOCAL_TRANSFORMS
                    DirectX::XMVECTOR out_instance_node_world_scale;
                    DirectX::XMVECTOR out_instance_node_world_rotation;
                    DirectX::XMVECTOR out_instance_node_world_translation;
                    DirectX::XMMatrixDecompose(&out_instance_node_world_scale, &out_instance_node_world_rotation, &out_instance_node_world_translation, DirectX::XMLoadFloat4x4(&mesh_instance_node_world_transforms[mesh_instance_index]));

                    // FLT_EPSILON
                    constexpr float const scale_epsilon = 1E-5F;
                    assert(DirectX::XMVector3EqualInt(DirectX::XMVectorTrueInt(), DirectX::XMVectorLess(DirectX::XMVectorAbs(DirectX::XMVectorSubtract(out_instance_node_world_scale, DirectX::XMVectorSplatOne())), DirectX::XMVectorReplicate(scale_epsilon))));
                    constexpr float const rotation_epsilon = 1E-5F;
                    assert(DirectX::XMVector3EqualInt(DirectX::XMVectorTrueInt(), DirectX::XMVectorLess(DirectX::XMVectorAbs(DirectX::XMVectorSubtract(out_instance_node_world_rotation, DirectX::XMQuaternionIdentity())), DirectX::XMVectorReplicate(rotation_epsilon))));
                    constexpr float const translation_epsilon = 1E-5F;
                    assert(DirectX::XMVector3EqualInt(DirectX::XMVectorTrueInt(), DirectX::XMVectorLess(DirectX::XMVectorAbs(DirectX::XMVectorSubtract(out_instance_node_world_translation, DirectX::XMVectorZero())), DirectX::XMVectorReplicate(translation_epsilon))));
#endif
                    DirectX::XMFLOAT4X4 model_transform;
                    DirectX::XMStoreFloat4x4(&model_transform, DirectX::XMMatrixIdentity());

                    mesh_instance_data.push_back({model_transform});

                    asset_import_gltf_animation(&mesh_instance_data.back().m_animation_skeleton, frame_rate, data, skin, animation);
                }
                else
                {
                    // TODO: This should NOT happen
                    assert(0);

                    {
                        DirectX::XMFLOAT4X4 model_transform = mesh_instance_node_world_transforms[mesh_instance_index];
                        mesh_instance_data.push_back({model_transform});
                    }

                    mesh_instance_data.back().m_animation_skeleton.init(1, (max_joint_index + 1));

                    DirectX::XMFLOAT4 quaternion;
                    DirectX::XMFLOAT3 translation;
                    DirectX::XMStoreFloat4(&quaternion, DirectX::XMQuaternionIdentity());
                    DirectX::XMStoreFloat3(&translation, DirectX::XMVectorZero());

                    for (size_t joint_index = 0; joint_index < (max_joint_index + 1); ++joint_index)
                    {
                        mesh_instance_data.back().m_animation_skeleton.set_transform(0, joint_index, quaternion, translation);
                    }
                }
            }
        }
    }

    cgltf_free(data);
}

static void asset_import_gltf_mesh(mesh_asset_data *out_mesh_data, int32_t *out_max_joint_index, cgltf_data const *data, cgltf_mesh const *mesh)
{
    // SkinnedMeshInstance::SkinnedMeshInstance
    // create new copy of mesh

    // TLAS -> MeshInstance/SkinnedMeshInstance
    // BLAS -> Mesh // new copy for skinned mesh instance

    // Only one VertexBuffer & IndexBuffer for each GLTF
    // Build BLAS
    // triangles.indexOffset = (mesh->indexOffset + geometry->indexOffsetInMesh) * sizeof(*mesh->buffers->indexData.data())
    // triangles.vertexOffset = (mesh->vertexOffset + geometry->vertexOffsetInMesh) * sizeof(*mesh->buffers->positionData.data()) + mesh->buffers->getVertexBufferRange(engine::VertexBinding::Position).byteOffset
    // triangles.vertexStride = sizeof(*mesh->buffers->positionData.data())

    // SceneGraph::Refresh
    // GeometryInstanceIndex // global index for both TLAS and BLAS
    // GeometryIndex // global index for TLAS

    // ID: unique
    // Index: not unique

    // InstanceData hitInstance = t_InstanceData[hitInstanceID]
    // ---
    // geometryInstanceID = hitInstance.firstGeometryInstanceIndex + hitGeometryIndex // used for lighting etc // for both TLAS and BLAS
    // ---
    // hitGeometryID = hitInstance.firstGeometryIndex + hitGeometryIndex;
    // GeometryData hitGeometryInfo = t_GeometryData[hitGeometryID]
    // ---
    // ByteAddressBuffer hitIndexBuffer = t_BindlessBuffers[NonUniformResourceIndex(hitGeometry.indexBufferID)];
    // ByteAddressBuffer hitVertexBuffer = t_BindlessBuffers[NonUniformResourceIndex(hitGeometry.vertexBufferID)];
    // ---
    // uint3 indices = hitIndexBuffer.Load3(hitGeometry.indexOffset + c_SizeOfTriangleIndices * triangleIndex);
    // float3 vertexPositions[0] = asfloat(hitVertexBuffer.Load3(hitGeometryInfo.positionOffset + c_SizeOfPosition * indices[0]));
    // float3 normals[0] = asfloat(hitVertexBuffer.Load3(hitGeometryInfo.normalOffset + c_SizeOfPosition * indices[0]));
    // ---
    // MaterialConstants hitMeterial = t_MaterialConstants[hitGeometry.MaterialID]
    // ---
    // Texture2D diffuseTexture = t_BindlessTextures[NonUniformResourceIndex(hitMeterial.baseOrDiffuseTextureIndex)]
    // Texture2D specularTexture = t_BindlessTextures[NonUniformResourceIndex(hitMeterial.metalRoughOrSpecularTextureIndex)];
    // ---
    // ["B.3.5. Metal BRDF and Dielectric BRDF" of "glTF 2.0 Specification"](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf)

    (*out_max_joint_index) = -1;

    std::unordered_map<size_t, size_t> subset_material_indices;
    subset_material_indices.reserve(mesh->primitives_count);

    out_mesh_data->m_subsets.reserve(mesh->primitives_count);

    for (size_t primitive_index = 0; primitive_index < mesh->primitives_count; ++primitive_index)
    {
        cgltf_primitive const *primitive = &mesh->primitives[primitive_index];

        if (cgltf_primitive_type_triangles == primitive->type)
        {
            cgltf_accessor const *position_accessor = NULL;
            cgltf_accessor const *normal_accessor = NULL;
            cgltf_accessor const *tangent_accessor = NULL;
            cgltf_accessor const *texcoord_accessor = NULL;
            cgltf_accessor const *joints_accessor = NULL;
            cgltf_accessor const *weights_accessor = NULL;
            {
                cgltf_int position_index = -1;
                cgltf_int normal_index = -1;
                cgltf_int tangent_index = -1;
                cgltf_int texcoord_index = -1;
                cgltf_int joints_index = -1;
                cgltf_int weights_index = -1;

                for (size_t vertex_attribute_index = 0; vertex_attribute_index < primitive->attributes_count; ++vertex_attribute_index)
                {
                    cgltf_attribute const *vertex_attribute = &primitive->attributes[vertex_attribute_index];

                    switch (vertex_attribute->type)
                    {
                    case cgltf_attribute_type_position:
                    {
                        assert(cgltf_attribute_type_position == vertex_attribute->type);

                        if (NULL == position_accessor || vertex_attribute->index < position_index)
                        {
                            position_accessor = vertex_attribute->data;
                            position_index = vertex_attribute->index;
                        }
                    }
                    break;
                    case cgltf_attribute_type_normal:
                    {
                        assert(cgltf_attribute_type_normal == vertex_attribute->type);

                        if (NULL == normal_accessor || vertex_attribute->index < normal_index)
                        {
                            normal_accessor = vertex_attribute->data;
                            normal_index = vertex_attribute->index;
                        }
                    }
                    break;
                    case cgltf_attribute_type_tangent:
                    {
                        assert(cgltf_attribute_type_tangent == vertex_attribute->type);

                        if (NULL == tangent_accessor || vertex_attribute->index < tangent_index)
                        {
                            tangent_accessor = vertex_attribute->data;
                            tangent_index = vertex_attribute->index;
                        }
                    }
                    break;
                    case cgltf_attribute_type_texcoord:
                    {
                        assert(cgltf_attribute_type_texcoord == vertex_attribute->type);

                        if (NULL == texcoord_accessor || vertex_attribute->index < tangent_index)
                        {
                            texcoord_accessor = vertex_attribute->data;
                            texcoord_index = vertex_attribute->index;
                        }
                    }
                    break;
                    case cgltf_attribute_type_joints:
                    {
                        assert(cgltf_attribute_type_joints == vertex_attribute->type);

                        if (NULL == joints_accessor || vertex_attribute->index < tangent_index)
                        {
                            joints_accessor = vertex_attribute->data;
                            joints_index = vertex_attribute->index;
                        }
                    }
                    break;
                    case cgltf_attribute_type_weights:
                    {
                        assert(cgltf_attribute_type_weights == vertex_attribute->type);

                        if (NULL == weights_accessor || vertex_attribute->index < tangent_index)
                        {
                            weights_accessor = vertex_attribute->data;
                            weights_index = vertex_attribute->index;
                        }
                    }
                    break;
                    default:
                    {
                        // Do Nothing
                    }
                    }
                }
            }

            if (NULL != position_accessor)
            {
                subset_asset_data *subset_data;
                uint32_t vertex_index_offset;
                uint32_t index_index_offset;
                {
                    cgltf_material const *const primitive_material = primitive->material;

                    size_t const primitive_material_index = cgltf_material_index(data, primitive_material);

                    auto found = subset_material_indices.find(primitive_material_index);
                    if (subset_material_indices.end() != found)
                    {
                        size_t const subset_data_index = found->second;
                        assert(subset_data_index < out_mesh_data->m_subsets.size());
                        subset_data = &out_mesh_data->m_subsets[subset_data_index];
                        vertex_index_offset = static_cast<uint32_t>(subset_data->m_vertex_position_binding.size());
                        index_index_offset = static_cast<uint32_t>(subset_data->m_indices.size());
                    }
                    else
                    {
                        size_t const subset_data_index = out_mesh_data->m_subsets.size();
                        out_mesh_data->m_subsets.push_back({});
                        subset_material_indices.emplace_hint(found, primitive_material_index, subset_data_index);
                        subset_data = &out_mesh_data->m_subsets.back();
                        vertex_index_offset = 0U;
                        index_index_offset = 0U;

                        if (NULL != primitive_material->normal_texture.texture)
                        {
                            cgltf_image const *const normal_texture_image = primitive_material->normal_texture.texture->image;
                            assert(NULL != normal_texture_image);
                            assert(NULL == normal_texture_image->buffer_view);
                            assert(NULL != normal_texture_image->uri);
                            subset_data->m_normal_texture_image_uri = normal_texture_image->uri;
                            cgltf_decode_uri(&subset_data->m_normal_texture_image_uri[0]);

                            subset_data->m_normal_texture_scale = primitive_material->normal_texture.scale;
                        }
                        else
                        {
                            subset_data->m_normal_texture_scale = 1.0;
                        }

                        subset_data->m_emissive_factor = DirectX::XMFLOAT3(primitive_material->emissive_factor[0], primitive_material->emissive_factor[1], primitive_material->emissive_factor[2]);

                        if (NULL != primitive_material->emissive_texture.texture)
                        {
                            cgltf_image const *const emissive_texture_image = primitive_material->emissive_texture.texture->image;
                            assert(NULL != emissive_texture_image);
                            assert(NULL == emissive_texture_image->buffer_view);
                            assert(NULL != emissive_texture_image->uri);
                            subset_data->m_emissive_texture_image_uri = emissive_texture_image->uri;
                            cgltf_decode_uri(&subset_data->m_emissive_texture_image_uri[0]);
                        }

                        if (primitive_material->has_pbr_metallic_roughness)
                        {
                            subset_data->m_base_color_factor = DirectX::XMFLOAT3(primitive_material->pbr_metallic_roughness.base_color_factor[0], primitive_material->pbr_metallic_roughness.base_color_factor[1], primitive_material->pbr_metallic_roughness.base_color_factor[2]);

                            if (NULL != primitive_material->pbr_metallic_roughness.base_color_texture.texture)
                            {
                                cgltf_image const *const base_color_texture_image = primitive_material->pbr_metallic_roughness.base_color_texture.texture->image;
                                assert(NULL != base_color_texture_image);
                                assert(NULL == base_color_texture_image->buffer_view);
                                assert(NULL != base_color_texture_image->uri);
                                subset_data->m_base_color_texture_image_uri = base_color_texture_image->uri;
                            }

                            subset_data->m_metallic_factor = primitive_material->pbr_metallic_roughness.metallic_factor;

                            subset_data->m_roughness_factor = primitive_material->pbr_metallic_roughness.roughness_factor;

                            if (NULL != primitive_material->pbr_metallic_roughness.metallic_roughness_texture.texture)
                            {
                                cgltf_image const *const metallic_roughness_texture_image = primitive_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image;
                                assert(NULL != metallic_roughness_texture_image);
                                assert(NULL == metallic_roughness_texture_image->buffer_view);
                                assert(NULL != metallic_roughness_texture_image->uri);
                                subset_data->m_metallic_roughness_texture_image_uri = metallic_roughness_texture_image->uri;
                            }
                        }
                    }
                }

                uint32_t const vertex_count = static_cast<uint32_t>(position_accessor->count);

                assert(vertex_index_offset == subset_data->m_vertex_position_binding.size());
                subset_data->m_vertex_position_binding.resize(vertex_index_offset + vertex_count);
                vertex_position_binding *const out_vertex_position_binding = &subset_data->m_vertex_position_binding[vertex_index_offset];

                {
                    uintptr_t position_base = -1;
                    size_t position_stride = -1;
                    {
                        cgltf_buffer_view const *const position_buffer_view = position_accessor->buffer_view;
                        position_base = reinterpret_cast<uintptr_t>(position_buffer_view->buffer->data) + position_buffer_view->offset + position_accessor->offset;
                        position_stride = (0 != position_buffer_view->stride) ? position_buffer_view->stride : position_accessor->stride;
                    }

                    assert(cgltf_type_vec3 == position_accessor->type);
                    assert(cgltf_component_type_r_32f == position_accessor->component_type);

                    for (size_t vertex_index = 0; vertex_index < position_accessor->count; ++vertex_index)
                    {
                        float const *const position_float3 = reinterpret_cast<float const *>(position_base + position_stride * vertex_index);

                        out_vertex_position_binding[vertex_index].m_position.x = position_float3[0];
                        out_vertex_position_binding[vertex_index].m_position.y = position_float3[1];
                        out_vertex_position_binding[vertex_index].m_position.z = position_float3[2];
                    }
                }

                assert(vertex_index_offset == subset_data->m_vertex_varying_binding.size());
                subset_data->m_vertex_varying_binding.resize(vertex_index_offset + vertex_count);
                vertex_varying_binding *const out_vertex_varying_binding = &subset_data->m_vertex_varying_binding[vertex_index_offset];

                if (NULL != normal_accessor)
                {
                    uintptr_t normal_base = -1;
                    size_t normal_stride = -1;
                    {
                        cgltf_buffer_view const *const normal_buffer_view = normal_accessor->buffer_view;
                        normal_base = reinterpret_cast<uintptr_t>(normal_buffer_view->buffer->data) + normal_buffer_view->offset + normal_accessor->offset;
                        normal_stride = (0 != normal_buffer_view->stride) ? normal_buffer_view->stride : normal_accessor->stride;
                    }

                    assert(normal_accessor->count == vertex_count);

                    assert(cgltf_type_vec3 == normal_accessor->type);
                    assert(cgltf_component_type_r_32f == normal_accessor->component_type);

                    for (size_t vertex_index = 0; vertex_index < normal_accessor->count; ++vertex_index)
                    {
                        float const *const normal_float3 = reinterpret_cast<float const *>(normal_base + normal_stride * vertex_index);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
                        DirectX::PackedVector::XMDECN4 packed_vector;
                        DirectX::PackedVector::XMStoreDecN4(&packed_vector, DirectX::XMLoadFloat3(reinterpret_cast<DirectX::XMFLOAT3 const *>(normal_float3)));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
                        out_vertex_varying_binding[vertex_index].m_normal = packed_vector.v;
                    }
                }
                else
                {
                    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        out_vertex_varying_binding[vertex_index].m_normal = 0;
                    }
                }

                if (NULL != tangent_accessor)
                {
                    uintptr_t tangent_base = -1;
                    size_t tangent_stride = -1;
                    {
                        cgltf_buffer_view const *const tangent_buffer_view = tangent_accessor->buffer_view;
                        tangent_base = reinterpret_cast<uintptr_t>(tangent_buffer_view->buffer->data) + tangent_buffer_view->offset + tangent_accessor->offset;
                        tangent_stride = (0 != tangent_buffer_view->stride) ? tangent_buffer_view->stride : tangent_accessor->stride;
                    }

                    assert(tangent_accessor->count == vertex_count);

                    assert(cgltf_type_vec4 == tangent_accessor->type);
                    assert(cgltf_component_type_r_32f == tangent_accessor->component_type);

                    for (size_t vertex_index = 0; vertex_index < tangent_accessor->count; ++vertex_index)
                    {
                        float const *const tangent_float4 = reinterpret_cast<float const *>(tangent_base + tangent_stride * vertex_index);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
                        DirectX::PackedVector::XMDECN4 packed_vector;
                        DirectX::PackedVector::XMStoreDecN4(&packed_vector, DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4 const *>(tangent_float4)));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
                        out_vertex_varying_binding[vertex_index].m_tangent = packed_vector.v;
                    }
                }
                else
                {
                    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        out_vertex_varying_binding[vertex_index].m_tangent = 0;
                    }
                }

                if (NULL != texcoord_accessor)
                {
                    uintptr_t texcoord_base = -1;
                    size_t texcoord_stride = -1;
                    {
                        cgltf_buffer_view const *const texcoord_buffer_view = texcoord_accessor->buffer_view;
                        texcoord_base = reinterpret_cast<uintptr_t>(texcoord_buffer_view->buffer->data) + texcoord_buffer_view->offset + texcoord_accessor->offset;
                        texcoord_stride = (0 != texcoord_buffer_view->stride) ? texcoord_buffer_view->stride : texcoord_accessor->stride;
                    }

                    assert(texcoord_accessor->count == vertex_count);

                    assert(cgltf_type_vec2 == texcoord_accessor->type);

                    switch (texcoord_accessor->component_type)
                    {
                    case cgltf_component_type_r_8u:
                    {
                        assert(cgltf_component_type_r_8u == texcoord_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < texcoord_accessor->count; ++vertex_index)
                        {
                            uint8_t const *const texcoord_ubyte2 = reinterpret_cast<uint8_t const *>(texcoord_base + texcoord_stride * vertex_index);

                            DirectX::PackedVector::XMUBYTEN2 packed_vector_ubyten2;
                            packed_vector_ubyten2.x = texcoord_ubyte2[0];
                            packed_vector_ubyten2.y = texcoord_ubyte2[1];

                            DirectX::XMVECTOR unpacked_vector = DirectX::PackedVector::XMLoadUByteN2(&packed_vector_ubyten2);

                            DirectX::PackedVector::XMUSHORTN2 texcoord_ushortn2;
                            DirectX::PackedVector::XMStoreUShortN2(&texcoord_ushortn2, unpacked_vector);

                            out_vertex_varying_binding[vertex_index].m_texcoord = texcoord_ushortn2.v;
                        }
                    }
                    break;
                    case cgltf_component_type_r_16u:
                    {
                        assert(cgltf_component_type_r_16u == texcoord_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < texcoord_accessor->count; ++vertex_index)
                        {
                            uint16_t const *const texcoord_ushortn2 = reinterpret_cast<uint16_t const *>(texcoord_base + texcoord_stride * vertex_index);

                            DirectX::PackedVector::XMUSHORTN2 packed_vector;
                            packed_vector.x = texcoord_ushortn2[0];
                            packed_vector.y = texcoord_ushortn2[1];

                            out_vertex_varying_binding[vertex_index].m_texcoord = packed_vector.v;
                        }
                    }
                    break;
                    case cgltf_component_type_r_32f:
                    {
                        assert(cgltf_component_type_r_32f == texcoord_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < texcoord_accessor->count; ++vertex_index)
                        {
                            float const *const texcoord_float2 = reinterpret_cast<float const *>(texcoord_base + texcoord_stride * vertex_index);

                            DirectX::XMFLOAT2 unpacked_vector;
                            unpacked_vector.x = texcoord_float2[0];
                            unpacked_vector.y = texcoord_float2[1];

                            DirectX::PackedVector::XMUSHORTN2 packed_vector;
                            DirectX::PackedVector::XMStoreUShortN2(&packed_vector, DirectX::XMLoadFloat2(&unpacked_vector));

                            out_vertex_varying_binding[vertex_index].m_texcoord = packed_vector.v;
                        }
                    }
                    break;
                    default:
                        assert(0);
                    }
                }
                else
                {
                    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        out_vertex_varying_binding[vertex_index].m_texcoord = 0;
                    }
                }

                assert(vertex_index_offset == subset_data->m_vertex_skinned_binding.size());
                subset_data->m_vertex_skinned_binding.resize(vertex_index_offset + vertex_count);
                vertex_skinned_binding *const out_vertex_skinned_binding = &subset_data->m_vertex_skinned_binding[vertex_index_offset];

                if (NULL != joints_accessor)
                {
                    uintptr_t joints_base = -1;
                    size_t joints_stride = -1;
                    {
                        cgltf_buffer_view const *const joint_indices_buffer_view = joints_accessor->buffer_view;
                        joints_base = reinterpret_cast<uintptr_t>(joint_indices_buffer_view->buffer->data) + joint_indices_buffer_view->offset + joints_accessor->offset;
                        joints_stride = (0 != joint_indices_buffer_view->stride) ? joint_indices_buffer_view->stride : joints_accessor->stride;
                    }

                    assert(joints_accessor->count == vertex_count);

                    assert(cgltf_type_vec4 == joints_accessor->type);

                    switch (joints_accessor->component_type)
                    {
                    case cgltf_component_type_r_8u:
                    {
                        assert(cgltf_component_type_r_8u == joints_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < joints_accessor->count; ++vertex_index)
                        {
                            uint8_t const *const joint_indices_ubyte4 = reinterpret_cast<uint8_t const *>(joints_base + joints_stride * vertex_index);

                            DirectX::PackedVector::XMUSHORT4 packed_vector;
                            packed_vector.x = joint_indices_ubyte4[0];
                            packed_vector.y = joint_indices_ubyte4[1];
                            packed_vector.z = joint_indices_ubyte4[2];
                            packed_vector.w = joint_indices_ubyte4[3];

                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ubyte4[0]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ubyte4[1]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ubyte4[2]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ubyte4[3]));

                            out_vertex_skinned_binding[vertex_index].m_indices = packed_vector.v;
                        }
                    }
                    break;
                    case cgltf_component_type_r_16u:
                    {
                        assert(cgltf_component_type_r_16u == joints_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < joints_accessor->count; ++vertex_index)
                        {
                            uint16_t const *const joint_indices_ushort4 = reinterpret_cast<uint16_t const *>(joints_base + joints_stride * vertex_index);

                            DirectX::PackedVector::XMUSHORT4 packed_vector;
                            packed_vector.x = joint_indices_ushort4[0];
                            packed_vector.y = joint_indices_ushort4[1];
                            packed_vector.z = joint_indices_ushort4[2];
                            packed_vector.w = joint_indices_ushort4[3];

                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ushort4[0]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ushort4[1]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ushort4[2]));
                            (*out_max_joint_index) = std::max((*out_max_joint_index), static_cast<int32_t>(joint_indices_ushort4[3]));

                            out_vertex_skinned_binding[vertex_index].m_indices = packed_vector.v;
                        }
                    }
                    break;
                    default:
                        assert(0);
                    }
                }
                else
                {
                    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        out_vertex_skinned_binding[vertex_index].m_indices = 0;
                    }
                }

                if (NULL != weights_accessor)
                {
                    uintptr_t weights_base = -1;
                    size_t weights_stride = -1;
                    {
                        cgltf_buffer_view const *const joint_weights_buffer_view = weights_accessor->buffer_view;
                        weights_base = reinterpret_cast<uintptr_t>(joint_weights_buffer_view->buffer->data) + joint_weights_buffer_view->offset + weights_accessor->offset;
                        weights_stride = (0 != joint_weights_buffer_view->stride) ? joint_weights_buffer_view->stride : weights_accessor->stride;
                    }

                    assert(weights_accessor->count == vertex_count);

                    assert(cgltf_type_vec4 == weights_accessor->type);

                    switch (weights_accessor->component_type)
                    {
                    case cgltf_component_type_r_8u:
                    {
                        assert(cgltf_component_type_r_8u == weights_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < weights_accessor->count; ++vertex_index)
                        {
                            uint8_t const *const joint_weights_ubyte4 = reinterpret_cast<uint8_t const *>(weights_base + weights_stride * vertex_index);

                            DirectX::PackedVector::XMUBYTEN4 packed_vector_ubyten4;
                            packed_vector_ubyten4.x = joint_weights_ubyte4[0];
                            packed_vector_ubyten4.y = joint_weights_ubyte4[1];
                            packed_vector_ubyten4.z = joint_weights_ubyte4[2];
                            packed_vector_ubyten4.w = joint_weights_ubyte4[3];

                            DirectX::XMVECTOR unpacked_vector = DirectX::PackedVector::XMLoadUByteN4(&packed_vector_ubyten4);

                            DirectX::PackedVector::XMUSHORTN4 joint_weights_ushortn4;
                            DirectX::PackedVector::XMStoreUShortN4(&joint_weights_ushortn4, unpacked_vector);

                            out_vertex_skinned_binding[vertex_index].m_weights = joint_weights_ushortn4.v;
                        }
                    }
                    break;
                    case cgltf_component_type_r_16u:
                    {
                        assert(cgltf_component_type_r_16u == weights_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < weights_accessor->count; ++vertex_index)
                        {
                            uint16_t const *const joint_weights_ushortn4 = reinterpret_cast<uint16_t const *>(weights_base + weights_stride * vertex_index);

                            DirectX::PackedVector::XMUSHORTN4 packed_vector;
                            packed_vector.x = joint_weights_ushortn4[0];
                            packed_vector.y = joint_weights_ushortn4[1];
                            packed_vector.z = joint_weights_ushortn4[2];
                            packed_vector.w = joint_weights_ushortn4[3];

                            out_vertex_skinned_binding[vertex_index].m_weights = packed_vector.v;
                        }
                    }
                    break;
                    case cgltf_component_type_r_32f:
                    {
                        assert(cgltf_component_type_r_32f == weights_accessor->component_type);

                        for (size_t vertex_index = 0; vertex_index < weights_accessor->count; ++vertex_index)
                        {
                            float const *const joint_weights_float4 = reinterpret_cast<float const *>(weights_base + weights_stride * vertex_index);

                            DirectX::XMFLOAT4 unpacked_vector;
                            unpacked_vector.x = joint_weights_float4[0];
                            unpacked_vector.y = joint_weights_float4[1];
                            unpacked_vector.z = joint_weights_float4[2];
                            unpacked_vector.w = joint_weights_float4[3];

                            DirectX::PackedVector::XMUSHORTN4 packed_vector;
                            DirectX::PackedVector::XMStoreUShortN4(&packed_vector, DirectX::XMLoadFloat4(&unpacked_vector));

                            out_vertex_skinned_binding[vertex_index].m_weights = packed_vector.v;
                        }
                    }
                    break;
                    default:
                        assert(0);
                    }
                }
                else
                {
                    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        out_vertex_skinned_binding[vertex_index].m_weights = 0;
                    }
                }

                // TODO: support strip and fan

                // The vertex buffer for each primitive is independent
                // Index for each primitive is from zero

                cgltf_accessor const *const index_accessor = primitive->indices;
                if (NULL != index_accessor)
                {
                    uint32_t const index_count = static_cast<uint32_t>(index_accessor->count);

                    assert(index_index_offset == subset_data->m_indices.size());
                    subset_data->m_indices.resize(index_index_offset + index_count);
                    uint32_t *const out_indices = &subset_data->m_indices[index_index_offset];

                    uintptr_t index_base = -1;
                    size_t index_stride = -1;
                    {
                        cgltf_buffer_view const *const index_buffer_view = index_accessor->buffer_view;
                        index_base = reinterpret_cast<uintptr_t>(index_buffer_view->buffer->data) + index_buffer_view->offset + index_accessor->offset;
                        index_stride = (0 != index_buffer_view->stride) ? index_buffer_view->stride : index_accessor->stride;
                    }

                    assert(cgltf_type_scalar == index_accessor->type);

                    switch (index_accessor->component_type)
                    {
                    case cgltf_component_type_r_8u:
                    {
                        assert(cgltf_component_type_r_8u == index_accessor->component_type);

                        for (size_t index_index = 0; index_index < index_accessor->count; ++index_index)
                        {
                            uint8_t const *const index_ubyte = reinterpret_cast<uint8_t const *>(index_base + index_stride * index_index);

                            out_indices[index_index] = (vertex_index_offset + static_cast<uint32_t>(*index_ubyte));
                        }
                    }
                    break;
                    case cgltf_component_type_r_16u:
                    {
                        assert(cgltf_component_type_r_16u == index_accessor->component_type);

                        for (size_t index_index = 0; index_index < index_accessor->count; ++index_index)
                        {
                            uint16_t const *const index_ushort = reinterpret_cast<uint16_t const *>(index_base + index_stride * index_index);

                            out_indices[index_index] = (vertex_index_offset + static_cast<uint32_t>(*index_ushort));
                        }
                    }
                    break;
                    case cgltf_component_type_r_32u:
                    {
                        assert(cgltf_component_type_r_32u == index_accessor->component_type);

                        for (size_t index_index = 0; index_index < index_accessor->count; ++index_index)
                        {
                            uint32_t const *const index_uint = reinterpret_cast<uint32_t const *>(index_base + index_stride * index_index);

                            out_indices[index_index] = (vertex_index_offset + static_cast<uint32_t>(*index_uint));
                        }
                    }
                    break;
                    default:
                        assert(0);
                    }
                }
                else
                {
                    uint32_t const index_count = vertex_count;

                    assert(index_index_offset == subset_data->m_indices.size());
                    subset_data->m_indices.resize(index_index_offset + index_count);
                    uint32_t *const out_indices = &subset_data->m_indices[index_index_offset];

                    for (size_t index_index = 0; index_count; ++index_index)
                    {
                        out_indices[index_index] = (vertex_index_offset + static_cast<uint32_t>(index_index));
                    }
                }
            }
        }
    }

    if ((*out_max_joint_index) < 0)
    {
        for (subset_asset_data &subset : out_mesh_data->m_subsets)
        {
            std::vector<vertex_skinned_binding> temp_empty;
            temp_empty.swap(subset.m_vertex_skinned_binding);

            assert(subset.m_vertex_skinned_binding.empty());
        }

        out_mesh_data->m_skinned = false;
    }
    else
    {
        out_mesh_data->m_skinned = true;
    }
}

static void asset_import_gltf_mesh_instances(std::vector<cgltf_node const *> &out_mesh_instance_nodes, std::vector<DirectX::XMFLOAT4X4> &out_mesh_instance_node_world_transforms, cgltf_data const *data, cgltf_mesh const *mesh)
{
    std::vector<DirectX::XMFLOAT4X4> node_local_transforms(static_cast<size_t>(data->nodes_count));

    for (size_t node_index = 0; node_index < data->nodes_count; ++node_index)
    {
        cgltf_node const *node = &data->nodes[node_index];

        if (node->has_matrix)
        {
            node_local_transforms[node_index] = (*reinterpret_cast<DirectX::XMFLOAT4X4 const *>(&node->matrix));
        }
        else
        {
            DirectX::XMVECTOR node_local_scale;
            if (node->has_scale)
            {
                node_local_scale = DirectX::XMLoadFloat3(reinterpret_cast<DirectX::XMFLOAT3 const *>(&node->scale));
            }
            else
            {
                node_local_scale = DirectX::XMVectorSplatOne();
            }

            DirectX::XMVECTOR node_local_rotation;
            if (node->has_rotation)
            {
                node_local_rotation = DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4 const *>(&node->rotation));
            }
            else
            {
                node_local_rotation = DirectX::XMQuaternionIdentity();
            }

            DirectX::XMVECTOR node_local_translation;
            if (node->has_translation)
            {
                node_local_translation = DirectX::XMLoadFloat3(reinterpret_cast<DirectX::XMFLOAT3 const *>(&node->translation));
            }
            else
            {
                node_local_translation = DirectX::XMVectorZero();
            }

            DirectX::XMStoreFloat4x4(&node_local_transforms[node_index], DirectX::XMMatrixMultiply(DirectX::XMMatrixScalingFromVector(node_local_scale), DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationQuaternion(node_local_rotation), DirectX::XMMatrixTranslationFromVector(node_local_translation))));
        }
    }

    std::vector<DirectX::XMFLOAT4X4> node_world_transforms(static_cast<size_t>(data->nodes_count));

    struct hierarchy_propagate_transform
    {
        size_t parent_node_index;
        size_t node_index;
    };

    std::vector<hierarchy_propagate_transform> hierarchy_propagate_transform_stack;
    for (size_t scene_node_index = data->scene->nodes_count; scene_node_index > 0; --scene_node_index)
    {
        hierarchy_propagate_transform_stack.push_back({static_cast<size_t>(-1), cgltf_node_index(data, data->scene->nodes[scene_node_index - 1])});
    }

    while (!hierarchy_propagate_transform_stack.empty())
    {
        hierarchy_propagate_transform current = hierarchy_propagate_transform_stack.back();
        hierarchy_propagate_transform_stack.pop_back();

        DirectX::XMMATRIX local_transform = DirectX::XMLoadFloat4x4(&node_local_transforms[current.node_index]);

        DirectX::XMMATRIX parent_world_transform = (static_cast<size_t>(-1) != current.parent_node_index) ? DirectX::XMLoadFloat4x4(&node_world_transforms[current.parent_node_index]) : DirectX::XMMatrixIdentity();

        DirectX::XMMATRIX world_transform = DirectX::XMMatrixMultiply(local_transform, parent_world_transform);

        DirectX::XMStoreFloat4x4(&node_world_transforms[current.node_index], world_transform);

        if (data->nodes[current.node_index].mesh == mesh)
        {
            out_mesh_instance_nodes.push_back(&data->nodes[current.node_index]);
            out_mesh_instance_node_world_transforms.push_back(node_world_transforms[current.node_index]);
        }

        for (size_t child_node_index = data->nodes[current.node_index].children_count; child_node_index > 0; --child_node_index)
        {
            hierarchy_propagate_transform_stack.push_back({current.node_index, cgltf_node_index(data, data->nodes[current.node_index].children[child_node_index - 1])});
        }
    }
}

static void asset_import_gltf_animation(animated_skeleton *out_animated_skeleton, float frame_rate, cgltf_data const *data, cgltf_skin const *skin, cgltf_animation const *animation)
{
    // Skin
    // https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/Renderer/shaders/animation.glsl
    // defined(HAS_WEIGHTS_0_VEC4) && defined(HAS_JOINTS_0_VEC4)
    // defined(HAS_WEIGHTS_1_VEC4) && defined(HAS_JOINTS_1_VEC4)
    size_t const joint_count = skin->joints_count;

    // Due to the hierarchy, we can NOT know which nodes are really influencing the skeleton joints. We have to consider all nodes.
    std::vector<DirectX::XMFLOAT3> node_local_scales(static_cast<size_t>(data->nodes_count));
    std::vector<DirectX::XMFLOAT4> node_local_rotations(static_cast<size_t>(data->nodes_count));
    std::vector<DirectX::XMFLOAT3> node_local_translations(static_cast<size_t>(data->nodes_count));

    for (size_t node_index = 0; node_index < data->nodes_count; ++node_index)
    {
        cgltf_node const *node = &data->nodes[node_index];

        if (node->has_matrix)
        {
            DirectX::XMVECTOR out_node_local_scale;
            DirectX::XMVECTOR out_node_local_rotation;
            DirectX::XMVECTOR out_node_local_translation;
            DirectX::XMMatrixDecompose(&out_node_local_scale, &out_node_local_rotation, &out_node_local_translation, DirectX::XMLoadFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4 const *>(&node->matrix)));

            DirectX::XMStoreFloat3(&node_local_scales[node_index], out_node_local_scale);
            DirectX::XMStoreFloat4(&node_local_rotations[node_index], out_node_local_rotation);
            DirectX::XMStoreFloat3(&node_local_translations[node_index], out_node_local_translation);
        }
        else
        {
            if (node->has_scale)
            {
                node_local_scales[node_index].x = node->scale[0];
                node_local_scales[node_index].y = node->scale[1];
                node_local_scales[node_index].z = node->scale[2];
            }
            else
            {
                DirectX::XMStoreFloat3(&node_local_scales[node_index], DirectX::XMVectorSplatOne());
            }

            if (node->has_rotation)
            {
                node_local_rotations[node_index].x = node->rotation[0];
                node_local_rotations[node_index].y = node->rotation[1];
                node_local_rotations[node_index].z = node->rotation[2];
                node_local_rotations[node_index].w = node->rotation[3];
            }
            else
            {
                DirectX::XMStoreFloat4(&node_local_rotations[node_index], DirectX::XMQuaternionIdentity());
            }

            if (node->has_translation)
            {
                node_local_translations[node_index].x = node->translation[0];
                node_local_translations[node_index].y = node->translation[1];
                node_local_translations[node_index].z = node->translation[2];
            }
            else
            {
                DirectX::XMStoreFloat3(&node_local_translations[node_index], DirectX::XMVectorZero());
            }
        }
    }

    // Animation
    // https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/gltf/animation.js
    // gltfAnimation.advance
    // https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/gltf/scene.js
    // applyTransformHierarchy
    // https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/gltf/skin.js
    // gltfSkin.computeJoints
    size_t const channel_count = (NULL != animation) ? animation->channels_count : 0;

    float animation_max_time = -1.0;
    std::vector<float> channel_min_key_times(channel_count, -1.0F);
    std::vector<float> channel_max_key_times(channel_count, -1.0F);
    for (size_t channel_index = 0; channel_index < channel_count; ++channel_index)
    {
        float channel_min_key_time = -1.0;
        float channel_max_key_time = -1.0;
        {
            cgltf_accessor const *channel_time_accessor = animation->channels[channel_index].sampler->input;

            assert(cgltf_type_scalar == channel_time_accessor->type);
            assert(cgltf_component_type_r_32f == channel_time_accessor->component_type);
            assert(sizeof(float) == channel_time_accessor->stride);

            cgltf_bool result_accessor_read_float_min = cgltf_accessor_read_float(channel_time_accessor, 0, &channel_min_key_time, 1);
            assert(result_accessor_read_float_min);

            cgltf_bool result_accessor_read_float_max = cgltf_accessor_read_float(channel_time_accessor, channel_time_accessor->count - 1, &channel_max_key_time, 1);
            assert(result_accessor_read_float_max);
        }

        channel_min_key_times[channel_index] = channel_min_key_time;
        channel_max_key_times[channel_index] = channel_max_key_time;
        animation_max_time = std::max(channel_max_key_time, animation_max_time);
    }

    size_t const frame_count = (NULL != animation) ? static_cast<size_t>(frame_rate * animation_max_time) : 1;

    out_animated_skeleton->init(frame_count, joint_count);

    std::vector<float> channel_previous_sample_times(channel_count, 0.0F);
    std::vector<size_t> channel_previous_key_indices(channel_count, 0);
    for (size_t frame_index = 0; frame_index < frame_count; ++frame_index)
    {
        if (NULL != animation)
        {
            float const frame_sample_time = static_cast<float>((0.5 / static_cast<double>(frame_rate)) + (1.0 / static_cast<double>(frame_rate)) * static_cast<double>(frame_index));
            assert(frame_sample_time < animation_max_time);

            for (size_t channel_index = 0; channel_index < channel_count; ++channel_index)
            {
                cgltf_animation_channel const *channel = &animation->channels[channel_index];

                cgltf_accessor const *channel_time_accessor = channel->sampler->input;

                assert(cgltf_type_scalar == channel_time_accessor->type);
                assert(cgltf_component_type_r_32f == channel_time_accessor->component_type);
                assert(sizeof(float) == channel_time_accessor->stride);

                size_t channel_key_count = channel_time_accessor->count;
                assert(channel_key_count >= 1);

                float const channel_sample_time = std::min(std::max(frame_sample_time, channel_min_key_times[channel_index]), channel_max_key_times[channel_index]);

                if (channel_previous_sample_times[channel_index] > channel_sample_time)
                {
                    channel_previous_key_indices[channel_index] = 0;
                }

                channel_previous_sample_times[channel_index] = channel_sample_time;

                float channel_previous_key_time = -1.0;
                float channel_next_key_time = channel_max_key_times[channel_index];
                size_t next_key_index = -1;
                for (size_t channel_key_index = channel_previous_key_indices[channel_index]; channel_key_index < channel_key_count; ++channel_key_index)
                {
                    float channel_key_time = -1.0;
                    {
                        cgltf_bool result_accessor_read_float = cgltf_accessor_read_float(channel_time_accessor, channel_key_index, &channel_key_time, 1);
                        assert(result_accessor_read_float);
                    }

                    if (channel_sample_time <= channel_key_time)
                    {
                        assert(channel_previous_key_time >= 0.0);
                        next_key_index = std::min(std::max(static_cast<size_t>(1), channel_key_index), channel_key_count - 1);
                        channel_previous_key_indices[channel_index] = (next_key_index > 1) ? (next_key_index - 1) : 0;
                        break;
                    }

                    channel_previous_key_time = channel_key_time;
                }

                float const channel_delta_key_time = channel_next_key_time - channel_previous_key_time;
                assert(channel_delta_key_time >= 0.0);

                float const channel_time_normalized = (channel_next_key_time > channel_previous_key_time && channel_delta_key_time > 0.0) ? ((channel_sample_time - channel_previous_key_time) / channel_delta_key_time) : static_cast<float>(0.0);

                cgltf_accessor const *channel_animated_property_accessor = channel->sampler->output;

                if (NULL != channel->target_node)
                {
                    size_t const target_node_index = cgltf_node_index(data, channel->target_node);

                    switch (channel->target_path)
                    {
                    case cgltf_animation_path_type_scale:
                    {
                        assert(cgltf_animation_path_type_scale == channel->target_path);

                        assert(cgltf_type_vec3 == channel_animated_property_accessor->type);
                        assert(cgltf_component_type_r_32f == channel_animated_property_accessor->component_type);
                        assert((sizeof(float) * 3) == channel_animated_property_accessor->stride);

                        switch (channel->sampler->interpolation)
                        {
                        case cgltf_interpolation_type_step:
                        case cgltf_interpolation_type_linear:
                        {
                            // NOTE: We promote STEP to LINEAR.
                            assert(cgltf_interpolation_type_step == channel->sampler->interpolation || cgltf_interpolation_type_linear == channel->sampler->interpolation);

                            DirectX::XMFLOAT3 channel_previous_key_scale;
                            DirectX::XMFLOAT3 channel_next_key_scale;
                            {
                                cgltf_bool result_accessor_read_float_previous_key = cgltf_accessor_read_float(channel_animated_property_accessor, channel_previous_key_indices[channel_index], reinterpret_cast<float *>(&channel_previous_key_scale), 3);
                                assert(result_accessor_read_float_previous_key);

                                cgltf_bool result_accessor_read_float_next_key = cgltf_accessor_read_float(channel_animated_property_accessor, next_key_index, reinterpret_cast<float *>(&channel_next_key_scale), 3);
                                assert(result_accessor_read_float_next_key);
                            }

                            DirectX::XMStoreFloat3(&node_local_scales[target_node_index], DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&channel_previous_key_scale), DirectX::XMLoadFloat3(&channel_next_key_scale), channel_time_normalized));
                        }
                        break;
                        case cgltf_interpolation_type_cubic_spline:
                        default:
                        {
                            assert(0);
                        }
                        }
                    }
                    break;
                    case cgltf_animation_path_type_rotation:
                    {
                        assert(cgltf_animation_path_type_rotation == channel->target_path);

                        assert(cgltf_type_vec4 == channel_animated_property_accessor->type);
                        assert(cgltf_component_type_r_32f == channel_animated_property_accessor->component_type);
                        assert((sizeof(float) * 4) == channel_animated_property_accessor->stride);

                        switch (channel->sampler->interpolation)
                        {
                        case cgltf_interpolation_type_step:
                        case cgltf_interpolation_type_linear:
                        {
                            // NOTE: We promote STEP to LINEAR.
                            assert(cgltf_interpolation_type_step == channel->sampler->interpolation || cgltf_interpolation_type_linear == channel->sampler->interpolation);

                            DirectX::XMFLOAT4 channel_previous_key_rotation;
                            DirectX::XMFLOAT4 channel_next_key_rotation;
                            {
                                cgltf_bool result_accessor_read_float_previous_key = cgltf_accessor_read_float(channel_animated_property_accessor, channel_previous_key_indices[channel_index], reinterpret_cast<float *>(&channel_previous_key_rotation), 4);
                                assert(result_accessor_read_float_previous_key);

                                cgltf_bool result_accessor_read_float_next_key = cgltf_accessor_read_float(channel_animated_property_accessor, next_key_index, reinterpret_cast<float *>(&channel_next_key_rotation), 4);
                                assert(result_accessor_read_float_next_key);
                            }

                            DirectX::XMStoreFloat4(&node_local_rotations[target_node_index], DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&channel_previous_key_rotation), DirectX::XMLoadFloat4(&channel_next_key_rotation), channel_time_normalized));
                        }
                        break;
                        case cgltf_interpolation_type_cubic_spline:
                        default:
                        {
                            assert(0);
                        }
                        }
                    }
                    break;
                    case cgltf_animation_path_type_translation:
                    {
                        assert(cgltf_animation_path_type_translation == channel->target_path);

                        assert(cgltf_type_vec3 == channel_animated_property_accessor->type);
                        assert(cgltf_component_type_r_32f == channel_animated_property_accessor->component_type);
                        assert((sizeof(float) * 3) == channel_animated_property_accessor->stride);

                        switch (channel->sampler->interpolation)
                        {
                        case cgltf_interpolation_type_step:
                        case cgltf_interpolation_type_linear:
                        {
                            // NOTE: We promote STEP to LINEAR.
                            assert(cgltf_interpolation_type_step == channel->sampler->interpolation || cgltf_interpolation_type_linear == channel->sampler->interpolation);

                            DirectX::XMFLOAT3 channel_previous_key_translation;
                            DirectX::XMFLOAT3 channel_next_key_translation;
                            {
                                cgltf_bool result_accessor_read_float_previous_key = cgltf_accessor_read_float(channel_animated_property_accessor, channel_previous_key_indices[channel_index], reinterpret_cast<float *>(&channel_previous_key_translation), 3);
                                assert(result_accessor_read_float_previous_key);

                                cgltf_bool result_accessor_read_float_next_key = cgltf_accessor_read_float(channel_animated_property_accessor, next_key_index, reinterpret_cast<float *>(&channel_next_key_translation), 3);
                                assert(result_accessor_read_float_next_key);
                            }

                            DirectX::XMStoreFloat3(&node_local_translations[target_node_index], DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&channel_previous_key_translation), DirectX::XMLoadFloat3(&channel_next_key_translation), channel_time_normalized));
                        }
                        break;
                        case cgltf_interpolation_type_cubic_spline:
                        default:
                        {
                            assert(0);
                        }
                        }
                    }
                    break;
                    case cgltf_animation_path_type_weights:
                    default:
                    {
                        assert(0);
                    }
                    }
                }
                else
                {
                    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations
                    // When **node** isn��t defined, channel **SHOULD** be ignored.
                    assert(0);
                }
            }
        }

        std::vector<DirectX::XMFLOAT4X4> node_world_transforms(static_cast<size_t>(data->nodes_count));

        struct hierarchy_propagate_transform
        {
            size_t parent_node_index;
            size_t node_index;
        };

        std::vector<hierarchy_propagate_transform> hierarchy_propagate_transform_stack;
        for (size_t scene_node_index = data->scene->nodes_count; scene_node_index > 0; --scene_node_index)
        {
            hierarchy_propagate_transform_stack.push_back({static_cast<size_t>(-1), cgltf_node_index(data, data->scene->nodes[scene_node_index - 1])});
        }

        while (!hierarchy_propagate_transform_stack.empty())
        {
            hierarchy_propagate_transform current = hierarchy_propagate_transform_stack.back();
            hierarchy_propagate_transform_stack.pop_back();

            DirectX::XMMATRIX local_transform = DirectX::XMMatrixMultiply(DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&node_local_scales[current.node_index])), DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node_local_rotations[current.node_index])), DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&node_local_translations[current.node_index]))));

            DirectX::XMMATRIX parent_world_transform = (static_cast<size_t>(-1) != current.parent_node_index) ? DirectX::XMLoadFloat4x4(&node_world_transforms[current.parent_node_index]) : DirectX::XMMatrixIdentity();

            DirectX::XMMATRIX world_transform = DirectX::XMMatrixMultiply(local_transform, parent_world_transform);

            DirectX::XMStoreFloat4x4(&node_world_transforms[current.node_index], world_transform);

            for (size_t child_node_index = data->nodes[current.node_index].children_count; child_node_index > 0; --child_node_index)
            {
                hierarchy_propagate_transform_stack.push_back({current.node_index, cgltf_node_index(data, data->nodes[current.node_index].children[child_node_index - 1])});
            }
        }

        cgltf_accessor const *skin_inverse_bind_matrix_accessor = skin->inverse_bind_matrices;

        assert(cgltf_type_mat4 == skin_inverse_bind_matrix_accessor->type);
        assert(cgltf_component_type_r_32f == skin_inverse_bind_matrix_accessor->component_type);
        assert((sizeof(float) * 16) == skin_inverse_bind_matrix_accessor->stride);

        for (size_t joint_index = 0; joint_index < joint_count; ++joint_index)
        {
            size_t const joint_node_index = cgltf_node_index(data, skin->joints[joint_index]);

            DirectX::XMFLOAT4X4 inverse_bind_matrix;
            {
                cgltf_bool result_accessor_read_float_inverse_bind_matrix = cgltf_accessor_read_float(skin_inverse_bind_matrix_accessor, joint_index, reinterpret_cast<float *>(&inverse_bind_matrix), 16);
                assert(result_accessor_read_float_inverse_bind_matrix);
            }

            DirectX::XMMATRIX joint_transform = DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&inverse_bind_matrix), DirectX::XMLoadFloat4x4(&node_world_transforms[joint_node_index]));

            DirectX::XMVECTOR out_joint_scale;
            DirectX::XMVECTOR out_joint_rotation;
            DirectX::XMVECTOR out_joint_translation;
            DirectX::XMMatrixDecompose(&out_joint_scale, &out_joint_rotation, &out_joint_translation, joint_transform);

            // FLT_EPSILON
            constexpr float const scale_epsilon = 5E-5F;
            assert(DirectX::XMVector3EqualInt(DirectX::XMVectorTrueInt(), DirectX::XMVectorLess(DirectX::XMVectorAbs(DirectX::XMVectorSubtract(out_joint_scale, DirectX::XMVectorSplatOne())), DirectX::XMVectorReplicate(scale_epsilon))));

            DirectX::XMFLOAT4 joint_rotation;
            DirectX::XMFLOAT3 joint_translation;
            DirectX::XMStoreFloat4(&joint_rotation, out_joint_rotation);
            DirectX::XMStoreFloat3(&joint_translation, out_joint_translation);

            out_animated_skeleton->set_transform(frame_index, joint_index, joint_rotation, joint_translation);
        }
    }
}
