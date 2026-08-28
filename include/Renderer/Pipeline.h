#pragma once

#include <Renderer/Material.h>
#include <Renderer/Renderer.h>

namespace Pyxis {

struct DrawBuffer {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indexData;
    void clear() {
        vertexData.clear();
        indexData.clear();
    }
};

//////////  PIPELINES  //////////
// Pipelines are the abstraction over
// using a set of shaders, buffers,
// textures, ect, and queue the draw calls
// using their respective shaders.
class Pipeline {
    friend class Renderer;

  protected:
    //////////////////////////////
    /// UNDERLYING MEMBER VARS ///
    //////////////////////////////
    SDL_GPUGraphicsPipeline *m_GraphicsPipeline;

    // uniforms that will be uploaded before render
    Ref<Uniform> m_VertexUniform = nullptr;
    Ref<Uniform> m_FragmentUniform = nullptr;

    // Depth & Stencil
    bool m_HasDepthStencilTexture = false;
    SDL_GPUDepthStencilTargetInfo m_DepthStencilTargetInfo{};

    // Vertex Buffer
    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    uint32_t m_VertexSize = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_MaxSize = 0;

    // Index Buffer
    SDL_GPUBuffer *m_IndexBuffer = nullptr;
    uint32_t m_MaxIndices = 0;
    uint32_t m_IndexCount = 0;

    // transfer buffers
    SDL_GPUTransferBuffer *m_VertexTransferBuffer;
    void *m_VertexTransferBufferData;
    SDL_GPUTransferBufferLocation m_VertexTransferBufferLocation;

    SDL_GPUTransferBuffer *m_IndexTransferBuffer;
    void *m_IndexTransferBufferData;
    SDL_GPUTransferBufferLocation m_IndexTransferBufferLocation;

    // queues for materials
    std::unordered_map<Ref<Bindable>, DrawBuffer> m_BindableBuffers;

    // output color targets
    std::vector<SDL_GPUColorTargetInfo> m_ColorTargetInfos;
    // whether or not we target screen output
    bool m_TargetSwapchain = false;

    // only applicable if you are not targeting swapchain.
    glm::ivec2 m_Resolution = {1920, 1080};

  public:
    // Main constructor
    Pipeline(uint32_t maxVertices, uint32_t vertexSize, uint32_t maxIndices,
             std::vector<SDL_GPUVertexAttribute> vertexAttributes,
             std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
             std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
             SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo,
             const std::string &vertexShaderPath,
             const std::string &fragmentShaderPath, bool TargetSwapchain);

    // for later if i want to make them shared maybe...
    //  static Ref<Pipeline> CreatePipeline(
    //      uint32_t maxVertices, uint32_t vertexSize, uint32_t maxIndices,
    //      std::vector<SDL_GPUVertexAttribute> vertexAttributes,
    //      std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
    //      std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
    //      SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo,
    //      const std::string &vertexShaderPath,
    //      const std::string &fragmentShaderPath, bool TargetSwapchain);

    ~Pipeline();

    //////////////////////
    /// MAIN FUNCTIONS ///
    //////////////////////

    // Actually draw the pipeline by binding, and going through the queue
    void Draw(Renderer::FrameData &frameData);

    // Queues a mesh to be drawn. You can use a Material, or just Texture
    // inline due to template
    template <typename VertexType>
    inline void QueueMesh(const std::vector<VertexType> &vertices,
                          const std::vector<uint32_t> &indices,
                          Ref<Bindable> bindable) {
        PX_ASSERT(sizeof(VertexType) == m_VertexSize,
                  "Drawing with incorrect vertex size!");
        // before copying vertices over, we need to know how many there are
        // first before
        uint32_t vertexCount =
            m_BindableBuffers[bindable].vertexData.size() / m_VertexSize;
        // copy vertex data
        uint8_t *bytes = (uint8_t *)vertices.data();
        m_BindableBuffers[bindable].vertexData.insert(
            m_BindableBuffers[bindable].vertexData.end(), bytes,
            bytes + (vertices.size() * m_VertexSize));

        // copy indices to material buffer
        auto &indexVector = m_BindableBuffers[bindable].indexData;
        for (auto i : indices) {
            // add vertex count to get correct index location
            indexVector.push_back(i + vertexCount);
        }
    }

    // Set the vertex uniform data that this pipeline uses
    void SetVertexUniform(Ref<Uniform> uniform);

    // Set the Fragment uniform data that this pipeline uses
    void SetFragmentUniform(Ref<Uniform> uniform);

    // Set the resolution this pipeline renders at
    void SetResolution(const glm::ivec2 &resolution);
    // Get the resolution this pipeline renders at
    glm::ivec2 GetResolution();

    // needed for when a texture gets resized, as the underlying gpu texture is
    // re-made
    bool UpdateColorTargetTexture(int slot, const Ref<Texture> &texture);
    // needed for when a texture gets resized, as the underlying gpu texture is
    // re-made
    void UpdateDepthStencilTargetTexture(const Ref<Texture> &texture);

    ////////////////////////
    /// HELPER FUNCTIONS ///
    ////////////////////////
  public:
    // Returns bool of if this pipeline uses the swapchain.
    inline bool TargetsSwapchain() { return m_TargetSwapchain; };

    inline std::vector<SDL_GPUColorTargetInfo> &GetColorTargets() {
        return m_ColorTargetInfos;
    }
    inline SDL_GPUDepthStencilTargetInfo &GetDepthStencilTarget() {
        return m_DepthStencilTargetInfo;
    }

  private:
    // Binds this pipeline
    void Bind(SDL_GPURenderPass *renderPass);
    // maps the transfer buffers to a place we can write to.
    bool Map();
    // unmap the transfer buffers
    void Unmap();

    void UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer);

  private:
};
} // namespace Pyxis
