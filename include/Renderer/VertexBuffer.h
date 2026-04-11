#pragma once

#include <vector>
namespace Pyxis {
enum VertexAttributeType {
    VertexAttribute_Float,
    VertexAttribute_Float2,
    VertexAttribute_Float3,
    VertexAttribute_Float4,
};

class VertexBuffer {
  public:
    std::vector<VertexAttributeType> attributes;
    VertexBuffer(int slot, std::vector<VertexAttributeType> attributes);
};
} // namespace Pyxis
