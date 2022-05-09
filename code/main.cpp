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

DEFINE_APPLICATION_MAIN(demo_app)

// \[NVIDIA Driver 128 MB\](https://developer.nvidia.com/content/constant-buffers-without-constant-pain-0)
// \[AMD Special Pool 256MB\](https://gpuopen.com/events/gdc-2018-presentations)
static constexpr uint32_t const g_upload_ring_buffer_size = 64U * 1024U * 1024U;

demo_app::demo_app()
{
    this->mSettings.mWidth = 1280U;
    this->mSettings.mHeight = 720U;
    this->mSettings.mDragToResize = false;
}

char const *demo_app::GetName()
{
    return "Path-Tracing";
}

bool demo_app::Init()
{
    {
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
    }

    {
        centerWindow(this->pWindow);
    }

    this->m_renderer = NULL;
    {
        // extern PlatformParameters gPlatformParameters;
        // gPlatformParameters.mSelectedRendererApi = RENDERER_API_VULKAN;

        // FORGE_EXPLICIT_RENDERER_API
        // FORGE_EXPLICIT_RENDERER_API_VULKAN
        // link "RendererVulkan.lib" instead of "Renderer.lib"

        RendererDesc settings = {};
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(this->GetName(), &settings, &this->m_renderer);
    }
    assert(this->m_renderer);

    this->m_graphics_queue = NULL;
    {
        QueueDesc queue_desc = {};
        queue_desc.mType = QUEUE_TYPE_GRAPHICS;
        initQueue(this->m_renderer, &queue_desc, &this->m_graphics_queue);
    }
    assert(this->m_graphics_queue);

    this->m_swap_chain_color_format = TinyImageFormat_UNDEFINED;
    {
        SwapChainDesc swap_chain_desc = {};
        swap_chain_desc.mPresentQueueCount = 1;
        swap_chain_desc.ppPresentQueues = &this->m_graphics_queue;
        swap_chain_desc.mWindowHandle = this->pWindow->handle;
        this->m_swap_chain_color_format = getSupportedSwapchainFormat(this->m_renderer, &swap_chain_desc, COLOR_SPACE_SDR_LINEAR);
    }
    assert(TinyImageFormat_UNDEFINED != this->m_swap_chain_color_format);

    initResourceLoaderInterface(this->m_renderer);

    this->m_upload_ring_buffer = NULL;
    {
        BufferLoadDesc upload_ring_buffer_desc = {};
        upload_ring_buffer_desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        upload_ring_buffer_desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        upload_ring_buffer_desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
        upload_ring_buffer_desc.mDesc.mSize = g_upload_ring_buffer_size;
        upload_ring_buffer_desc.ppBuffer = &this->m_upload_ring_buffer;
        addResource(&upload_ring_buffer_desc, NULL);
    }
    assert(this->m_upload_ring_buffer);

    this->m_upload_ring_buffer_offset_alignment = this->m_renderer->pGpu->mUniformBufferAlignment;

    this->m_upload_ring_buffer_base = NULL;
    {
        this->m_upload_ring_buffer_update_desc = new BufferUpdateDesc{};
        static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc)->pBuffer = this->m_upload_ring_buffer;
        static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc)->mDstOffset = 0U;
        static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc)->mSize = g_upload_ring_buffer_size;
        beginUpdateResource(static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc));

        this->m_upload_ring_buffer_base = static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc)->pMappedData;
    }

    for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
    {
        this->m_cmd_pools[frame_throttling_index] = NULL;
        {
            CmdPoolDesc cmd_pool_desc = {};
            cmd_pool_desc.pQueue = this->m_graphics_queue;
            cmd_pool_desc.mTransient = false;
            initCmdPool(this->m_renderer, &cmd_pool_desc, &this->m_cmd_pools[frame_throttling_index]);
        }
        assert(this->m_cmd_pools[frame_throttling_index]);

        this->m_cmds[frame_throttling_index] = NULL;
        {
            CmdDesc cmd_desc = {};
            cmd_desc.pPool = this->m_cmd_pools[frame_throttling_index];
            initCmd(this->m_renderer, &cmd_desc, &this->m_cmds[frame_throttling_index]);
        }
        assert(this->m_cmds[frame_throttling_index]);

        initSemaphore(this->m_renderer, &this->m_acquire_next_image_semaphore[frame_throttling_index]);

        initSemaphore(this->m_renderer, &this->m_queue_submit_semaphore[frame_throttling_index]);

        initFence(this->m_renderer, &this->m_fences[frame_throttling_index]);

        // In Vulkan, we are using the dynamic uniform buffer.
        // To use the same descritor set, the same uniform buffer should be used between different frames.
        this->m_upload_ring_buffer_begin[frame_throttling_index] = g_upload_ring_buffer_size * frame_throttling_index / FRAME_THROTTLING_COUNT;
        this->m_upload_ring_buffer_end[frame_throttling_index] = g_upload_ring_buffer_size * (frame_throttling_index + 1U) / FRAME_THROTTLING_COUNT;
    }

    this->m_frame_throtting_index = 0U;

    this->init_scene();

    return true;
}

void demo_app::Exit()
{
    this->exit_scene();

    for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
    {
        assert(this->m_fences[frame_throttling_index]);
        exitFence(this->m_renderer, this->m_fences[frame_throttling_index]);
        this->m_fences[frame_throttling_index] = NULL;

        assert(this->m_queue_submit_semaphore[frame_throttling_index]);
        exitSemaphore(this->m_renderer, this->m_queue_submit_semaphore[frame_throttling_index]);
        this->m_queue_submit_semaphore[frame_throttling_index] = NULL;

        assert(this->m_acquire_next_image_semaphore[frame_throttling_index]);
        exitSemaphore(this->m_renderer, this->m_acquire_next_image_semaphore[frame_throttling_index]);
        this->m_acquire_next_image_semaphore[frame_throttling_index] = NULL;

        assert(this->m_cmds[frame_throttling_index]);
        exitCmd(this->m_renderer, this->m_cmds[frame_throttling_index]);
        this->m_cmds[frame_throttling_index] = NULL;

        assert(this->m_cmd_pools[frame_throttling_index]);
        exitCmdPool(this->m_renderer, this->m_cmd_pools[frame_throttling_index]);
        this->m_cmd_pools[frame_throttling_index] = NULL;
    }

    endUpdateResource(static_cast<BufferUpdateDesc *>(this->m_upload_ring_buffer_update_desc));
    delete (this->m_upload_ring_buffer_update_desc);

    assert(this->m_upload_ring_buffer);
    removeResource(this->m_upload_ring_buffer);
    this->m_upload_ring_buffer = NULL;

    exitResourceLoaderInterface(this->m_renderer);

    assert(this->m_graphics_queue);
    exitQueue(this->m_renderer, this->m_graphics_queue);
    this->m_graphics_queue = NULL;

    assert(this->m_renderer);
    exitRenderer(this->m_renderer);
    exitGPUConfiguration();
    this->m_renderer = NULL;
}

bool demo_app::Load(ReloadDesc *pReloadDesc)
{
    if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
    {
        this->m_swap_chain = NULL;
        {
            SwapChainDesc swap_chain_desc = {};
            swap_chain_desc.mPresentQueueCount = 1;
            swap_chain_desc.ppPresentQueues = &this->m_graphics_queue;
            swap_chain_desc.mWindowHandle = this->pWindow->handle;
            swap_chain_desc.mWidth = getRectWidth(&this->pWindow->clientRect);
            swap_chain_desc.mHeight = getRectHeight(&this->pWindow->clientRect);
            swap_chain_desc.mEnableVsync = this->mSettings.mVSyncEnabled;
            swap_chain_desc.mColorFormat = this->m_swap_chain_color_format;
            swap_chain_desc.mColorSpace = COLOR_SPACE_SDR_LINEAR;
            swap_chain_desc.mImageCount = getRecommendedSwapchainImageCount(this->m_renderer, &pWindow->handle);
            addSwapChain(this->m_renderer, &swap_chain_desc, &this->m_swap_chain);
        }
        assert(this->m_swap_chain);

        this->m_swap_chain_image_width = this->m_swap_chain->ppRenderTargets[0]->mWidth;
        this->m_swap_chain_image_height = this->m_swap_chain->ppRenderTargets[0]->mHeight;

        for (uint32_t swap_chain_image_index = 0U; swap_chain_image_index < this->m_swap_chain->mImageCount; ++swap_chain_image_index)
        {
            assert(SAMPLE_COUNT_1 == this->m_swap_chain->ppRenderTargets[swap_chain_image_index]->mSampleCount);
            assert(0U == this->m_swap_chain->ppRenderTargets[swap_chain_image_index]->mSampleQuality);

            assert(this->m_swap_chain->ppRenderTargets[swap_chain_image_index]->mWidth == this->m_swap_chain_image_width);
            assert(this->m_swap_chain->ppRenderTargets[swap_chain_image_index]->mHeight == this->m_swap_chain_image_height);
        }
    }

    if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
    {
        this->load_shader();
    }

    return true;
}

void demo_app::Unload(ReloadDesc *pReloadDesc)
{
    waitForFences(this->m_renderer, FRAME_THROTTLING_COUNT, this->m_fences);

    if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
    {
        this->unload_shader();
    }

    if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
    {
        this->m_swap_chain_image_width = -1;
        this->m_swap_chain_image_height = -1;

        assert(this->m_swap_chain);
        removeSwapChain(this->m_renderer, this->m_swap_chain);
        this->m_swap_chain = NULL;
    }
}