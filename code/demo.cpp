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

#include "demo.h"

#include "../third-party/The-Forge/Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../third-party/The-Forge/Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include <assert.h>

#include "../shaders/light_injection_shared.h"

static inline uint32_t linear_allocate(uint32_t *current_buffer_offset, uint32_t max_buffer_size, uint32_t memory_requirement, uint32_t buffer_alignment);

static inline DirectX::XMMATRIX XM_CALLCONV DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(float FovAngleY, float AspectRatio, float NearZ, float FarZ);

void demo_app::load_shader()
{
    this->m_light_injection_shader = NULL;
    {
        ShaderLoadDesc light_injection_shader_load_desc = {};
        light_injection_shader_load_desc.mVert.pFileName = "light_injection.vert";
        light_injection_shader_load_desc.mFrag.pFileName = "light_injection.frag";
        addShader(this->m_renderer, &light_injection_shader_load_desc, &this->m_light_injection_shader);
    }
    assert(this->m_light_injection_shader);

    this->m_light_injection_root_signature = NULL;
    {
        RootSignatureDesc light_injection_root_signature_desc = {};
        light_injection_root_signature_desc.ppShaders = &this->m_light_injection_shader;
        light_injection_root_signature_desc.mShaderCount = 1U;
        addRootSignature(this->m_renderer, &light_injection_root_signature_desc, &this->m_light_injection_root_signature);
    }
    assert(this->m_light_injection_root_signature);

    this->m_light_injection_render_pass_descriptor_set = NULL;
    {
        DescriptorSetDesc render_pass_descriptor_set_desc;
        render_pass_descriptor_set_desc.pRootSignature = this->m_light_injection_root_signature;
        render_pass_descriptor_set_desc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE;
        render_pass_descriptor_set_desc.mMaxSets = 1U;
        addDescriptorSet(this->m_renderer, &render_pass_descriptor_set_desc, &this->m_light_injection_render_pass_descriptor_set);
    }
    assert(this->m_light_injection_render_pass_descriptor_set);

    uint32_t const mesh_section_count = static_cast<uint32_t>(this->m_mesh_sections.size());
    for (uint32_t mesh_section_index = 0; mesh_section_index < mesh_section_count; ++mesh_section_index)
    {
        this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set = NULL;
        {
            DescriptorSetDesc mesh_material_descriptor_set_desc = {};
            mesh_material_descriptor_set_desc.pRootSignature = this->m_light_injection_root_signature;
            mesh_material_descriptor_set_desc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_PER_DRAW;
            mesh_material_descriptor_set_desc.mMaxSets = 1U;
            addDescriptorSet(this->m_renderer, &mesh_material_descriptor_set_desc, &this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set);
        }
        assert(this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set);

        {
            DescriptorData descriptor_data = {};
            descriptor_data.pName = "light_injection_mesh_material_set_constant_buffer";
            descriptor_data.ppBuffers = &this->m_mesh_sections[mesh_section_index].m_material_constant_buffer;
            updateDescriptorSet(this->m_renderer, 0U, this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set, 1U, &descriptor_data);
        }
    }

    // Binding - Position
    this->m_light_injection_vertex_buffer_position_stride = sizeof(float) * 3U;
    // Binding - Varying
    this->m_light_injection_vertex_buffer_varying_stride = sizeof(uint32_t);

    this->m_light_injection_pipeline = NULL;
    {
        VertexLayout vertex_layout = {};
        // Binding
        vertex_layout.mBindingCount = 2U;
        // Binding - Position
        vertex_layout.mBindings[0].mStride = this->m_light_injection_vertex_buffer_position_stride;
        // Binding - Varying
        vertex_layout.mBindings[1].mStride = this->m_light_injection_vertex_buffer_varying_stride;
        // Attribute
        vertex_layout.mAttribCount = 2U;
        // Attribute - Position
        vertex_layout.mAttribs[0].mBinding = 0U;
        vertex_layout.mAttribs[0].mLocation = 0U;
        vertex_layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertex_layout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertex_layout.mAttribs[0].mOffset = 0U;
        // Attribute - Normal
        vertex_layout.mAttribs[1].mBinding = 1U;
        vertex_layout.mAttribs[1].mLocation = 1U;
        vertex_layout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertex_layout.mAttribs[1].mFormat = TinyImageFormat_R10G10B10A2_UNORM;
        vertex_layout.mAttribs[1].mOffset = 0U;

        RasterizerStateDesc rasterizer_state_desc = {};
        rasterizer_state_desc.mFrontFace = FRONT_FACE_CCW;
        rasterizer_state_desc.mCullMode = CULL_MODE_BACK;

        DepthStateDesc depth_state_desc = {};
        depth_state_desc.mDepthTest = true;
        depth_state_desc.mDepthWrite = true;
        depth_state_desc.mDepthFunc = CMP_GREATER;

        PipelineDesc pipeline_desc = {};
        pipeline_desc.mType = PIPELINE_TYPE_GRAPHICS;
        pipeline_desc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipeline_desc.mGraphicsDesc.pVertexLayout = &vertex_layout;
        pipeline_desc.mGraphicsDesc.pRasterizerState = &rasterizer_state_desc;
        pipeline_desc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
        pipeline_desc.mGraphicsDesc.mSampleQuality = 0U;
        pipeline_desc.mGraphicsDesc.pShaderProgram = this->m_light_injection_shader;
        pipeline_desc.mGraphicsDesc.pRootSignature = this->m_light_injection_root_signature;
        pipeline_desc.mGraphicsDesc.pDepthState = &depth_state_desc;
        pipeline_desc.mGraphicsDesc.mRenderTargetCount = 1U;
        pipeline_desc.mGraphicsDesc.pColorFormats = &this->m_swap_chain_color_format;
        pipeline_desc.mGraphicsDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED; // TinyImageFormat_D32_SFLOAT;

        addPipeline(this->m_renderer, &pipeline_desc, &this->m_light_injection_pipeline);
    }
    assert(this->m_light_injection_pipeline);
}

void demo_app::unload_shader()
{
    assert(this->m_light_injection_pipeline);
    removePipeline(this->m_renderer, this->m_light_injection_pipeline);
    this->m_light_injection_pipeline = NULL;

    this->m_light_injection_vertex_buffer_position_stride = -1;
    this->m_light_injection_vertex_buffer_varying_stride = -1;

    uint32_t const mesh_section_count = static_cast<uint32_t>(this->m_mesh_sections.size());
    for (uint32_t mesh_section_index = 0U; mesh_section_index < mesh_section_count; ++mesh_section_index)
    {
        assert(this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set);
        removeDescriptorSet(this->m_renderer, this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set);
        this->m_mesh_sections[mesh_section_index].m_light_injection_mesh_material_descriptor_set = NULL;
    }

    assert(this->m_light_injection_render_pass_descriptor_set);
    removeDescriptorSet(this->m_renderer, this->m_light_injection_render_pass_descriptor_set);
    this->m_light_injection_render_pass_descriptor_set = NULL;

    assert(this->m_light_injection_root_signature);
    removeRootSignature(this->m_renderer, this->m_light_injection_root_signature);
    this->m_light_injection_root_signature = NULL;

    assert(this->m_light_injection_shader);
    removeShader(this->m_renderer, this->m_light_injection_shader);
    this->m_light_injection_shader = NULL;
}

void demo_app::Update(float deltaTime)
{
    // Camera
    {
        DirectX::XMFLOAT3 camera_eye_position(0.0F, 100.F, 32.0F);
        DirectX::XMFLOAT3 camera_eye_direction(0.0F, -1.0F, 0.0F);
        DirectX::XMFLOAT3 camera_up_direction(0.0F, 0.0F, 1.0F);
        DirectX::XMStoreFloat4x4(&this->m_camera_view_transform, DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&camera_eye_position), DirectX::XMLoadFloat3(&camera_eye_direction), DirectX::XMLoadFloat3(&camera_up_direction)));

        float camera_fov_angle_y = 1.0247777777F; // (static_cast<double>(XM_PIDIV2) - std::atan(1.7777777)) * 2.0
        float camera_aspect_ratio = 1.7777777F;
        float camera_near_z = 1.0F;
        float camera_far_z = 1000.0F;
        DirectX::XMStoreFloat4x4(&this->m_camera_projection_transform, DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(camera_fov_angle_y, camera_aspect_ratio, camera_near_z, camera_far_z));
    }

    // Scene Instances
    {
        uint32_t const mesh_section_instance_count = static_cast<uint32_t>(this->m_mesh_section_instances.size());
        for (uint32_t mesh_section_instance_index = 0U; mesh_section_instance_index < mesh_section_instance_count; ++mesh_section_instance_index)
        {
            DirectX::XMStoreFloat4x4(&this->m_mesh_section_instances[mesh_section_instance_index].m_model_transform, DirectX::XMMatrixIdentity());
        }
    }
}

void demo_app::Draw()
{
    if (this->m_swap_chain->mEnableVsync != this->mSettings.mVSyncEnabled)
    {
        waitQueueIdle(this->m_graphics_queue);
        toggleVSync(this->m_renderer, &this->m_swap_chain);
    }

    waitForFences(this->m_renderer, 1U, &this->m_fences[this->m_frame_throtting_index]);

    resetCmdPool(this->m_renderer, this->m_cmd_pools[this->m_frame_throtting_index]);

    beginCmd(this->m_cmds[this->m_frame_throtting_index]);

    uint32_t upload_ring_buffer_current = this->m_upload_ring_buffer_begin[this->m_frame_throtting_index];

    // Make the acquire as late as possible
    uint32_t swap_chain_image_index = -1;
    acquireNextImage(this->m_renderer, this->m_swap_chain, this->m_acquire_next_image_semaphore[this->m_frame_throtting_index], NULL, &swap_chain_image_index);
    assert(-1 != swap_chain_image_index);

    // Light Injection
    {
        cmdBeginDebugMarker(this->m_cmds[this->m_frame_throtting_index], 1.0F, 1.0F, 1.0F, "Light Injection");

        // Begin Render Pass
        {
            RenderTargetBarrier render_target_barrier = {};
            render_target_barrier.pRenderTarget = this->m_swap_chain->ppRenderTargets[swap_chain_image_index];
            render_target_barrier.mCurrentState = RESOURCE_STATE_UNDEFINED;
            render_target_barrier.mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(this->m_cmds[this->m_frame_throtting_index], 0U, NULL, 0U, NULL, 1U, &render_target_barrier);

            BindRenderTargetsDesc bind_render_targets_desc = {};
            bind_render_targets_desc.mRenderTargetCount = 1U;
            bind_render_targets_desc.mRenderTargets[0].pRenderTarget = this->m_swap_chain->ppRenderTargets[swap_chain_image_index];
            bind_render_targets_desc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
            bind_render_targets_desc.mRenderTargets[0].mClearValue.r = 0.0F;
            bind_render_targets_desc.mRenderTargets[0].mClearValue.g = 0.0F;
            bind_render_targets_desc.mRenderTargets[0].mClearValue.b = 0.0F;
            bind_render_targets_desc.mRenderTargets[0].mClearValue.a = 0.0F;
            // bind_render_targets_desc.mDepthStencil
            cmdBindRenderTargets(this->m_cmds[this->m_frame_throtting_index], &bind_render_targets_desc);
        }

        cmdSetViewport(this->m_cmds[this->m_frame_throtting_index], 0.0F, 0.0F, static_cast<float>(this->m_swap_chain_image_width), static_cast<float>(this->m_swap_chain_image_height), 0.0F, 1.0F);

        cmdSetScissor(this->m_cmds[this->m_frame_throtting_index], 0U, 0U, this->m_swap_chain_image_width, this->m_swap_chain_image_height);

        uint32_t const mesh_section_instance_count = static_cast<uint32_t>(this->m_mesh_section_instances.size());

        cmdBindPipeline(this->m_cmds[this->m_frame_throtting_index], this->m_light_injection_pipeline);

        uint32_t render_pass_set_per_frame_rootcbv_data_offset = linear_allocate(&upload_ring_buffer_current, this->m_upload_ring_buffer_end[this->m_frame_throtting_index], sizeof(light_injection_render_pass_set_per_frame_rootcbv_data), this->m_upload_ring_buffer_offset_alignment);
        {
            light_injection_render_pass_set_per_frame_rootcbv_data *light_injection_render_pass_set_per_frame_rootcbv_data_instance = reinterpret_cast<light_injection_render_pass_set_per_frame_rootcbv_data *>(reinterpret_cast<uintptr_t>(this->m_upload_ring_buffer_base) + render_pass_set_per_frame_rootcbv_data_offset);
            light_injection_render_pass_set_per_frame_rootcbv_data_instance->view_transform = this->m_camera_view_transform;
            light_injection_render_pass_set_per_frame_rootcbv_data_instance->projection_transform = this->m_camera_projection_transform;
        }

        for (uint32_t mesh_section_instance_index = 0U; mesh_section_instance_index < mesh_section_instance_count; ++mesh_section_instance_index)
        {
            mesh_section_instance const &current_mesh_section_instance = this->m_mesh_section_instances[mesh_section_instance_index];
            mesh_section const &current_mesh_section = this->m_mesh_sections[current_mesh_section_instance.m_mesh_section_index];

            uint32_t light_injection_render_pass_set_per_draw_rootcbv_data_offset = linear_allocate(&upload_ring_buffer_current, this->m_upload_ring_buffer_end[this->m_frame_throtting_index], sizeof(light_injection_render_pass_set_per_draw_rootcbv_data), this->m_upload_ring_buffer_offset_alignment);
            {
                light_injection_render_pass_set_per_draw_rootcbv_data *light_injection_render_pass_set_per_draw_rootcbv_data_instance = reinterpret_cast<light_injection_render_pass_set_per_draw_rootcbv_data *>(reinterpret_cast<uintptr_t>(this->m_upload_ring_buffer_base) + light_injection_render_pass_set_per_draw_rootcbv_data_offset);
                light_injection_render_pass_set_per_draw_rootcbv_data_instance->model_transform = current_mesh_section_instance.m_model_transform;
            }

            {
                DescriptorDataRange descriptor_data_ranges[2] = {};
                descriptor_data_ranges[0].mOffset = render_pass_set_per_frame_rootcbv_data_offset;
                descriptor_data_ranges[0].mSize = sizeof(light_injection_render_pass_set_per_frame_rootcbv_data);
                descriptor_data_ranges[1].mOffset = light_injection_render_pass_set_per_draw_rootcbv_data_offset;
                descriptor_data_ranges[1].mSize = sizeof(light_injection_render_pass_set_per_draw_rootcbv_data);

                DescriptorData descriptor_params[2] = {};
                descriptor_params[0].pName = "light_injection_render_pass_set_per_frame_rootcbv";
                descriptor_params[0].pRanges = &descriptor_data_ranges[0];
                descriptor_params[0].ppBuffers = &this->m_upload_ring_buffer;
                descriptor_params[1].pName = "light_injection_render_pass_set_per_draw_rootcbv";
                descriptor_params[1].pRanges = &descriptor_data_ranges[1];
                descriptor_params[1].ppBuffers = &this->m_upload_ring_buffer;
                cmdBindDescriptorSetWithRootCbvs(this->m_cmds[this->m_frame_throtting_index], 0U, this->m_light_injection_render_pass_descriptor_set, 2U, descriptor_params);
            }

            cmdBindDescriptorSet(this->m_cmds[this->m_frame_throtting_index], 0U, current_mesh_section.m_light_injection_mesh_material_descriptor_set);

            {
                Buffer *buffers[2] = {current_mesh_section.m_vertex_position_buffer, current_mesh_section.m_vertex_varying_buffer};
                uint32_t strides[2] = {this->m_light_injection_vertex_buffer_position_stride, this->m_light_injection_vertex_buffer_varying_stride};
                uint64_t offsets[2] = {0U, 0U};
                cmdBindVertexBuffer(this->m_cmds[this->m_frame_throtting_index], sizeof(buffers) / sizeof(buffers[0]), buffers, strides, offsets);
            }

            cmdBindIndexBuffer(this->m_cmds[this->m_frame_throtting_index], current_mesh_section.m_index_buffer, INDEX_TYPE_UINT16, 0U);

            cmdDrawIndexedInstanced(this->m_cmds[this->m_frame_throtting_index], current_mesh_section.m_index_count, 0U, 1U, 0U, 0U);
        }

        // End Render Pass
        {
            cmdBindRenderTargets(this->m_cmds[this->m_frame_throtting_index], NULL);

            RenderTargetBarrier render_target_barrier = {};
            render_target_barrier.pRenderTarget = this->m_swap_chain->ppRenderTargets[swap_chain_image_index];
            render_target_barrier.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            render_target_barrier.mNewState = RESOURCE_STATE_PRESENT;
            cmdResourceBarrier(this->m_cmds[this->m_frame_throtting_index], 0U, NULL, 0U, NULL, 1U, &render_target_barrier);
        }

        cmdEndDebugMarker(this->m_cmds[this->m_frame_throtting_index]);
    }

    endCmd(this->m_cmds[this->m_frame_throtting_index]);

    {
        QueueSubmitDesc queue_submit_desc = {};
        queue_submit_desc.mCmdCount = 1U;
        queue_submit_desc.ppCmds = &this->m_cmds[this->m_frame_throtting_index];
        queue_submit_desc.mWaitSemaphoreCount = 1U;
        queue_submit_desc.ppWaitSemaphores = &this->m_acquire_next_image_semaphore[this->m_frame_throtting_index];
        queue_submit_desc.mSignalSemaphoreCount = 1U;
        queue_submit_desc.ppSignalSemaphores = &this->m_queue_submit_semaphore[this->m_frame_throtting_index];
        queue_submit_desc.pSignalFence = this->m_fences[this->m_frame_throtting_index];
        queueSubmit(this->m_graphics_queue, &queue_submit_desc);
    }

    {
        QueuePresentDesc queue_present_desc = {};
        queue_present_desc.mIndex = swap_chain_image_index;
        queue_present_desc.pSwapChain = this->m_swap_chain;
        queue_present_desc.mWaitSemaphoreCount = 1U;
        queue_present_desc.ppWaitSemaphores = &this->m_queue_submit_semaphore[this->m_frame_throtting_index];
        queue_present_desc.mSubmitDone = true;
        queuePresent(this->m_graphics_queue, &queue_present_desc);
    }

    ++this->m_frame_throtting_index;
    this->m_frame_throtting_index %= FRAME_THROTTLING_COUNT;
}

static inline uint32_t linear_allocate(uint32_t *current_buffer_offset, uint32_t max_buffer_size, uint32_t memory_requirement, uint32_t buffer_alignment)
{
    // #include "../third-party/The-Forge/Common_3/Utilities/RingBuffer.h"
    // getGPURingBufferOffset

    uint32_t aligned_buffer_offset = round_up((*current_buffer_offset), buffer_alignment);

    assert((aligned_buffer_offset + memory_requirement) <= max_buffer_size);

    (*current_buffer_offset) = (aligned_buffer_offset + memory_requirement);

    return aligned_buffer_offset;
}

static inline DirectX::XMMATRIX XM_CALLCONV DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
{
    // [Reversed-Z](https://developer.nvidia.com/content/depth-precision-visualized)
    //
    // _  0  0  0
    // 0  _  0  0
    // 0  0  b -1
    // 0  0  a  0
    //
    // _  0  0  0
    // 0  _  0  0
    // 0  0 zb  -z
    // 0  0  a
    //
    // z' = -b - a/z
    //
    // Standard
    // 0 = -b + a/nearz // z=-nearz
    // 1 = -b + a/farz  // z=-farz
    // a = farz*nearz/(nearz - farz)
    // b = farz/(nearz - farz)
    //
    // Reversed-Z
    // 1 = -b + a/nearz // z=-nearz
    // 0 = -b + a/farz  // z=-farz
    // a = farz*nearz/(farz - nearz)
    // b = nearz/(farz - nearz)

    // __m128 _mm_shuffle_ps(__m128 lo,__m128 hi, _MM_SHUFFLE(hi3,hi2,lo1,lo0))
    // Interleave inputs into low 2 floats and high 2 floats of output. Basically
    // out[0]=lo[lo0];
    // out[1]=lo[lo1];
    // out[2]=hi[hi2];
    // out[3]=hi[hi3];

    // DirectX::XMMatrixPerspectiveFovRH

    float SinFov;
    float CosFov;
    DirectX::XMScalarSinCos(&SinFov, &CosFov, 0.5F * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;
    float b = NearZ / (FarZ - NearZ);
    float a = (FarZ / (FarZ - NearZ)) * NearZ;

    // Note: This is recorded on the stack
    DirectX::XMVECTOR rMem = {
        Width,
        Height,
        b,
        a};

    // Copy from memory to SSE register
    DirectX::XMVECTOR vValues = rMem;
    DirectX::XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp, vValues);
    // CosFov / SinFov,0,0,0
    DirectX::XMMATRIX M;
    M.r[0] = vTemp;
    // 0,Height / AspectRatio,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp, DirectX::g_XMMaskY);
    M.r[1] = vTemp;
    // x=b,y=a,0,-1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues, DirectX::g_XMNegIdentityR3, _MM_SHUFFLE(3, 2, 3, 2));
    // 0,0,b,-1.0f
    vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(3, 0, 0, 0));
    M.r[2] = vTemp;
    // 0,0,a,0.0f
    vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(2, 1, 0, 0));
    M.r[3] = vTemp;
    return M;
}