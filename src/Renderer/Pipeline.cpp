#include "Core/Core.h"
#include <Renderer/Pipeline.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

Pipeline::Pipeline(
    SDL_GPUDevice *device, uint32_t maxVertices, uint32_t vertexSize,
    std::vector<SDL_GPUVertexAttribute> vertexAttributes,
    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
    std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    bool TargetsSwapchain)
    : m_Device(device), m_VertexSize(vertexSize),
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
    PX_STEPSUCCESS("Loaded HLSL file");

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
    SDL_GPUBufferCreateInfo bufferInfo{};
    m_MaxSize = maxVertices * m_VertexSize;
    bufferInfo.size = m_MaxSize;
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_VertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);
    if (m_VertexBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create vertex buffer! {}", SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created vertex buffer");

    //////////////// CREATE TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = m_MaxSize;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_TransferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    // Setup buffer location as well to be used later
    m_TransferBufferLocation.transfer_buffer = m_TransferBuffer;
    m_TransferBufferLocation.offset = 0;
    if (m_TransferBuffer == nullptr) {
        PX_STEPFAILURE("Failed to create transfer buffer! {}", SDL_GetError());
        m_Status = -1;
        return;
    }
    PX_STEPSUCCESS("Created transfer buffer");

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
    PX_STEPSUCCESS("Created graphipcs pipeline");
    PX_ENDSTEPS();
    m_Status = 0; // success
}

bool Pipeline::Map() {
    m_TransferBufferData = SDL_MapGPUTransferBuffer(m_Device, m_TransferBuffer,
                                                    true); // cycling on
    if (m_TransferBufferData == nullptr) {
        PX_ERROR("Unable to map transfer buffer: {}", SDL_GetError());
        return false;
    }
    return true;
}

void Pipeline::Unmap() {
    SDL_UnmapGPUTransferBuffer(m_Device, m_TransferBuffer);
    m_TransferBufferData = nullptr;
}

void Pipeline::QueueVertices(void *vertices, uint32_t count) {
    // this should only be called after the renderer's frame was begun so
    // that it's mapped.
    if (m_TransferBufferData == nullptr) {
        PX_ERROR("Tried to queue vertices on a pipeline that's not mapped!");
        return;
    } else if ((count + m_VertexCount) * m_VertexSize > m_MaxSize) {
        PX_ERROR("Unable to queue more vertices, limit reached!");
        return;
    }
    std::memcpy(&m_TransferBufferData + (m_VertexCount * m_VertexSize),
                vertices, count * m_VertexSize);
    m_VertexCount += count;
}

void Pipeline::UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer) {
    // Upload sprite data
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuffer);
    SDL_GPUBufferRegion bufferRegion{.buffer = m_VertexBuffer,
                                     .offset = 0,
                                     .size = m_VertexCount * m_VertexSize};
    SDL_UploadToGPUBuffer(copyPass, &m_TransferBufferLocation, &bufferRegion,
                          true);
    SDL_EndGPUCopyPass(copyPass);
}

void Pipeline::Bind(SDL_GPURenderPass *renderPass) {

    // bind the graphics pipeline
    SDL_BindGPUGraphicsPipeline(renderPass, m_GraphicsPipeline);

    // bind vertex buffer
    SDL_GPUBufferBinding bufferBindings[1];
    bufferBindings[0].buffer =
        m_VertexBuffer;           // index 0 is slot 0 in this example
    bufferBindings[0].offset = 0; // start from the first byte

    SDL_BindGPUVertexBuffers(renderPass, 0, bufferBindings,
                             1); // bind one buffer starting from slot 0
}

void Pipeline::Draw(SDL_GPURenderPass *renderPass) {

    // issue a draw call
    SDL_DrawGPUPrimitives(renderPass, m_VertexCount, 1, 0, 0);
    // reset vertex count
    m_VertexCount = 0;
}

} // namespace Pyxis
