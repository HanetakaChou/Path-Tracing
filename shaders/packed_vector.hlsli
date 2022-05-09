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

#ifndef _PACKED_VECTOR_HLSLI_
#define _PACKED_VECTOR_HLSLI_ 1

float4 R10G10B10A2_SNORM_to_FLOAT4(uint packed_input)
{
	// DirectX::PackedVector::XMLoadDecN4

	int element_x = ((int(packed_input & 0x3FF) ^ 0x200) - 0x200);
	int element_y = (((int(packed_input >> 10) & 0x3FF) ^ 0x200) - 0x200);
	int element_z = (((int(packed_input >> 20) & 0x3FF) ^ 0x200) - 0x200);
	int element_w = ((int(packed_input >> 30) ^ 0x2) - 0x2);

	return clamp(float4(element_x, element_y, element_z, element_w) * float4(1.0f / 511.0f, 1.0f / 511.0f, 1.0f / 511.0f, 1.0f), float4(-1.0, -1.0, -1.0, -1.0), float4(1.0, 1.0, 1.0, 1.0));
}

uint FLOAT4_to_R10G10B10A2_SNORM(float4 unpacked_input)
{
	// DirectX::PackedVector::XMStoreDecN4

	float4 tmp = clamp(unpacked_input, float4(-1.0, -1.0, -1.0, -1.0), float4(1.0, 1.0, 1.0, 1.0)) * float4(511.0f, 511.0f, 511.0f, 1.0f);

	int element_x = (int(tmp.x) & 0x3FF);
	int element_y = ((int(tmp.y) & 0x3FF) << 10);
	int element_z = ((int(tmp.z) & 0x3FF) << 20);
	int element_w = (int(tmp.w) << 30);

	return uint(element_x | element_y | element_z | element_w);
}

float2 R16G16_SNORM_to_FLOAT2(uint packed_input)
{
	// DirectX::PackedVector::XMLoadShortN2

	int element_x = ((int(packed_input & 0xffff) ^ 0x8000) - 0x8000);
	int element_y = (((int(packed_input >> 16) & 0xffff) ^ 0x8000) - 0x8000);

	return clamp(float2(element_x, element_y) * (1.0f / 32767.0f), float2(-1.0, -1.0), float2(1.0, 1.0));
}

uint R16G16_FLOAT2_to_SNORM(float2 unpacked_input)
{
	// DirectX::PackedVector::XMStoreShortN2

	float2 tmp = clamp(unpacked_input, float2(-1.0, -1.0), float2(1.0, 1.0)) * float2(32767.0f, 32767.0f);

	int element_x = (int(tmp.x) & 0xffff);
	int element_y = ((int(tmp.y) & 0xffff) << 16);

	return uint(element_x | element_y);
}


float2 R16G16_UNORM_to_FLOAT2(uint packed_input)
{
	// DirectX::PackedVector::XMLoadUShortN2

	uint element_x = (packed_input & 0xffff);
	uint element_y = (packed_input >> 16);

	return float2(element_x, element_y) * (1.0f / 65535.0f);
}

uint R16G16_FLOAT2_to_UNORM(float2 unpacked_input)
{
	// DirectX::PackedVector::XMStoreUShortN2

	float2 tmp = clamp(unpacked_input, float2(0.0, 0.0), float2(1.0, 1.0)) * float2(65535.0f, 65535.0f) + float2(0.5f, 0.5f);

	uint element_x = (uint(tmp.x) & 0xffff);
	uint element_y = ((uint(tmp.y) & 0xffff) << 16);

	return uint(element_x | element_y);
}

uint4 R16G16B16A16_UINT_to_UINT4(uint2 packed_input)
{
	// DirectX::PackedVector::XMLoadUShort4

	uint element_x = (packed_input.x & 0xffff);
	uint element_y = (packed_input.x >> 16);
	uint element_z = (packed_input.y & 0xffff);
	uint element_w = (packed_input.y >> 16);

	return uint4(element_x, element_y, element_z, element_w);
}

float4 R16G16B16A16_UNORM_to_FLOAT4(uint2 packed_input)
{
	// DirectX::PackedVector::XMLoadUShortN4

	uint element_x = (packed_input.x & 0xffff);
	uint element_y = (packed_input.x >> 16);
	uint element_z = (packed_input.y & 0xffff);
	uint element_w = (packed_input.y >> 16);

	return float4(element_x, element_y, element_z, element_w) * (1.0f / 65535.0f);
}

#endif