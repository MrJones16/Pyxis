#pragma once
#include <Renderer/Texture.h>
#include <unordered_map>

namespace Pyxis {

struct Uniform {
    int size = 0;
    void *data = nullptr;

    Uniform(int _size, void *_data) {
        size = _size;
        data = malloc(size);
        std::memcpy(data, _data, size);
    }
    Uniform() {
        size = 0;
        data = nullptr;
    }
    template <typename UniformStruct> Uniform(UniformStruct uniformStruct) {
        size = sizeof(UniformStruct);
        data = malloc(size);
        std::memcpy(data, &uniformStruct, size);
    }
    ~Uniform() {
        if (data != nullptr) {
            free(data);
            size = 0;
        }
    }

    void UpdateData(int _size, void *_data) {
        if (data == nullptr) {
            size = _size;
            data = malloc(size);
            std::memcpy(data, _data, size);
        } else {
            if (size == _size) {
                // memcpy
                std::memcpy(data, _data, size);
            } else {
                free(data);
                size = _size;
                data = malloc(size);
                std::memcpy(data, _data, size);
            }
        }
    }
};

// Holds the set of textures and uniform info for grouping draw calls
// This will hold Refs to the textures provided, and has it's own
// data storage for the uniform data to be persistent
class Material : Bindable, public std::enable_shared_from_this<Material> {
  public:
  protected:
    std::unordered_map<uint8_t, Ref<Texture>> m_Textures;
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

    std::shared_ptr<Material> get_shared() { return shared_from_this(); }

  private:
    inline void Bind(SDL_GPUCommandBuffer *commandBuffer,
                     SDL_GPURenderPass *renderPass,
                     int defaultSlot = 0) override {
        // default slot is ignored for materials

        for (auto &kvp : m_Textures) {
            kvp.second->Bind(renderPass, kvp.first);
        }
        for (auto &uniform : m_UniformData)
            SDL_PushGPUFragmentUniformData(commandBuffer, uniform.first,
                                           uniform.second.data,
                                           uniform.second.size);
    }
};
} // namespace Pyxis
