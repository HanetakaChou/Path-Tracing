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

#define _CRT_SECURE_NO_WARNINGS 1

#define CGLTF_IMPLEMENTATION
#include "../third-party/cgltf/cgltf.h"

#include "../third-party/The-Forge/Common_3/Utilities/Interfaces/IFileSystem.h"

extern "C" cgltf_result cgltf_fs_read_file(const struct cgltf_memory_options *memory_options, const struct cgltf_file_options *, const char *path, cgltf_size *size, void **data)
{
    void *(*const memory_alloc)(void *, cgltf_size) = memory_options->alloc_func ? memory_options->alloc_func : &cgltf_default_alloc;
    void (*const memory_free)(void *, void *) = memory_options->free_func ? memory_options->free_func : &cgltf_default_free;

    FileStream file = {};
    if (!fsOpenStreamFromPath(RD_MESHES, path, FM_READ, &file))
    {
        return cgltf_result_file_not_found;
    }

    cgltf_size file_size = size ? *size : 0;

    if (file_size == 0)
    {
        ssize_t length = fsGetStreamFileSize(&file);

        if (length < 0)
        {
            fsCloseStream(&file);
            return cgltf_result_io_error;
        }

        file_size = length;
    }

    void *file_data = memory_alloc(memory_options->user_data, file_size);
    if (!file_data)
    {
        fsCloseStream(&file);
        return cgltf_result_out_of_memory;
    }

    cgltf_size read_size = fsReadFromStream(&file, file_data, file_size);

    fsCloseStream(&file);

    if (read_size != file_size)
    {
        memory_free(memory_options->user_data, file_data);
        return cgltf_result_io_error;
    }

    if (size)
    {
        *size = file_size;
    }
    if (data)
    {
        *data = file_data;
    }

    return cgltf_result_success;
}

extern "C" void cgltf_fs_file_release(const struct cgltf_memory_options *memory_options, const struct cgltf_file_options *, void *data)
{
    void (*const memory_free)(void *, void *) = memory_options->free_func ? memory_options->free_func : &cgltf_default_free;

    memory_free(memory_options->user_data, data);
}

#include "../third-party/The-Forge/Common_3/Utilities/Interfaces/IMemory.h"

extern "C" void *cgltf_tf_alloc(void *, cgltf_size size)
{
    return tf_malloc(size);
}

extern "C" void cgltf_tf_free(void *, void *ptr)
{
    tf_free(ptr);
}
