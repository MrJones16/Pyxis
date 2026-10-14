#pragma once
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace Pyxis {

// A rectangle of cells produced by BinaryGreedyMesh, in cell units.
// (x, y) is the bottom-left cell, (w, h) the size.
struct BGMRect {
    int x, y, w, h;
};

// Binary greedy meshing (ported from the old PyxisEngine
// Pyxis/Core/BinaryGreedyMesh.h).
//
// `columns` is a 2D bit mask stored as one uint64_t per column (so up to 64
// rows): bit y of columns[x] is cell (x, y), 1 = solid. LSB is the bottom.
// Each column's runs of 1s are grown upward, then extended to the right while
// the next columns contain the same run, producing few, large rectangles.
inline std::vector<BGMRect> BinaryGreedyMesh(std::span<const uint64_t> columns) {
    std::vector<BGMRect> rects;
    if (columns.empty())
        return rects;

    // work on a copy, bits are removed as they are meshed
    std::vector<uint64_t> map(columns.begin(), columns.end());
    const int count = (int)map.size();

    for (int x = 0; x < count; x++) {
        while (map[x] != 0) {
            // start of the first run of 1s and its height
            const int start = std::countr_zero(map[x]);
            const int height = std::countr_one(map[x] >> start);
            // mask with only this run's bits set
            const uint64_t mask =
                height >= 64 ? ~0ull : (((1ull << height) - 1ull) << start);
            map[x] &= ~mask;

            // extend to the right while the next column fully contains the run
            int width = 1;
            while (x + width < count && (map[x + width] & mask) == mask) {
                map[x + width] &= ~mask;
                width++;
            }
            rects.push_back({x, start, width, height});
        }
    }
    return rects;
}

} // namespace Pyxis
