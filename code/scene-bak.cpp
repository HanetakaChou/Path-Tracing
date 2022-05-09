#include "demo.h"

#include "../third-party/The-Forge/Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../third-party/The-Forge/Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include <DirectXPackedVector.h>

#include "../shaders/light_injection_shared.h"

#include "scene.h"

void demo_app::init_scene()
{
    scene scene_instance;

    DirectX::XMFLOAT4X4 root_transform;
    DirectX::XMStoreFloat4x4(&root_transform, DirectX::XMMatrixIdentity());
    scene_instance.import_gltf(root_transform, "sparkle.gltf");

    // Mesh

    // https://github.com/NVIDIAGameWorks/donut/blob/main/src/engine/GltfImporter.cpp
    // https://github.com/NVIDIAGameWorks/donut/blob/main/src/engine/TextureCache.cpp
    // https://github.com/NVIDIAGameWorks/donut/blob/main/src/engine/Scene.cpp
    // GltfImporter::Load
    //     -> SceneGraph::m_Meshes
    //     -> SceneGraph::m_Materials
    //     -> SceneGraph::m_MeshInstances
    //     TextureCache::LoadTextureFromFileDeferred(Async)
    //         -> TextureCache::m_TexturesToFinalize
    //     TextureCache::FinalizeTexture
    //         <- TextureCache::m_TexturesToFinalize
    //         -> TextureCache::m_DescriptorTable
    //
    // Scene::CreateMeshBuffers
    //     <- SceneGraph::m_Meshes::buffers
    //     -> Scene::m_DescriptorTable
    //
    // Scene::UpdateMaterial
    //     <- SceneGraph::m_Materials
    //
    // Scene::UpdateGeometry
    //     <- SceneGraph::m_Meshes::geometries
    //
    // Scene::UpdateInstance
    //     <- SceneGraph::m_MeshInstances
    //
    // https://github.com/NVIDIAGameWorks/RTX-Path-Tracing/blob/main/RTXPT/PathTracerBridgeDonut.hlsli
    // getGeometryFromHit
    //     t_BindlessTextures <- Scene::m_DescriptorTable
    // sampleGeometryMaterial
    //     t_BindlessBuffers  <- TextureCache::m_DescriptorTable

    // Skeleton
    //
    // https://github.com/NVIDIAGameWorks/donut/blob/main/src/engine/GltfImporter.cpp
    // GltfImporter::Load
    //     -> SceneGraph::m_SkinnedMeshInstances
    //
    // Scene::CreateMeshBuffers
    //     -> SkinnedMeshInstance::skinningBindingSet
    //
    // Scene::UpdateSkinnedMeshes
    //     <- jointNode::GetLocalToWorldTransform
    //     -> SkinnedMeshInstance::jointBuffer
    //
    // https://github.com/NVIDIAGameWorks/RTX-Path-Tracing/blob/main/donut/shaders/skinning_cs.hlsl
    // t_VertexBuffer <- SkinnedMeshInstance::skinningBindingSet
    // t_VertexBuffer <- SkinnedMeshInstance::skinningBindingSet
    // t_VertexBuffer <- SkinnedMeshInstance::skinningBindingSet

    // Animation
    //
    // https://github.com/NVIDIAGameWorks/donut/blob/main/src/engine/GltfImporter.cpp
    // GltfImporter::Load
    //     -> SceneGraph::m_Animations
    //
    // SceneGraphAnimation::Apply
    //     -> m_TargetNode::SetTransform

    // #include "../third-party/The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipeline.h"
    // ProcessGLTF

    // Scene Assets
    {
#include "cornell_box.h"
        uint32_t const mesh_section_count = g_cornell_box_mesh_section_count;
        this->m_mesh_sections.resize(mesh_section_count);
        for (uint32_t mesh_section_index = 0; mesh_section_index < mesh_section_count; ++mesh_section_index)
        {
            this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer = NULL;
            {
                std::vector<float> mesh_section_vertex_position_data;
                mesh_section_vertex_position_data.resize(3U * g_cornell_box_mesh_section_vertex_count[mesh_section_index]);
                for (uint32_t vertex_index = 0U; vertex_index < g_cornell_box_mesh_section_vertex_count[mesh_section_index]; ++vertex_index)
                {
                    mesh_section_vertex_position_data[3U * vertex_index] = g_cornell_box_mesh_section_vertex_position[mesh_section_index][3U * vertex_index] * 100.0F * -1.0F;
                    mesh_section_vertex_position_data[3U * vertex_index + 1U] = g_cornell_box_mesh_section_vertex_position[mesh_section_index][3U * vertex_index + 2U] * 100.0F;
                    mesh_section_vertex_position_data[3U * vertex_index + 2U] = g_cornell_box_mesh_section_vertex_position[mesh_section_index][3U * vertex_index + 1U] * 100.0F;
                }

                BufferLoadDesc vertex_position_buffer_desc = {};
                vertex_position_buffer_desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                vertex_position_buffer_desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                vertex_position_buffer_desc.mDesc.mSize = sizeof(float) * 3U * g_cornell_box_mesh_section_vertex_count[mesh_section_index];
                vertex_position_buffer_desc.pData = &mesh_section_vertex_position_data[0];
                vertex_position_buffer_desc.ppBuffer = &this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer;
                addResource(&vertex_position_buffer_desc, NULL);

                // The stack memory of "mesh_section_vertex_position_data" can not be released before the loading has been completed
                waitForAllResourceLoads();
            }
            assert(this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer);

            this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer = NULL;
            {
                std::vector<uint32_t> mesh_section_vertex_varying_data;
                mesh_section_vertex_varying_data.resize(g_cornell_box_mesh_section_vertex_count[mesh_section_index]);
                for (uint32_t vertex_index = 0U; vertex_index < g_cornell_box_mesh_section_vertex_count[mesh_section_index]; ++vertex_index)
                {
                    DirectX::PackedVector::XMUDECN4 vector_packed_output;
                    {
                        DirectX::XMFLOAT4A vector_unpacked_input(
                            ((g_cornell_box_mesh_section_vertex_normal[mesh_section_index][3U * vertex_index] * -1.0F) + 1.0F) * 0.5F,
                            (g_cornell_box_mesh_section_vertex_normal[mesh_section_index][3U * vertex_index + 2U] + 1.0F) * 0.5F,
                            (g_cornell_box_mesh_section_vertex_normal[mesh_section_index][3U * vertex_index + 1U] + 1.0F) * 0.5F,
                            1.0F);
                        assert((-0.01F < vector_unpacked_input.x) && (vector_unpacked_input.x < 1.01F));
                        assert((-0.01F < vector_unpacked_input.y) && (vector_unpacked_input.y < 1.01F));
                        assert((-0.01F < vector_unpacked_input.z) && (vector_unpacked_input.z < 1.01F));
                        assert((-0.01F < vector_unpacked_input.w) && (vector_unpacked_input.w < 1.01F));
                        DirectX::PackedVector::XMStoreUDecN4(&vector_packed_output, DirectX::XMLoadFloat4A(&vector_unpacked_input));
                    }
                    mesh_section_vertex_varying_data[vertex_index] = vector_packed_output.v;
                }

                BufferLoadDesc vertex_varying_buffer_desc = {};
                vertex_varying_buffer_desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                vertex_varying_buffer_desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                vertex_varying_buffer_desc.mDesc.mSize = sizeof(uint32_t) * g_cornell_box_mesh_section_vertex_count[mesh_section_index];
                vertex_varying_buffer_desc.pData = &mesh_section_vertex_varying_data[0];
                vertex_varying_buffer_desc.ppBuffer = &this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer;
                addResource(&vertex_varying_buffer_desc, NULL);

                // The stack memory of "mesh_section_vertex_varying_data" can not be released before the loading has been completed
                waitForAllResourceLoads();
            }
            assert(this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer);

            this->m_mesh_sections[mesh_section_index].m_index_buffer = NULL;
            {
                std::vector<uint16_t> mesh_section_index_data;
                mesh_section_index_data.resize(g_cornell_box_mesh_section_index_count[mesh_section_index]);
                for (uint32_t index_index = 0U; index_index < g_cornell_box_mesh_section_index_count[mesh_section_index]; ++index_index)
                {
                    mesh_section_index_data[index_index] = g_cornell_box_mesh_section_index[mesh_section_index][index_index];
                }

                BufferLoadDesc index_buffer_desc = {};
                index_buffer_desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
                index_buffer_desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                index_buffer_desc.mDesc.mSize = sizeof(uint16_t) * g_cornell_box_mesh_section_index_count[mesh_section_index];
                index_buffer_desc.pData = &mesh_section_index_data[0];
                index_buffer_desc.ppBuffer = &this->m_mesh_sections[mesh_section_index].m_index_buffer;
                addResource(&index_buffer_desc, NULL);

                // The stack memory of "mesh_section_index_data" can not be released before the loading has been completed
                waitForAllResourceLoads();
            }
            assert(this->m_mesh_sections[mesh_section_index].m_index_buffer);

            this->m_mesh_sections[mesh_section_index].m_index_count = g_cornell_box_mesh_section_index_count[mesh_section_index];

            this->m_mesh_sections[mesh_section_index].m_material_constant_buffer = NULL;
            {
                light_injection_mesh_material_set_constant_buffer_data light_injection_mesh_material_set_constant_buffer_data_instance;
                light_injection_mesh_material_set_constant_buffer_data_instance.base_color.x = g_cornell_box_mesh_section_base_color[mesh_section_index][0];
                light_injection_mesh_material_set_constant_buffer_data_instance.base_color.y = g_cornell_box_mesh_section_base_color[mesh_section_index][1];
                light_injection_mesh_material_set_constant_buffer_data_instance.base_color.z = g_cornell_box_mesh_section_base_color[mesh_section_index][2];
                light_injection_mesh_material_set_constant_buffer_data_instance.metallic = g_cornell_box_mesh_section_metallic[mesh_section_index];
                light_injection_mesh_material_set_constant_buffer_data_instance.roughness = g_cornell_box_mesh_section_roughness[mesh_section_index];

                BufferLoadDesc material_constant_buffer_desc = {};
                material_constant_buffer_desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                material_constant_buffer_desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                material_constant_buffer_desc.mDesc.mSize = sizeof(light_injection_mesh_material_set_constant_buffer_data);
                material_constant_buffer_desc.pData = &light_injection_mesh_material_set_constant_buffer_data_instance;
                material_constant_buffer_desc.ppBuffer = &this->m_mesh_sections[mesh_section_index].m_material_constant_buffer;
                addResource(&material_constant_buffer_desc, NULL);

                // The stack memory of "mesh_section_material_constant_data" can not be released before the loading has been completed
                waitForAllResourceLoads();
            }
            assert(this->m_mesh_sections[mesh_section_index].m_material_constant_buffer);
        }
    }

    // Scene Instances
    {
        uint32_t const mesh_section_instance_count = static_cast<uint32_t>(this->m_mesh_sections.size());
        this->m_mesh_section_instances.resize(mesh_section_instance_count);
        for (uint32_t mesh_section_instance_index = 0U; mesh_section_instance_index < mesh_section_instance_count; ++mesh_section_instance_index)
        {
            this->m_mesh_section_instances[mesh_section_instance_index].m_mesh_section_index = mesh_section_instance_index;
        }
    }
}

void demo_app::exit_scene()
{
    // Scene Assets
    {
        uint32_t const mesh_section_count = static_cast<uint32_t>(this->m_mesh_sections.size());
        for (uint32_t mesh_section_index = 0; mesh_section_index < mesh_section_count; ++mesh_section_index)
        {
            assert(this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer);
            removeResource(this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer);
            this->m_mesh_sections[mesh_section_index].m_vertex_position_buffer = NULL;

            assert(this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer);
            removeResource(this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer);
            this->m_mesh_sections[mesh_section_index].m_vertex_varying_buffer = NULL;

            assert(this->m_mesh_sections[mesh_section_index].m_index_buffer);
            removeResource(this->m_mesh_sections[mesh_section_index].m_index_buffer);
            this->m_mesh_sections[mesh_section_index].m_index_buffer = NULL;

            assert(this->m_mesh_sections[mesh_section_index].m_material_constant_buffer);
            removeResource(this->m_mesh_sections[mesh_section_index].m_material_constant_buffer);
            this->m_mesh_sections[mesh_section_index].m_material_constant_buffer = NULL;

            assert(!this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set);
        }
    }
}