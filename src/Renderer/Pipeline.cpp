#include "Core/Core.h"
#include <Renderer/Pipeline.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {
Pipeline::Pipeline(SDL_GPUDevice *gpuDevice,
                   const PipelineDescription &description)
    : m_GPUDevice(gpuDevice), m_Description(description) {

    PX_BEGINSTEPS("Creating Pipeline");

    SDL_GPUShader *vertexShader = CompileShader(
        m_Description.vertexShaderPath, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);

    SDL_GPUShader *fragmentShader = CompileShader(
        m_Description.vertexShaderPath, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    //////////////// CREATE PIPELINE ////////////////
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = m_Description.primitiveType;

    // describe the vertex buffers
    SDL_GPUVertexBufferDescription vertexBufferDescription{};
    vertexBufferDescription.slot = 0;
    vertexBufferDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDescription.instance_step_rate = 0;
    vertexBufferDescription.pitch = description.vertexSize;
    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions =
        &vertexBufferDescription;
    pipelineInfo.vertex_input_state.num_vertex_attributes =
        (uint32_t)description.vertexAttributes.size();
    pipelineInfo.vertex_input_state.vertex_attributes =
        description.vertexAttributes.data();

    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions;
    for (const ColorTarget &target : description.colorTargets) {
        SDL_GPUColorTargetDescription colorTargetDescription{};
        colorTargetDescription.format = target.format;
        colorTargetDescription.blend_state = target.blend;
        colorTargetDescriptions.push_back(colorTargetDescription);
    }
    pipelineInfo.target_info.num_color_targets =
        (uint32_t)colorTargetDescriptions.size();
    pipelineInfo.target_info.color_target_descriptions =
        colorTargetDescriptions.data();

    if (description.depthTarget.hasDepthTarget) {
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format =
            description.depthTarget.format;
        SDL_GPUDepthStencilState depthStencilState{};
        depthStencilState.enable_depth_test =
            description.depthTarget.testEnabled;
        depthStencilState.enable_depth_write =
            description.depthTarget.writeEnabled;
        depthStencilState.enable_stencil_test = false;
        depthStencilState.compare_op = description.depthTarget.compareOp;
        depthStencilState.write_mask = 0xFF;
        pipelineInfo.depth_stencil_state = depthStencilState;
    }

    // create the pipeline
    m_GraphicsPipeline =
        SDL_CreateGPUGraphicsPipeline(m_GPUDevice, &pipelineInfo);

    // free shaders as well
    SDL_ReleaseGPUShader(m_GPUDevice, vertexShader);
    SDL_ReleaseGPUShader(m_GPUDevice, fragmentShader);
    PX_ASSERT(m_GraphicsPipeline != nullptr,
              "Failed to create graphics pipeline: {}", SDL_GetError())
}

Pipeline::~Pipeline() {
    auto device = Renderer::GetGPUDevice();
    SDL_ReleaseGPUGraphicsPipeline(device, m_GraphicsPipeline);
}

SDL_GPUShader *Pipeline::CompileShader(const std::string &shaderPath,
                                       SDL_ShaderCross_ShaderStage type) {

    // first, load HLSL shader
    size_t hlslCodeSize;
    void *hlslCode = SDL_LoadFile(shaderPath.c_str(), &hlslCodeSize);

    PX_ASSERT(hlslCode != nullptr, "Unable to load HLSL Shader file {} : {}",
              shaderPath, SDL_GetError())

    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.shader_stage = type;
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
    spirvInfo.shader_stage = type;
    spirvInfo.bytecode = (Uint8 *)spirvCode;
    spirvInfo.bytecode_size = spirvCodeSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.props = 0;

    SDL_ShaderCross_GraphicsShaderMetadata *metaDataVertex =
        SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8 *)spirvCode, spirvCodeSize,
                                             0);
    PX_ASSERT(metaDataVertex != nullptr,
              "Unable to reflect vertex shader metadata: {}", SDL_GetError());

    SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        m_GPUDevice, &spirvInfo, &metaDataVertex->resource_info, 0);
    PX_ASSERT(shader != nullptr,
              "Unable to compile vertex shader into GPUShader: {}",
              SDL_GetError());

    SDL_free(metaDataVertex);
    SDL_free(spirvCode);
    return shader;
}

} // namespace Pyxis
