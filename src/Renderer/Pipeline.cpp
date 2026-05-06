#include "Core/Core.h"
#include <Renderer/Pipeline.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <queue>

namespace Pyxis {

Pipeline::Pipeline(
    SDL_GPUDevice *device, uint32_t maxVertices, uint32_t vertexSize,
    uint32_t maxIndices, std::vector<SDL_GPUVertexAttribute> vertexAttributes,
    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
    std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    bool TargetsSwapchain)
    : m_Device(device), m_VertexSize(vertexSize), m_MaxIndices(maxIndices),
      m_ColorTargetInfos(colorTargetInfos),
      m_TargetSwapchain(TargetsSwapchain) {

    PX_BEGINSTEPS("Creating Pipeline");

    //////////////// LOAD VERTEX SHADER ////////////////

    // first, load HLSL shader
    size_t hlslCodeSize;
    void *hlslCode = SDL_LoadFile(vertexShaderPath.c_str(), &hlslCodeSize);

    if (hlslCode == nullptr) {
        PX_STEPFAILURE("Unable to load HLSL Shader file {} : {}",
                       vertexShaderPath, SDL_GetError());
        m_Status = -1;
        return;
    }
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

    if (spirvCode == nullptr) {
        PX_STEPFAILURE("Unable to compile vertex shader into SPIRV: {}",
                       SDL_GetError());
        m_Status = -1;
        return;
    }

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
    if (metaDataVertex == nullptr) {
        PX_STEPFAILURE("Unable to refelct vertex shader metadata: {}",
                       SDL_GetError());
        m_Status = -1;
        SDL_free(spirvCode);
        return;
    }
    PX_STEPSUCCESS("Reflected vertex shader metadata");

    SDL_GPUShader *vertexShader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
            m_Device, &spirvInfo, &metaDataVertex->resource_info, 0);

    if (vertexShader == nullptr) {
        PX_STEPFAILURE("Unable to compile vertex shader into GPUShader: {}",
                       SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Compiled GPU Vertex Shader");

    SDL_free(metaDataVertex);
    SDL_free(spirvCode);

    // At this point, vertex shader is created. If we exit early, we must still
    // free that!

    //////////////// LOAD FRAGMENT SHADER ////////////////

    // first, load HLSL shader
    hlslCode = SDL_LoadFile(fragmentShaderPath.c_str(), &hlslCodeSize);

    if (hlslCode == nullptr) {
        PX_STEPFAILURE("Unable to load HLSL Shader file {} : {}",
                       fragmentShaderPath, SDL_GetError());
        m_Status = -1;
        SDL_ReleaseGPUShader(device, vertexShader);
        return;
    }
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

    if (spirvCode == nullptr) {
        PX_STEPFAILURE("Unable to compile vertex shader into SPIRV: {}",
                       SDL_GetError());
        m_Status = -1;
        SDL_ReleaseGPUShader(device, vertexShader);
        return;
    }

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
    if (metaDataVertex == nullptr) {
        PX_STEPFAILURE("Unable to refelct fragment shader metadata: {}",
                       SDL_GetError());
        m_Status = -1;
        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_free(spirvCode);
        return;
    }
    PX_STEPSUCCESS("Reflected fragment shader metadata");

    SDL_GPUShader *fragmentShader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
            m_Device, &spirvInfo, &metaDataFragment->resource_info, 0);

    if (fragmentShader == nullptr) {
        PX_STEPFAILURE("Unable to compile fragment shader into GPUShader: {}",
                       SDL_GetError());
        SDL_ReleaseGPUShader(device, vertexShader);
        m_Status = -1;
        return;
    }
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
    if (m_VertexBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create vertex buffer! {}", SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created vertex buffer");

    //////////////// CREATE INDEX BUFFER ////////////////
    SDL_GPUBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.size = m_MaxIndices * sizeof(uint32_t);
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    m_IndexBuffer = SDL_CreateGPUBuffer(device, &indexBufferInfo);
    if (m_IndexBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create index buffer! {}", SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created index buffer");

    //////////////// CREATE VERTEX TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo vtbInfo{};
    vtbInfo.size = m_MaxSize;
    vtbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_VertexTransferBuffer = SDL_CreateGPUTransferBuffer(device, &vtbInfo);
    // Setup buffer location as well to be used later
    m_VertexTransferBufferLocation.transfer_buffer = m_VertexTransferBuffer;
    m_VertexTransferBufferLocation.offset = 0;
    if (m_VertexTransferBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create vertex transfer buffer! {}",
                       SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created vertex transfer buffer");

    //////////////// CREATE INDEX TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo itbInfo{};
    itbInfo.size = m_MaxIndices * sizeof(uint32_t);
    itbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_IndexTransferBuffer = SDL_CreateGPUTransferBuffer(device, &itbInfo);
    // Setup buffer location as well to be used later
    m_IndexTransferBufferLocation.transfer_buffer = m_IndexTransferBuffer;
    m_IndexTransferBufferLocation.offset = 0;
    if (m_IndexTransferBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create index transfer buffer! {}",
                       SDL_GetError());
        m_Status = -1;
        return;
    }
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

    // create the pipeline
    m_GraphicsPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    // free shaders as well
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    if (m_GraphicsPipeline == nullptr) {
        PX_ERROR("Failed to create graphics pipeline: {}", SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created graphics pipeline");

    PX_ENDSTEPS();
    m_Status = 0; // success
}

bool Pipeline::Map() {
    m_VertexTransferBufferData =
        SDL_MapGPUTransferBuffer(m_Device, m_VertexTransferBuffer,
                                 true); // cycling on
    if (m_VertexTransferBufferData == nullptr) {
        PX_ERROR("Unable to map vertex transfer buffer: {}", SDL_GetError());
        return false;
    }
    m_IndexTransferBufferData =
        SDL_MapGPUTransferBuffer(m_Device, m_IndexTransferBuffer,
                                 true); // cycling on
    if (m_IndexTransferBufferData == nullptr) {
        PX_ERROR("Unable to map index transfer buffer: {}", SDL_GetError());
        SDL_UnmapGPUTransferBuffer(m_Device, m_VertexTransferBuffer);
        m_VertexTransferBufferData = nullptr;
        return false;
    }
    return true;
}

void Pipeline::Unmap() {
    PX_ASSERT(m_VertexTransferBufferData != nullptr, "Unmapping unmapped!");
    SDL_UnmapGPUTransferBuffer(m_Device, m_VertexTransferBuffer);
    m_VertexTransferBufferData = nullptr;
    PX_ASSERT(m_IndexTransferBufferData != nullptr, "Unmapping unmapped!");
    SDL_UnmapGPUTransferBuffer(m_Device, m_IndexTransferBuffer);
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

void Pipeline::Draw(SDL_GPUCommandBuffer *commandBuffer, SDL_Window *window,
                    SDL_GPUTexture *swapchainTexture) {
    if (TargetsSwapchain()) {
        if (swapchainTexture == nullptr) {
            PX_WARN("Couldn't get swapchain! clearing pipeline queue and "
                    "skipping.");
            for (auto &kvp : m_MaterialBuffers) {
                kvp.second.clear(); // clear verts and indices
            }
            return;
        }
        m_ColorTargetInfos[0].texture = swapchainTexture;
    }
    struct MaterialBatch {
        Ref<Material> material;
        uint32_t vertexOffset; // in vertices not size
        uint32_t vertexCount;
        uint32_t indexOffset;
        uint32_t indexCount;
        MaterialBatch(Ref<Material> mat, uint32_t vo, uint32_t vc, uint32_t io,
                      uint32_t ic)
            : material(mat), vertexOffset(vo), vertexCount(vc), indexOffset(io),
              indexCount(ic) {}
    };
    std::queue<MaterialBatch> batchesQueue;
    std::queue<Ref<Material>> unusedMaterials;

    Map();
    // we need to add all the grouped materials into the one big vertex buffer
    for (auto &kvp : m_MaterialBuffers) {
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
        batchesQueue.push(MaterialBatch(kvp.first, m_VertexCount, vertexCount,
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

        m_MaterialBuffers.erase(unusedMaterials.front());
        unusedMaterials.pop();
    }
    // we now have a queue of batches to draw with the respective materials.
    Unmap();

    // we could skip here if there were no drawn vertices, but we still want to
    // do the renderpass if it clears the screen or something

    UploadToGPU(commandBuffer);
    m_VertexCount = 0;
    m_IndexCount = 0;

    // begin a render pass
    SDL_GPURenderPass *renderPass =
        SDL_BeginGPURenderPass(commandBuffer, m_ColorTargetInfos.data(),
                               m_ColorTargetInfos.size(), NULL);

    Bind(renderPass); // bind the pipeline itself
    while (!batchesQueue.empty()) {
        MaterialBatch &mb = batchesQueue.front();
        if (mb.material != nullptr)
            mb.material->Bind(commandBuffer, renderPass);
        SDL_DrawGPUIndexedPrimitives(renderPass, mb.indexCount, 1,
                                     mb.indexOffset, mb.vertexOffset, 0);
        batchesQueue.pop();
    }

    // end the render pass
    SDL_EndGPURenderPass(renderPass);
}

} // namespace Pyxis
