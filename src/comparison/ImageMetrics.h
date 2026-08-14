#pragma once

#include "renderer/Bitmap.h"

#include <limits>
#include <optional>

namespace pdfforge {

struct ImageDiffStats {
    double mse = 0;
    double psnr = 0;
    double ssim = 0;
    int differentPixels = 0;
    int totalPixels = 0;
};

ImageDiffStats compareBitmaps(const Bitmap& original, const Bitmap& modified);

}  // namespace pdfforge
