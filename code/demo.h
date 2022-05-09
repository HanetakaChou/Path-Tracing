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

#ifndef _DEMO_H_
#define _DEMO_H_ 1

#include "../third-party/The-Forge/Common_3/Application/Interfaces/IApp.h"
#include "../third-party/The-Forge/Common_3/Graphics/Interfaces/IGraphics.h"

#include <DirectXMath.h>
#include <vector>

static constexpr uint32_t const FRAME_THROTTLING_COUNT = 3U;

class demo_app : public IApp
{
    // Renderer
    Renderer *m_renderer;
    Queue *m_graphics_queue;
    TinyImageFormat m_swap_chain_color_format;

    CmdPool *m_cmd_pools[FRAME_THROTTLING_COUNT];
    Cmd *m_cmds[FRAME_THROTTLING_COUNT];
    Semaphore *m_acquire_next_image_semaphore[FRAME_THROTTLING_COUNT];
    Semaphore *m_queue_submit_semaphore[FRAME_THROTTLING_COUNT];
    Fence *m_fences[FRAME_THROTTLING_COUNT];

    Buffer *m_upload_ring_buffer;
    uint32_t m_upload_ring_buffer_offset_alignment;
    void *m_upload_ring_buffer_update_desc;
    void *m_upload_ring_buffer_base;
    uint32_t m_upload_ring_buffer_begin[FRAME_THROTTLING_COUNT];
    uint32_t m_upload_ring_buffer_end[FRAME_THROTTLING_COUNT];

    uint32_t m_frame_throtting_index;

    // Scene
    struct mesh_section
    {
        Buffer *m_vertex_position_buffer;
        Buffer *m_vertex_varying_buffer;
        Buffer *m_index_buffer;
        uint32_t m_index_count;
        Buffer *m_material_constant_buffer;
        DescriptorSet *m_light_injection_mesh_material_descriptor_set;
    };
    std::vector<mesh_section> m_mesh_sections;

    struct mesh_section_instance
    {
        DirectX::XMFLOAT4X4 m_model_transform;
        uint32_t m_mesh_section_index;
    };
    std::vector<mesh_section_instance> m_mesh_section_instances;

    // Camera
    DirectX::XMFLOAT4X4 m_camera_view_transform;
    DirectX::XMFLOAT4X4 m_camera_projection_transform;

    // Load / Unload Resize / RenderTarget
    SwapChain *m_swap_chain;
    uint32_t m_swap_chain_image_width;
    uint32_t m_swap_chain_image_height;

    // Load / Unload Shader
    Shader *m_light_injection_shader;
    RootSignature *m_light_injection_root_signature;
    DescriptorSet *m_light_injection_render_pass_descriptor_set;
    uint32_t m_light_injection_vertex_buffer_position_stride;
    uint32_t m_light_injection_vertex_buffer_varying_stride;
    Pipeline *m_light_injection_pipeline;

    char const *GetName() override;

    bool Init() override;

    void Exit() override;

    bool Load(ReloadDesc *pReloadDesc) override;

    void Unload(ReloadDesc *pReloadDesc) override;

    void Update(float deltaTime) override;

    void Draw() override;

    void init_scene();

    void exit_scene();

    void load_shader();

    void unload_shader();

public:
    demo_app();
};

#endif