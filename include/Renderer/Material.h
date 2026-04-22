#pragma once
#include <Renderer/Texture.h>

namespace Pyxis {

// Holds the set of textures and uniform info for grouping draw calls
// This will hold Refs to the textures provided, and has it's own
// data storage for the uniform data to be persistent
class Material {
  protected:
    std::unordered_map<uint8_t, Ref<Texture>> m_Textures;
    int m_UniformDataSize = 0;
    void *m_UniformData = nullptr;
    friend class Renderer;

  public:
    Material(int uniformDataSize) : m_UniformDataSize(uniformDataSize) {
        if (m_UniformDataSize > 0)
            m_UniformData = std::malloc(m_UniformDataSize);
    };

    template <typename UniformStruct>
    Material() : m_UniformDataSize(sizeof(UniformStruct)) {
        if (m_UniformDataSize > 0)
            m_UniformData = std::malloc(m_UniformDataSize);
    };

    inline ~Material() {
        if (m_UniformDataSize > 0)
            std::free(m_UniformData);
        m_Textures.clear();
    }

    inline void SetTexture(int slot, Ref<Texture> texture) {
        m_Textures[slot] = texture;
    }

    template <typename UniformStruct>
    inline void SetUniformData(UniformStruct uniformStruct) {
        PX_ASSERT(sizeof(UniformStruct) == m_UniformDataSize,
                  "Provided incorrect size of uniform data!");
        std::memcpy(m_UniformData, &uniformStruct, m_UniformDataSize);
    }
};
} // namespace Pyxis
