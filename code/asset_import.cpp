#include "asset_import.h"

void joint::init()
{
    DirectX::XMStoreFloat4(&this->m_quaternion, DirectX::XMQuaternionIdentity());
    DirectX::XMStoreFloat3(&this->m_translation, DirectX::XMVectorZero());
}

void joint::set_transform(DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation)
{
    this->m_quaternion = quaternion;
    this->m_translation = translation;
}

DirectX::XMFLOAT4 joint::get_quaternion() const
{
    return this->m_quaternion;
}

DirectX::XMFLOAT3 joint::get_translation() const
{
    return this->m_translation;
}

void pose::init(size_t joint_count)
{
    this->m_joints.resize(joint_count);

    for (size_t joint_index = 0; joint_index < joint_count; ++joint_index)
    {
        this->m_joints[joint_index].init();
    }
}

void pose::set_transform(size_t joint_index, DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation)
{
    this->m_joints[joint_index].set_transform(quaternion, translation);
}

size_t pose::get_joint_count() const
{
    return this->m_joints.size();
}

DirectX::XMFLOAT4 pose::get_quaternion(size_t joint_index) const
{
    assert(joint_index < this->m_joints.size());
    return this->m_joints[joint_index].get_quaternion();
}

DirectX::XMFLOAT3 pose::get_translation(size_t joint_index) const
{
    assert(joint_index < this->m_joints.size());
    return this->m_joints[joint_index].get_translation();
}

void animated_skeleton::init(size_t frame_count, size_t joint_count)
{
    this->m_poses.resize(frame_count);

    for (size_t frame_index = 0; frame_index < frame_count; ++frame_index)
    {
        this->m_poses[frame_index].init(joint_count);
    }
}

void animated_skeleton::set_transform(size_t frame_index, size_t joint_index, DirectX::XMFLOAT4 const &quaternion, DirectX::XMFLOAT3 const &translation)
{
    this->m_poses[frame_index].set_transform(joint_index, quaternion, translation);
}

size_t animated_skeleton::get_joint_count() const
{
    return ((this->m_poses.size() > 0) ? (this->m_poses[0].get_joint_count()) : 0);
}

pose const &animated_skeleton::get_pose(size_t frame_index) const
{
    size_t const frame_count = this->m_poses.size();
    assert(frame_count > 0);

    size_t const pose_index = frame_index % frame_count;
    return this->m_poses[pose_index];
}
