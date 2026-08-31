#include "Core/Core.h"
#include <Renderer/Pipeline.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <queue>

namespace Pyxis {
Pipeline::Pipeline(
    const glm::ivec2 &resolution, uint32_t maxVertices, uint32_t vertexSize,
    uint32_t maxIndices, std::vector<SDL_GPUVertexAttribute> vertexAttributes,
    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
    std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
    SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    bool TargetsSwapchain)
    : m_Resolution(resolution), m_VertexSize(vertexSize),
      m_MaxIndices(maxIndices), m_ColorTargetInfos(colorTargetInfos),
      m_TargetSwapchain(TargetsSwapchain) {

    PX_BEGINSTEPS("Creating Pipeline");

    auto device = Renderer::GetGPUDevice();

    //////////////// LOAD VERTEX SHADER ////////////////

    // first, load HLSL shader
    size_t hlslCodeSize;
    void *hlslCode = SDL_LoadFile(vertexShaderPath.c_str(), &hlslCodeSize);

    PX_ASSERT(hlslCode != nullptr, "Unable to load HLSL Shader file {} : {}",
              vertexShaderPath, SDL_GetError())
    PX_STEPSUCCESS("Loaded HLSL File {}", vertexShaderPath);

    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.shader_stage =
        SDL_ShaderCross_ShaderStage::SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    hlslInfo.entrypoint = "main";
    hlslInfo.include_dir = nullptr;
    hlslInfo.props = 0;
    hlslInfo.source = (char *)hlslCode;

    // now, we compile it into SPIRV bytecode:
    size_t spirvCodeSize;
    void *spirvCode =
        SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvCodeSize);

    // free HLSL file
    SDL_free(hlslCode);

    PX_ASSERT(spirvCode != nullptr,
              "Unable to compile vertex shader into SPIRV: {}", SDL_GetError())

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.shader_stage =
        SDL_ShaderCross_ShaderStage::SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    spirvInfo.bytecode = (Uint8 *)spirvCode;
    spirvInfo.bytecode_size = spirvCodeSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.props = 0;
    PX_STEPSUCCESS("Compiled HLSL into SPIRV");

    SDL_ShaderCross_GraphicsShaderMetadata *metaDataVertex =
        SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8 *)spirvCode, spirvCodeSize,
                                             0);
    PX_ASSERT(metaDataVertex != nullptr,
              "Unable to reflect vertex shader metadata: {}", SDL_GetError());
    PX_STEPSUCCESS("Reflected vertex shader metadata");

    SDL_GPUShader *vertexShader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
            device, &spirvInfo, &metaDataVertex->resource_info, 0);
    PX_ASSERT(vertexShader != nullptr,
              "Unable to compile vertex shader into GPUShader: {}",
              SDL_GetError());
    PX_STEPSUCCESS("Compiled GPU Vertex Shader");

    SDL_free(metaDataVertex);
    SDL_free(spirvCode);

    //////////////// LOAD FRAGMENT SHADER ////////////////

    // first, load HLSL shader
    hlslCode = SDL_LoadFile(fragmentShaderPath.c_str(), &hlslCodeSize);
    PX_ASSERT(hlslCode != nullptr, "Unable to load HLSL Shader file {} : {}",
              fragmentShaderPath, SDL_GetError())
    PX_STEPSUCCESS("Loaded HLSL file {}", fragmentShaderPath);

    hlslInfo.shader_stage =
        SDL_ShaderCross_ShaderStage::SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    hlslInfo.entrypoint = "main";
    hlslInfo.include_dir = nullptr;
    hlslInfo.props = 0;
    hlslInfo.source = (char *)hlslCode;

    // now, we compile it into SPIRV bytecode:
    spirvCode = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvCodeSize);
    // free HLSL file
    SDL_free(hlslCode);

    PX_ASSERT(spirvCode != nullptr,
              "Unable to compile vertex shader into SPIRV: {}", SDL_GetError());
    PX_STEPSUCCESS("Compiled HLSL into SPIRV");

    spirvInfo.shader_stage =
        SDL_ShaderCross_ShaderStage::SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    spirvInfo.bytecode = (Uint8 *)spirvCode;
    spirvInfo.bytecode_size = spirvCodeSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.props = 0;

    SDL_ShaderCross_GraphicsShaderMetadata *metaDataFragment =
        SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8 *)spirvCode, spirvCodeSize,
                                             0);
    PX_ASSERT(metaDataFragment != nullptr,
              "Unable to refelct fragment shader metadata: {}", SDL_GetError());
    PX_STEPSUCCESS("Reflected fragment shader metadata");

    SDL_GPUShader *fragmentShader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
            device, &spirvInfo, &metaDataFragment->resource_info, 0);
    PX_ASSERT(fragmentShader != nullptr,
              "Unable to compile fragment shader into GPUShader: {}",
              SDL_GetError());

    PX_STEPSUCCESS("Compiled GPU Fragment Shader");

    SDL_free(metaDataFragment);
    SDL_free(spirvCode);

    // At this point, both shaders are made!

    //////////////// CREATE VERTEX BUFFER ////////////////
    SDL_GPUBufferCreateInfo vertexBufferInfo{};
    m_MaxSize = maxVertices * m_VertexSize;
    vertexBufferInfo.size = m_MaxSize;
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_VertexBuffer = SDL_CreateGPUBuffer(device, &vertexBufferInfo);
    PX_ASSERT(m_VertexBuffer != nullptr, "Failed to create vertex buffer! {}",
              SDL_GetError())
    PX_STEPSUCCESS("Created vertex buffer");

    //////////////// CREATE INDEX BUFFER ////////////////
    SDL_GPUBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.size = m_MaxIndices * sizeof(uint32_t);
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    m_IndexBuffer = SDL_CreateGPUBuffer(device, &indexBufferInfo);
    PX_ASSERT(m_IndexBuffer != nullptr, "Failed to create index buffer! {}",
              SDL_GetError())
    PX_STEPSUCCESS("Created index buffer");

    //////////////// CREATE VERTEX TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo vtbInfo{};
    vtbInfo.size = m_MaxSize;
    vtbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_VertexTransferBuffer = SDL_CreateGPUTransferBuffer(device, &vtbInfo);
    // Setup buffer location as well to be used later
    m_VertexTransferBufferLocation.transfer_buffer = m_VertexTransferBuffer;
    m_VertexTransferBufferLocation.offset = 0;
    PX_ASSERT(m_VertexTransferBuffer != nullptr,
              "Failed to create vertex transfer buffer! {}", SDL_GetError());
    PX_STEPSUCCESS("Created vertex transfer buffer");

    //////////////// CREATE INDEX TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo itbInfo{};
    itbInfo.size = m_MaxIndices * sizeof(uint32_t);
    itbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_IndexTransferBuffer = SDL_CreateGPUTransferBuffer(device, &itbInfo);
    // Setup buffer location as well to be used later
    m_IndexTransferBufferLocation.transfer_buffer = m_IndexTransferBuffer;
    m_IndexTransferBufferLocation.offset = 0;
    PX_ASSERT(m_IndexTransferBuffer != nullptr,
              "Failed to create index transfer buffer! {}", SDL_GetError());
    PX_STEPSUCCESS("Created index transfer buffer");

    //////////////// CREATE PIPELINE ////////////////
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // describe the vertex buffers
    SDL_GPUVertexBufferDescription vertexBufferDesctiptions[1];
    vertexBufferDesctiptions[0].slot = 0;
    vertexBufferDesctiptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesctiptions[0].instance_step_rate = 0;
    vertexBufferDesctiptions[0].pitch = m_VertexSize;

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions =
        vertexBufferDesctiptions;

    pipelineInfo.vertex_input_state.num_vertex_attributes =
        vertexAttributes.size();
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes.data();

    pipelineInfo.target_info.num_color_targets = colorTargetDescriptions.size();
    pipelineInfo.target_info.color_target_descriptions =
        colorTargetDescriptions.data();

    // Setup depth & stencil stuff
    if (depthStencilTargetInfo != nullptr) {
        m_HasDepthStencilTexture = true;
        m_DepthStencilTargetInfo = *depthStencilTargetInfo;

        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format =
            SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

        SDL_GPUDepthStencilState dss{};
        dss.enable_depth_test = true;
        dss.enable_depth_write = true;
        dss.enable_stencil_test = false;
        dss.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        dss.write_mask = 0xFF;

        pipelineInfo.depth_stencil_state = dss;
    } else {
        pipelineInfo.target_info.has_depth_stencil_target = false;
    }

    // create the pipeline
    m_GraphicsPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    PX_TRACE("Default pipeline made with {} color targets",
             m_ColorTargetInfos.size());

    // free shaders as well
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);
    PX_ASSERT(m_GraphicsPipeline != nullptr,
              "Failed to create graphics pipeline: {}", SDL_GetError())
    PX_STEPSUCCESS("Created graphics pipeline");
    PX_ENDSTEPS();
}

Pipeline::~Pipeline() {
    auto device = Renderer::GetGPUDevice();
    SDL_ReleaseGPUBuffer(device, m_VertexBuffer);
    SDL_ReleaseGPUTransferBuffer(device, m_VertexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(device, m_IndexTransferBuffer);
    SDL_ReleaseGPUGraphicsPipeline(device, m_GraphicsPipeline);
    m_VertexUniform = nullptr;
    m_FragmentUniform = nullptr;
}

//////////////////////
/// MAIN FUNCTIONS ///
//////////////////////

void Pipeline::Draw(Renderer::FrameData &frameData) {

    if (TargetsSwapchain()) {

        m_ColorTargetInfos[0].texture = frameData.SwapchainTexture;
    }
    struct BindableBatch {
        Ref<Bindable> bindable;
        uint32_t vertexOffset; // in vertices not size
        uint32_t vertexCount;
        uint32_t indexOffset;
        uint32_t indexCount;
        BindableBatch(Ref<Bindable> b, uint32_t vo, uint32_t vc, uint32_t io,
                      uint32_t ic)
            : bindable(b), vertexOffset(vo), vertexCount(vc), indexOffset(io),
              indexCount(ic) {}
    };
    std::queue<BindableBatch> batchesQueue;
    std::queue<Ref<Bindable>> unusedMaterials;

    if (m_VertexUniform != nullptr && m_VertexUniform->size > 0) {
        SDL_PushGPUVertexUniformData(frameData.GPUCommandBuffer, 0,
                                     m_VertexUniform->data,
                                     m_VertexUniform->size);
    }
    if (m_FragmentUniform != nullptr && m_FragmentUniform->size > 0) {
        SDL_PushGPUFragmentUniformData(frameData.GPUCommandBuffer, 0,
                                       m_FragmentUniform->data,
                                       m_FragmentUniform->size);
    }

    Map();
    // we need to add all the grouped materials into the one big vertex buffer
    for (auto &kvp : m_BindableBuffers) {
        uint32_t vertexCount = kvp.second.vertexData.size() / m_VertexSize;
        uint32_t indexCount = kvp.second.indexData.size();
        if (vertexCount <= 0 || indexCount <= 0) {
            // this frame nothing with this material was drawn.
            // At this point, lets delete the vector in the map, so we can
            // de-allocate the size we used.
            unusedMaterials.push(kvp.first);
            continue;
        }

        if (vertexCount + m_VertexCount > m_MaxSize / m_VertexSize) {
            PX_WARN("Too many vertices to draw! skipping.");
            kvp.second.clear();
            continue;
        }
        if (indexCount + m_IndexCount > m_MaxIndices) {
            PX_WARN("Too many indices to draw! skipping.");
            kvp.second.clear();
            continue;
        }

        uint32_t vertexDataOffset = (m_VertexCount * m_VertexSize);
        batchesQueue.push(BindableBatch(kvp.first, m_VertexCount, vertexCount,
                                        m_IndexCount, indexCount));
        std::memcpy((uint8_t *)m_VertexTransferBufferData + vertexDataOffset,
                    kvp.second.vertexData.data(), vertexCount * m_VertexSize);
        uint32_t indexDataOffset = (m_IndexCount * (uint32_t)sizeof(uint32_t));
        // we DONT have to add the offset of the number of vertices to the
        // indices we have, because SDL3 is the goat and has a feature for that!
        std::memcpy((uint8_t *)m_IndexTransferBufferData + indexDataOffset,
                    kvp.second.indexData.data(), indexCount * sizeof(uint32_t));
        // uint8_t testBuffer[count * m_VertexSize];

        // std::memcpy(testBuffer, (uint8_t *)m_TransferBufferData + offset,
        //             count * m_VertexSize);
        // SpriteVertex *verts = (SpriteVertex *)testBuffer;
        // for (int i = 0; i < count; i++) {
        //     SpriteVertex v = verts[i];
        //     PX_TRACE("Sprite position: {}", v.position);
        //     PX_TRACE("Sprite color: {}", v.color);
        // }
        m_VertexCount += vertexCount;
        m_IndexCount += indexCount;
        kvp.second.clear(); // clears up the queue, but keeps the memory
                            // allocated for later use
    }
    while (unusedMaterials.size() > 0) {

        m_BindableBuffers.erase(unusedMaterials.front());
        unusedMaterials.pop();
    }
    // we now have a queue of batches to draw with the respective materials.
    Unmap();

    // we could skip here if there were no drawn vertices, but we still want to
    // do the renderpass if it clears the screen or something

    UploadToGPU(frameData.GPUCommandBuffer);
    m_VertexCount = 0;
    m_IndexCount = 0;

    // begin a render pass
    SDL_GPUDepthStencilTargetInfo *dsti = nullptr;
    if (m_HasDepthStencilTexture)
        dsti = &m_DepthStencilTargetInfo;

    SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(
        frameData.GPUCommandBuffer, m_ColorTargetInfos.data(),
        m_ColorTargetInfos.size(), dsti);

    SDL_GPUViewport vp = {};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.w = (float)m_Resolution.x; // must be the real texture width
    vp.h = (float)m_Resolution.y; // must be the real texture height
    // if swapchain is set, we grab this for you.
    if (TargetsSwapchain()) {
        vp.w = frameData.SwapchainSize.x;
        vp.h = frameData.SwapchainSize.y;
    }
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    SDL_SetGPUViewport(renderPass, &vp);

    Bind(renderPass); // bind the pipeline itself
    while (!batchesQueue.empty()) {
        BindableBatch &mb = batchesQueue.front();
        if (mb.bindable != nullptr)
            mb.bindable->Bind(frameData.GPUCommandBuffer, renderPass);
        SDL_DrawGPUIndexedPrimitives(renderPass, mb.indexCount, 1,
                                     mb.indexOffset, mb.vertexOffset, 0);
        batchesQueue.pop();
    }

    // end the render pass
    SDL_EndGPURenderPass(renderPass);
}

void Pipeline::SetVertexUniform(Ref<Uniform> uniform) {
    m_VertexUniform = uniform;
}
void Pipeline::SetFragmentUniform(Ref<Uniform> uniform) {
    m_FragmentUniform = uniform;
}

void Pipeline::SetResolution(const glm::ivec2 &resolution) {
    m_Resolution = resolution;
}
glm::ivec2 Pipeline::GetResolution() { return m_Resolution; }

bool Pipeline::UpdateColorTargetTexture(int slot, const Ref<Texture> &texture) {
    if (slot >= m_ColorTargetInfos.size()) {
        PX_WARN("tried updating color target at slot {} which doesn't exist",
                slot);
        return false;
    }
    m_ColorTargetInfos[slot].texture = texture->GetGPUTexture();
    return true;
}
void Pipeline::UpdateDepthStencilTargetTexture(const Ref<Texture> &texture) {
    m_DepthStencilTargetInfo.texture = texture->GetGPUTexture();
}

////////////////////////
/// HELPER FUNCTIONS ///
////////////////////////

void Pipeline::Bind(SDL_GPURenderPass *renderPass) {

    // bind the graphics pipeline
    SDL_BindGPUGraphicsPipeline(renderPass, m_GraphicsPipeline);

    // bind vertex buffer
    SDL_GPUBufferBinding vertexBufferBinding{
        .buffer = m_VertexBuffer, // index 0 is slot 0 in this example
        .offset = 0               // start from the first byte
    };
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding,
                             1); // bind one buffer starting from slot 0

    // bind index buffer
    SDL_GPUBufferBinding indexBufferBinding{.buffer = m_IndexBuffer,
                                            .offset = 0};
    SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);
}

bool Pipeline::Map() {

    auto device = Renderer::GetGPUDevice();
    m_VertexTransferBufferData =
        SDL_MapGPUTransferBuffer(device, m_VertexTransferBuffer,
                                 true); // cycling on
    PX_ASSERT(m_VertexTransferBufferData != nullptr,
              "Unable to map vertex transfer buffer: {}", SDL_GetError())
    m_IndexTransferBufferData =
        SDL_MapGPUTransferBuffer(device, m_IndexTransferBuffer,
                                 true); // cycling on
    PX_ASSERT(m_IndexTransferBufferData != nullptr,
              "Unable to map index transfer buffer: {}", SDL_GetError())
    return true;
}

void Pipeline::Unmap() {

    auto device = Renderer::GetGPUDevice();
    PX_ASSERT(m_VertexTransferBufferData != nullptr, "Unmapping unmapped!");
    SDL_UnmapGPUTransferBuffer(device, m_VertexTransferBuffer);
    m_VertexTransferBufferData = nullptr;
    PX_ASSERT(m_IndexTransferBufferData != nullptr, "Unmapping unmapped!");
    SDL_UnmapGPUTransferBuffer(device, m_IndexTransferBuffer);
    m_IndexTransferBufferData = nullptr;
}

void Pipeline::UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer) {
    if (m_VertexCount == 0 || m_IndexCount == 0)
        return;
    // Upload sprite data
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuffer);
    PX_ASSERT(copyPass != nullptr, "Failed to create a copy pass!");
    SDL_GPUBufferRegion vertexBufferRegion{.buffer = m_VertexBuffer,
                                           .offset = 0,
                                           .size =
                                               m_VertexCount * m_VertexSize};
    SDL_UploadToGPUBuffer(copyPass, &m_VertexTransferBufferLocation,
                          &vertexBufferRegion, true);
    SDL_GPUBufferRegion indexBufferRegion{.buffer = m_IndexBuffer,
                                          .offset = 0,
                                          .size = m_IndexCount *
                                                  (uint32_t)sizeof(uint32_t)};
    SDL_UploadToGPUBuffer(copyPass, &m_IndexTransferBufferLocation,
                          &indexBufferRegion, true);
    SDL_EndGPUCopyPass(copyPass);
}

} // namespace Pyxis
