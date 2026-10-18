#include <Renderer/PipelineState.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

// Loads an HLSL file and compiles it for the GPU device (HLSL -> SPIR-V ->
// the device's format). Returns nullptr (logged) on failure.
static SDL_GPUShader *CompileShader(SDL_GPUDevice *device,
                                    const std::string &path,
                                    SDL_ShaderCross_ShaderStage stage) {
    size_t hlslCodeSize = 0;
    void *hlslCode = SDL_LoadFile(path.c_str(), &hlslCodeSize);
    if (!PX_VERIFY(hlslCode != nullptr, "Unable to load HLSL shader file {}: {}",
                   path, SDL_GetError()))
        return nullptr;

    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.shader_stage = stage;
    hlslInfo.entrypoint = "main";
    hlslInfo.include_dir = nullptr;
    hlslInfo.props = 0;
    hlslInfo.source = (char *)hlslCode;

    size_t spirvCodeSize = 0;
    void *spirvCode =
        SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvCodeSize);
    SDL_free(hlslCode);
    if (!PX_VERIFY(spirvCode != nullptr,
                   "Unable to compile shader {} into SPIR-V: {}", path,
                   SDL_GetError()))
        return nullptr;

    SDL_ShaderCross_GraphicsShaderMetadata *metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8 *)spirvCode, spirvCodeSize,
                                             0);
    if (!PX_VERIFY(metadata != nullptr,
                   "Unable to reflect shader metadata of {}: {}", path,
                   SDL_GetError())) {
        SDL_free(spirvCode);
        return nullptr;
    }

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.shader_stage = stage;
    spirvInfo.bytecode = (Uint8 *)spirvCode;
    spirvInfo.bytecode_size = spirvCodeSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.props = 0;
    SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirvInfo, &metadata->resource_info, 0);
    SDL_free(metadata);
    SDL_free(spirvCode);
    PX_VERIFY(shader != nullptr, "Unable to create GPU shader from {}: {}",
              path, SDL_GetError());
    return shader;
}

PipelineState::PipelineState(const PipelineStateDescription &description)
    : m_Description(description) {
    PX_BEGINSTEPS("Creating pipeline state {}\n - Vertex Shader: {}\n - "
                  "Fragment Shader: {}",
                  description.name, description.vertexShaderPath,
                  description.fragmentShaderPath);
    SDL_GPUDevice *device = Renderer::GetGPUDevice();

    SDL_GPUShader *vertexShader =
        CompileShader(device, description.vertexShaderPath,
                      SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragmentShader =
        CompileShader(device, description.fragmentShaderPath,
                      SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    if (vertexShader != nullptr && fragmentShader != nullptr) {
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.primitive_type = description.primitiveType;

        SDL_GPUVertexBufferDescription vertexBufferDescription{};
        vertexBufferDescription.slot = 0;
        vertexBufferDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescription.instance_step_rate = 0;
        vertexBufferDescription.pitch = description.vertexStride;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions =
            &vertexBufferDescription;
        pipelineInfo.vertex_input_state.num_vertex_attributes =
            (uint32_t)description.vertexAttributes.size();
        pipelineInfo.vertex_input_state.vertex_attributes =
            description.vertexAttributes.data();

        std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions;
        for (const PipelineColorTarget &target : description.colorTargets) {
            SDL_GPUColorTargetDescription colorTargetDescription{};
            colorTargetDescription.format = target.format;
            colorTargetDescription.blend_state = target.blend;
            colorTargetDescriptions.push_back(colorTargetDescription);
        }
        pipelineInfo.target_info.num_color_targets =
            (uint32_t)colorTargetDescriptions.size();
        pipelineInfo.target_info.color_target_descriptions =
            colorTargetDescriptions.data();

        if (description.depth.hasDepthTarget) {
            pipelineInfo.target_info.has_depth_stencil_target = true;
            pipelineInfo.target_info.depth_stencil_format =
                description.depth.format;
            SDL_GPUDepthStencilState depthStencilState{};
            depthStencilState.enable_depth_test = description.depth.testEnabled;
            depthStencilState.enable_depth_write =
                description.depth.writeEnabled;
            depthStencilState.enable_stencil_test = false;
            depthStencilState.compare_op = description.depth.compareOp;
            depthStencilState.write_mask = 0xFF;
            pipelineInfo.depth_stencil_state = depthStencilState;
        }

        m_GraphicsPipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        PX_VERIFY(m_GraphicsPipeline != nullptr,
                  "Failed to create graphics pipeline {}: {}", description.name,
                  SDL_GetError());
    }

    // the pipeline keeps what it needs; the shader objects can go
    if (vertexShader != nullptr)
        SDL_ReleaseGPUShader(device, vertexShader);
    if (fragmentShader != nullptr)
        SDL_ReleaseGPUShader(device, fragmentShader);
}

PipelineState::~PipelineState() { Release(); }

void PipelineState::Release() {
    if (m_GraphicsPipeline != nullptr && Renderer::GetGPUDevice() != nullptr)
        SDL_ReleaseGPUGraphicsPipeline(Renderer::GetGPUDevice(),
                                       m_GraphicsPipeline);
    m_GraphicsPipeline = nullptr;
}

} // namespace Pyxis
