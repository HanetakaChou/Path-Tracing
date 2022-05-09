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

#ifndef _ASSET_IMPORT_H_
#define _ASSET_IMPORT_H_ 1

#include <vector>
#include <string>
#include <DirectXMath.h>

class joint
{
    DirectX::XMFLOAT4 m_quaternion;
    DirectX::XMFLOAT3 m_translation;

public:
    void init();
    void set_transform(DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation);
    DirectX::XMFLOAT4 get_quaternion() const;
    DirectX::XMFLOAT3 get_translation() const;
};

class pose
{
    std::vector<joint> m_joints;

public:
    void init(size_t joint_count);
    void set_transform(size_t joint_index, DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation);
    size_t get_joint_count() const;
    DirectX::XMFLOAT4 get_quaternion(size_t joint_index) const;
    DirectX::XMFLOAT3 get_translation(size_t joint_index) const;
};

// Bake Animation
// Since this demo is for rendering, we do NOT care too much about the animation. And we simply sample the poses for each frame.
class animated_skeleton
{
    std::vector<pose> m_poses;

public:
    void init(size_t frame_count, size_t joint_count);
    void set_transform(size_t frame_index, size_t joint_index, DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation);
    size_t get_joint_count() const;
    pose const &get_pose(size_t frame_index) const;
};

struct vertex_position_binding
{
    // R32G32B32_FLOAT
    DirectX::XMFLOAT3 m_position;
};

struct vertex_varying_binding
{
    // R10G10B10A2_UNORM
    uint32_t m_normal;
    // R10G10B10A2_UNORM
    uint32_t m_tangent;
    // R16G16_UNORM
    uint32_t m_texcoord;
};

struct vertex_skinned_binding
{
    // R16G16B16A16_UINT
    uint64_t m_indices;
    // R16G16B16A16_UNORM
    uint64_t m_weights;
};

struct subset_asset_data
{
    std::vector<vertex_position_binding> m_vertex_position_binding;

    std::vector<vertex_varying_binding> m_vertex_varying_binding;

    std::vector<vertex_skinned_binding> m_vertex_skinned_binding;

    std::vector<uint32_t> m_indices;

    std::string m_normal_texture_image_uri;

    float m_normal_texture_scale;

    DirectX::XMFLOAT3 m_emissive_factor;

    std::string m_emissive_texture_image_uri;

    DirectX::XMFLOAT3 m_base_color_factor;

    std::string m_base_color_texture_image_uri;

    float m_metallic_factor;

    float m_roughness_factor;

    std::string m_metallic_roughness_texture_image_uri;
};

struct mesh_asset_data
{
    std::vector<subset_asset_data> m_subsets;

    bool m_skinned;
};

struct mesh_instance_data
{
    DirectX::XMFLOAT4X4 m_model_transform;
    animated_skeleton m_animation_skeleton;
};

extern void asset_import_gltf(std::vector<mesh_asset_data> &out_mesh_data, std::vector<std::vector<mesh_instance_data>> &out_mesh_instance_data, float frame_rate, char const *path);

#endif