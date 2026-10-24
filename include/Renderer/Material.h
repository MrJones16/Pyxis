#pragma once
#include "Renderer/StorageBuffer.h"
#include <Renderer/Texture.h>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Pyxis {

// A block of uniform data (bytes), e.g. a camera matrix. Owns a copy of the
// data in a std::vector, so it copies / moves / destroys safely (rule of
// zero).
struct Uniform {
    std::vector<uint8_t> m_Data;

    Uniform() = default;
    Uniform(int size, const void *data) { UpdateData(size, data); }
    // copies a plain struct (e.g. a glm::mat4 or a struct of floats)
    template <typename UniformStruct>
        requires(!std::is_same_v<std::decay_t<UniformStruct>, Uniform> &&
                 std::is_trivially_copyable_v<UniformStruct>)
    explicit Uniform(const UniformStruct &uniformStruct) {
        UpdateData(sizeof(UniformStruct), &uniformStruct);
    }

    void UpdateData(int size, const void *data) {
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        m_Data.assign(bytes, bytes + size);
    }
    inline const void *GetData() const { return m_Data.data(); }
    inline uint32_t GetSize() const { return (uint32_t)m_Data.size(); }
};

// Holds the set of textures and uniform info for grouping draw calls
// This will hold Refs to the textures provided, and has it's own
// data storage for the uniform data to be persistent
class Material : public Bindable {
  public:
  protected:
    std::unordered_map<uint8_t, Ref<Texture>> m_Textures;
    std::unordered_map<uint8_t, Ref<StorageBuffer>> m_StorageBuffers;
    std::unordered_map<uint8_t, Uniform> m_UniformData;
    friend class Renderer;

  public:
    Material() { m_Textures = std::unordered_map<uint8_t, Ref<Texture>>(); };

    inline ~Material() {
        m_Textures.clear();
        m_UniformData.clear();
    }

    inline void SetTexture(int slot, Ref<Texture> texture) {
        m_Textures[slot] = texture;
    }

    // gets the texture, or null if not set at that slot
    inline Ref<Texture> GetTexture(int slot) {
        if (m_Textures.contains(slot))
            return m_Textures[slot];
        else
            return nullptr;
    }

    // copy an object's data into a temp buffer to then be uploaded before
    // rendering
    template <typename UniformStruct>
    inline void SetUniformData(uint8_t slot, UniformStruct uniformStruct) {
        m_UniformData[slot] = Uniform(uniformStruct);
    }

  private:
    inline void Bind(SDL_GPUCommandBuffer *commandBuffer,
                     SDL_GPURenderPass *renderPass,
                     int defaultSlot = 0) override {
        // default slot is ignored for materials

        for (auto &&[slot, tex] : m_Textures) {
            tex->Bind(renderPass, slot);
        }
        for (auto &&[slot, buf] : m_StorageBuffers) {
            buf->Bind(renderPass, slot);
        }
        for (auto &uniform : m_UniformData)
            SDL_PushGPUFragmentUniformData(commandBuffer, uniform.first,
                                           uniform.second.GetData(),
                                           uniform.second.GetSize());
    }
};
} // namespace Pyxis
