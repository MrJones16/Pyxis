#include "Core/Core.h"
#include <Renderer/BatchBuffer.h>
#include <Renderer/Material.h>
#include <Renderer/PipelineState.h>
#include <Renderer/Renderer.h>
#include <unordered_set>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace Pyxis {

// Static member definitions
SDL_Window *Renderer::s_Window = nullptr;
SDL_GPUDevice *Renderer::s_GPUDevice = nullptr;
Renderer::FrameData Renderer::s_FrameData = {};
std::unique_ptr<UploadQueue> Renderer::s_UploadQueue = nullptr;
bool Renderer::s_PassOpen = false;
RenderPassDescription Renderer::s_PassDescription = {};
std::vector<Renderer::RecordedDraw> Renderer::s_PassDraws = {};

// staging ring for all uploads; grows if a single upload is bigger
static constexpr uint32_t c_UploadRingCapacityBytes = 4u * 1024u * 1024u;

std::map<Renderer::SamplerType, SDL_GPUSampler *> Renderer::s_Samplers =
    std::map<SamplerType, SDL_GPUSampler *>();

bool Renderer::Init(const std::string &windowTitle, const glm::ivec2 resolution,
                    bool debug) {
    s_Window = SDL_CreateWindow(windowTitle.c_str(), resolution.x, resolution.y,
                                SDL_WINDOW_RESIZABLE);
    PX_BEGINSTEPS("Initializing Renderer...");
    if (s_Window == nullptr) {
        PX_ERROR("Unable to initialize SDL Window : {}", SDL_GetError());

        return false;
    }

    s_GPUDevice = SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(),
                                      debug, nullptr);
    if (s_GPUDevice == nullptr) {
        PX_ERROR("Error creating device: {}", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(s_GPUDevice, s_Window)) {
        PX_ERROR("Error claiming window for gpu device: {}", SDL_GetError());
        return false;
    }

    if (SDL_WindowSupportsGPUPresentMode(s_GPUDevice, s_Window,
                                         SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        SDL_SetGPUSwapchainParameters(s_GPUDevice, s_Window,
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      SDL_GPU_PRESENTMODE_IMMEDIATE);
    } else {
        PX_WARN("Unable to use IMMEDIATE mode!");
    }

    if (!SDL_ShaderCross_Init()) {
        PX_ERROR("Error initializing sdl3/shadercross!: {}", SDL_GetError());
        return false;
    }

    // Initialize default Texture samplers
    SDL_GPUSamplerCreateInfo samplerInfoPointClamp{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    auto sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointClamp);
    PX_ASSERT(sampler != nullptr, "Unable to create PointClamp Sampler!")
    s_Samplers[PointClamp] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoPointWrap{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };

    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointWrap);
    PX_ASSERT(sampler != nullptr, "Unable to create PointWrap Sampler!")
    s_Samplers[PointWrap] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoLinearClamp{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearClamp);
    PX_ASSERT(sampler != nullptr, "Unable to create LinearClamp Sampler!")
    s_Samplers[LinearClamp] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoLinearWrap{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearWrap);
    PX_ASSERT(sampler != nullptr, "Unable to create LinearWrap Sampler!")
    s_Samplers[LinearWrap] = sampler;

    s_FrameData = {};

    s_UploadQueue = std::make_unique<UploadQueue>(
        s_GPUDevice, c_UploadRingCapacityBytes, &Renderer::FlushPendingUploads);

    if (!TTF_Init()) {
        PX_ERROR("Failed to initialize SDL3_ttf: {}", SDL_GetError());
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");

    // reverse order of init

    TTF_Quit();

    // pending uploads are dropped (nothing will draw with them anymore)
    s_UploadQueue = nullptr;

    // release texture samplers
    // Texture::Shutdown(s_GPUDevice);
    for (auto &samplerkvp : s_Samplers) {
        SDL_ReleaseGPUSampler(s_GPUDevice, samplerkvp.second);
    }
    s_Samplers.clear();

    SDL_ShaderCross_Quit();

    SDL_DestroyGPUDevice(s_GPUDevice);
    s_GPUDevice = nullptr;
    SDL_DestroyWindow(s_Window);
    s_Window = nullptr;
}

//////////////////////
/// EVENT HANDLING ///
//////////////////////

// void Renderer::OnWindowResize(const glm::ivec2 &resolution) {} // todo

/////////////////////
/// GETS AND SETS ///
/////////////////////

void Renderer::SetTitle(const std::string &title) {
    SDL_SetWindowTitle(s_Window, title.c_str());
}

void Renderer::SetResolution(const glm::ivec2 &resolution) {
    SDL_SetWindowSize(s_Window, resolution.x, resolution.y);
}

glm::vec2 Renderer::GetResolution() {
    int w, h;
    SDL_GetWindowSize(s_Window, &w, &h);
    return glm::vec2(w, h);
}

//////////////////////
/// MAIN FUNCTIONS ///
//////////////////////

bool Renderer::BeginFrame() {
    if (!PX_VERIFY(s_FrameData.GPUCommandBuffer == nullptr,
                   "BeginFrame: a frame was already begun (missing EndFrame), "
                   "ending it first"))
        EndFrame();

    // we want to begin GPU work, so get the command buffer
    s_FrameData = {};
    s_FrameData.GPUCommandBuffer = BeginGPUCommandBuffer();
    if (s_FrameData.GPUCommandBuffer == nullptr)
        return false;

    // Acquire swapchain texture once per frame
    // This prevents multiple pipelines from trying to acquire the swapchain
    // texture, which would cause a deadlock
    uint32_t width = 0, height = 0;
    bool success = SDL_AcquireGPUSwapchainTexture(
        s_FrameData.GPUCommandBuffer, s_Window, &s_FrameData.SwapchainTexture,
        &width, &height);
    s_FrameData.SwapchainSize = {(int)width, (int)height};
    if (!success || s_FrameData.SwapchainTexture == nullptr) {
        if (!success)
            PX_ERROR("Failed to acquire the swapchain texture: {}",
                     SDL_GetError());
        // no frame this time: still submit the queued uploads with it
        s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
        EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
        s_UploadQueue->OnFrameSubmitted();
        s_FrameData = {};
        return false;
    }
    s_FrameData.AcquiredSwapchain = true;
    return true;
}

void Renderer::EndFrame() {
    if (s_FrameData.GPUCommandBuffer == nullptr)
        return; // no frame was begun (or BeginFrame failed)
    if (s_PassOpen) {
        PX_ERROR("EndFrame: pass {} was not ended, ending it now",
                 s_PassDescription.name);
        EndPass();
    }
    // uploads queued after the last render pass
    s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
    EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
    s_UploadQueue->OnFrameSubmitted();
    s_FrameData = {};
}

bool Renderer::IsFrameActive() {
    return s_FrameData.GPUCommandBuffer != nullptr;
}

Renderer::FrameData &Renderer::GetFrameData() { return s_FrameData; }

UploadQueue &Renderer::GetUploadQueue() { return *s_UploadQueue; }

void Renderer::FlushPendingUploads() {
    if (s_UploadQueue == nullptr || !s_UploadQueue->HasPendingUploads())
        return;
    if (IsFrameActive()) {
        s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
        return;
    }
    // no frame to record into: use (and submit) a separate command buffer
    SDL_GPUCommandBuffer *commandBuffer = BeginGPUCommandBuffer();
    if (commandBuffer == nullptr)
        return;
    s_UploadQueue->RecordCopyPass(commandBuffer);
    EndGPUCommandBuffer(commandBuffer);
    s_UploadQueue->OnFrameSubmitted();
}

SDL_GPUTextureFormat Renderer::GetSwapchainTextureFormat() {
    return SDL_GetGPUSwapchainTextureFormat(s_GPUDevice, s_Window);
}

/////////////////////
/// RENDER PASSES ///
/////////////////////

bool Renderer::BeginPass(const RenderPassDescription &description) {
    if (s_PassOpen) {
        PX_ERROR("BeginPass({}): pass {} was not ended, ending it first",
                 description.name, s_PassDescription.name);
        EndPass();
    }
    if (!IsFrameActive())
        return false;
    s_PassOpen = true;
    s_PassDescription = description;
    s_PassDraws.clear();
    return true;
}

void Renderer::Draw(const PipelineState &state, BatchBuffer &batch,
                    const Uniform *vertexUniform,
                    const Uniform *fragmentUniform) {
    if (!s_PassOpen)
        return; // no frame / no pass: nothing to draw into
    RecordedDraw draw;
    draw.state = &state;
    draw.batch = &batch;
    if (vertexUniform != nullptr)
        draw.vertexUniform = vertexUniform->m_Data;
    if (fragmentUniform != nullptr)
        draw.fragmentUniform = fragmentUniform->m_Data;
    s_PassDraws.push_back(std::move(draw));
}

// true if `state` can draw into the current pass (logged once per state)
static bool StateMatchesPass(const PipelineState &state,
                             const RenderPassDescription &pass,
                             SDL_GPUTextureFormat swapchainFormat) {
    static std::unordered_set<const PipelineState *> s_ReportedStates;
    const PipelineStateDescription &description = state.GetDescription();
    std::string problem;
    if (!state.IsValid()) {
        problem = "the pipeline state is invalid (shader/pipeline creation "
                  "failed)";
    } else if (description.colorTargets.size() !=
               pass.colorAttachments.size()) {
        problem = std::format("it has {} color targets, the pass has {}",
                              description.colorTargets.size(),
                              pass.colorAttachments.size());
    } else if (description.depth.hasDepthTarget != pass.hasDepthAttachment) {
        problem = "its depth target usage doesn't match the pass";
    } else {
        for (size_t i = 0; i < pass.colorAttachments.size(); i++) {
            const RenderPassColorAttachment &attachment =
                pass.colorAttachments[i];
            SDL_GPUTextureFormat format =
                attachment.useSwapchain
                    ? swapchainFormat
                    : (attachment.texture ? attachment.texture->GetFormat()
                                          : SDL_GPU_TEXTUREFORMAT_INVALID);
            if (format != description.colorTargets[i].format)
                problem = std::format("color target {} format differs", i);
        }
    }
    if (problem.empty())
        return true;
    if (s_ReportedStates.insert(&state).second)
        PX_ERROR("Skipping draws of pipeline state {} in pass {}: {}",
                 description.name, pass.name, problem);
    return false;
}

void Renderer::EndPass() {
    if (!s_PassOpen)
        return;
    s_PassOpen = false;
    SDL_GPUCommandBuffer *commandBuffer = s_FrameData.GPUCommandBuffer;
    const RenderPassDescription &pass = s_PassDescription;
    const SDL_GPUTextureFormat swapchainFormat = GetSwapchainTextureFormat();

    // 1. order/merge each batch and queue its vertex/index upload
    std::vector<const std::vector<BatchBuffer::DrawRun> *> drawRuns;
    drawRuns.reserve(s_PassDraws.size());
    for (RecordedDraw &draw : s_PassDraws)
        drawRuns.push_back(&draw.batch->PrepareDraw());

    // 2. one copy pass with every pending upload (batches + textures)
    FlushPendingUploads();

    // 3. the render pass
    std::vector<SDL_GPUColorTargetInfo> colorTargets;
    glm::ivec2 targetSize = {0, 0};
    for (const RenderPassColorAttachment &attachment : pass.colorAttachments) {
        SDL_GPUColorTargetInfo info{};
        if (attachment.useSwapchain) {
            info.texture = s_FrameData.SwapchainTexture;
            if (targetSize == glm::ivec2(0))
                targetSize = s_FrameData.SwapchainSize;
        } else if (attachment.texture != nullptr) {
            info.texture = attachment.texture->GetGPUTexture();
            if (targetSize == glm::ivec2(0))
                targetSize = attachment.texture->GetSize();
        }
        if (!PX_VERIFY(info.texture != nullptr,
                       "Render pass {}: a color attachment has no texture",
                       pass.name)) {
            s_PassDraws.clear();
            return;
        }
        info.clear_color = {attachment.clearColor.r, attachment.clearColor.g,
                            attachment.clearColor.b, attachment.clearColor.a};
        info.load_op = attachment.loadOp;
        info.store_op = attachment.storeOp;
        colorTargets.push_back(info);
    }
    SDL_GPUDepthStencilTargetInfo depthTarget{};
    if (pass.hasDepthAttachment) {
        const RenderPassDepthAttachment &depth = pass.depthAttachment;
        depthTarget.texture =
            depth.texture != nullptr ? depth.texture->GetGPUTexture() : nullptr;
        if (!PX_VERIFY(depthTarget.texture != nullptr,
                       "Render pass {}: the depth attachment has no texture",
                       pass.name)) {
            s_PassDraws.clear();
            return;
        }
        if (targetSize == glm::ivec2(0))
            targetSize = depth.texture->GetSize();
        depthTarget.clear_depth = depth.clearDepth;
        depthTarget.load_op = depth.loadOp;
        depthTarget.store_op = depth.storeOp;
        depthTarget.stencil_load_op = depth.stencilLoadOp;
        depthTarget.stencil_store_op = depth.stencilStoreOp;
        depthTarget.clear_stencil = depth.clearStencil;
    }

    SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(
        commandBuffer, colorTargets.data(), (uint32_t)colorTargets.size(),
        pass.hasDepthAttachment ? &depthTarget : nullptr);
    if (!PX_VERIFY(renderPass != nullptr,
                   "Failed to begin render pass {}: {}", pass.name,
                   SDL_GetError())) {
        s_PassDraws.clear();
        return;
    }

    SDL_GPUViewport viewport{};
    viewport.w = (float)targetSize.x;
    viewport.h = (float)targetSize.y;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(renderPass, &viewport);
    const SDL_Rect fullScissor = {0, 0, targetSize.x, targetSize.y};

    for (size_t drawIndex = 0; drawIndex < s_PassDraws.size(); drawIndex++) {
        RecordedDraw &draw = s_PassDraws[drawIndex];
        const std::vector<BatchBuffer::DrawRun> &runs = *drawRuns[drawIndex];
        if (runs.empty() || !StateMatchesPass(*draw.state, pass, swapchainFormat))
            continue;

        SDL_BindGPUGraphicsPipeline(renderPass, draw.state->GetGPUPipeline());
        SDL_GPUBufferBinding vertexBinding{
            .buffer = draw.batch->GetGPUVertexBuffer(), .offset = 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
        SDL_GPUBufferBinding indexBinding{
            .buffer = draw.batch->GetGPUIndexBuffer(), .offset = 0};
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding,
                               SDL_GPU_INDEXELEMENTSIZE_32BIT);
        if (!draw.vertexUniform.empty())
            SDL_PushGPUVertexUniformData(commandBuffer, 0,
                                         draw.vertexUniform.data(),
                                         (uint32_t)draw.vertexUniform.size());
        if (!draw.fragmentUniform.empty())
            SDL_PushGPUFragmentUniformData(
                commandBuffer, 0, draw.fragmentUniform.data(),
                (uint32_t)draw.fragmentUniform.size());

        for (const BatchBuffer::DrawRun &run : runs) {
            if (run.bindable != nullptr)
                run.bindable->Bind(commandBuffer, renderPass);
            if (run.hasScissor) {
                // clamp to the target: SDL/Vulkan want it inside
                int x0 = std::clamp(run.scissor.x, 0, targetSize.x);
                int y0 = std::clamp(run.scissor.y, 0, targetSize.y);
                int x1 = std::clamp(run.scissor.x + run.scissor.w, 0,
                                    targetSize.x);
                int y1 = std::clamp(run.scissor.y + run.scissor.h, 0,
                                    targetSize.y);
                if (x1 <= x0 || y1 <= y0)
                    continue; // fully clipped
                SDL_Rect scissor = {x0, y0, x1 - x0, y1 - y0};
                SDL_SetGPUScissor(renderPass, &scissor);
            } else {
                SDL_SetGPUScissor(renderPass, &fullScissor);
            }
            SDL_DrawGPUIndexedPrimitives(renderPass, run.indexCount, 1,
                                         run.firstIndex, 0, 0);
        }
    }

    SDL_EndGPURenderPass(renderPass);
    s_PassDraws.clear();
}

////////////////////////////////////////////////////////
/// PRIVATE FUNCTIONS TO BE USED BY TEXTURE/PIPELINE ///
////////////////////////////////////////////////////////

SDL_Window *Renderer::GetWindow() { return s_Window; };
SDL_GPUDevice *Renderer::GetGPUDevice() { return s_GPUDevice; };

SDL_GPUCommandBuffer *Renderer::BeginGPUCommandBuffer() {
    SDL_GPUCommandBuffer *cmdBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    PX_VERIFY(cmdBuffer != nullptr, "Failed to get a command buffer! {}",
              SDL_GetError());
    return cmdBuffer;
};

void Renderer::EndGPUCommandBuffer(SDL_GPUCommandBuffer *commandBuffer) {
    bool success = SDL_SubmitGPUCommandBuffer(commandBuffer);
    PX_VERIFY(success, "Failed to submit the command buffer: {}",
              SDL_GetError());
};

} // namespace Pyxis
