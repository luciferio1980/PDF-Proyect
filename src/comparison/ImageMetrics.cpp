#include "comparison/ImageMetrics.h"

#include "core/Error.h"

#include <cmath>

namespace pdfforge {
namespace {

double luma(const std::uint8_t* bgra) {
    // BGRA byte order from PDFium.
    const double b = bgra[0];
    const double g = bgra[1];
    const double r = bgra[2];
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

}  // namespace

ImageDiffStats compareBitmaps(const Bitmap& original, const Bitmap& modified) {
    if (original.width != modified.width || original.height != modified.height) {
        throw Error(Status::InvalidArgument, "bitmap size mismatch");
    }
    ImageDiffStats stats;
    stats.totalPixels = original.width * original.height;
    if (stats.totalPixels <= 0) {
        return stats;
    }
    double se = 0;
    double sumO = 0, sumM = 0, sumO2 = 0, sumM2 = 0, sumOM = 0;
    for (int y = 0; y < original.height; ++y) {
        for (int x = 0; x < original.width; ++x) {
            const auto* a = original.bgra.data() +
                            static_cast<std::size_t>(y) * static_cast<std::size_t>(original.stride) +
                            static_cast<std::size_t>(x) * 4u;
            const auto* b = modified.bgra.data() +
                            static_cast<std::size_t>(y) * static_cast<std::size_t>(modified.stride) +
                            static_cast<std::size_t>(x) * 4u;
            const double la = luma(a);
            const double lb = luma(b);
            const double d = la - lb;
            se += d * d;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2]) {
                ++stats.differentPixels;
            }
            sumO += la;
            sumM += lb;
            sumO2 += la * la;
            sumM2 += lb * lb;
            sumOM += la * lb;
        }
    }
    const double n = static_cast<double>(stats.totalPixels);
    stats.mse = se / n;
    if (stats.mse <= 1e-12) {
        stats.psnr = 99.0;
    } else {
        stats.psnr = 10.0 * std::log10((255.0 * 255.0) / stats.mse);
    }
    const double meanO = sumO / n;
    const double meanM = sumM / n;
    const double varO = sumO2 / n - meanO * meanO;
    const double varM = sumM2 / n - meanM * meanM;
    const double cov = sumOM / n - meanO * meanM;
    constexpr double k1 = 0.01;
    constexpr double k2 = 0.03;
    constexpr double L = 255.0;
    const double c1 = (k1 * L) * (k1 * L);
    const double c2 = (k2 * L) * (k2 * L);
    stats.ssim = ((2 * meanO * meanM + c1) * (2 * cov + c2)) /
                 ((meanO * meanO + meanM * meanM + c1) * (varO + varM + c2));
    return stats;
}

}  // namespace pdfforge
