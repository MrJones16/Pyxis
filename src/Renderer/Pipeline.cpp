#include <Renderer/Pipeline.h>
#include <SDL3/SDL_gpu.h>

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

    //////////////// LOAD SHADERS ////////////////
    // load the vertex shader code
    size_t vertexCodeSize;
    void *vertexCode = SDL_LoadFile(vertexShaderPath.c_str(), &vertexCodeSize);

    if (vertexCode == nullptr) {
        PX_ERROR("Unable to load Vertex Shader file {} : {}", vertexShaderPath,
                 SDL_GetError());
        m_Status = -1;
        return;
    }

    // create the vertex shader
    SDL_GPUShaderCreateInfo vertexInfo{};
    vertexInfo.code = (Uint8 *)vertexCode; // convert to an array of bytes
    vertexInfo.code_size = vertexCodeSize;
    vertexInfo.entrypoint = "main";
    vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV; // loading .spv shaders
    vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;  // vertex shader
    vertexInfo.num_samplers = 0;
    vertexInfo.num_storage_buffers = 0;
    vertexInfo.num_storage_textures = 0;
    vertexInfo.num_uniform_buffers = 0;
    SDL_GPUShader *vertexShader = SDL_CreateGPUShader(device, &vertexInfo);

    if (vertexShader == nullptr) {
        PX_ERROR("Unable to create GPU vertex shader {} : {}", vertexShaderPath,
                 SDL_GetError());
        m_Status = -1;
        return;
    }

    // free the file
    SDL_free(vertexCode);

    // create the fragment shader
    size_t fragmentCodeSize;
    void *fragmentCode =
        SDL_LoadFile(fragmentShaderPath.c_str(), &fragmentCodeSize);

    if (fragmentCode == nullptr) {
        PX_ERROR("Unable to load Fragment Shader file {} : {}",
                 fragmentShaderPath, SDL_GetError());
        m_Status = -1;
        return;
    }

    // create the fragment shader
    SDL_GPUShaderCreateInfo fragmentInfo{};
    fragmentInfo.code = (Uint8 *)fragmentCode;
    fragmentInfo.code_size = fragmentCodeSize;
    fragmentInfo.entrypoint = "main";
    fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT; // fragment shader
    fragmentInfo.num_samplers = 0;
    fragmentInfo.num_storage_buffers = 0;
    fragmentInfo.num_storage_textures = 0;
    fragmentInfo.num_uniform_buffers = 0;

    SDL_GPUShader *fragmentShader = SDL_CreateGPUShader(device, &fragmentInfo);

    if (fragmentShader == nullptr) {
        PX_ERROR("Unable to create GPU Fragment Shader {} : {}",
                 vertexShaderPath, SDL_GetError());
        m_Status = -1;
        return;
    }

    // free the file
    SDL_free(fragmentCode);

    //////////////// CREATE VERTEX BUFFER ////////////////
    SDL_GPUBufferCreateInfo bufferInfo{};
    m_MaxSize = maxVertices * m_VertexSize;
    bufferInfo.size = m_MaxSize;
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_VertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);

    //////////////// CREATE TRANSFER BUFFER ////////////////
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = m_MaxSize;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_TransferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    // Setup buffer location as well to be used later
    m_TransferBufferLocation.transfer_buffer = m_TransferBuffer;
    m_TransferBufferLocation.offset = 0;

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

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);
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
