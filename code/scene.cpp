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

#include "scene.h"
#include "asset_import.h"
#include <assert.h>

void scene::import_gltf(DirectX::XMFLOAT4X4 const &root_transform, char const *path)
{
    // 60 FPS
    constexpr float const frame_rate = 60.0F;

    std::vector<mesh_asset_data> mesh_data;
    std::vector<std::vector<mesh_instance_data>> mesh_instance_data;
    asset_import_gltf(mesh_data, mesh_instance_data, frame_rate, path);
}
