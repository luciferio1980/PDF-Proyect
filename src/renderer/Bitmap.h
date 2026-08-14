#pragma once

#include "core/Cancellation.h"
#include "core/Geometry.h"

#include <cstdint>
#include <vector>

namespace pdfforge {

struct Bitmap {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> bgra;

    [[nodiscard]] bool empty() const { return width <= 0 || height <= 0 || bgra.empty(); }

    [[nodiscard]] const std::uint8_t* pixel(int x, int y) const {
        return bgra.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride) +
               static_cast<std::size_t>(x) * 4u;
    }
};

struct RenderRequest {
    int pageIndex = 0;
    float dpi = 96.0f;
    int rotationQuarterTurns = 0;  // extra 0..3 clockwise
    int clipX = 0;
    int clipY = 0;
    int clipWidth = 0;  // 0 means full page
    int clipHeight = 0;
};

}  // namespace pdfforge
