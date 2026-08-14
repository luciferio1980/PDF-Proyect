#include "comparison/ImageMetrics.h"
#include "tests/TestHarness.h"

TEST(ImageMetricsIdentical) {
    pdfforge::Bitmap a;
    a.width = 8;
    a.height = 8;
    a.stride = 32;
    a.bgra.assign(32 * 8, 200);
    const auto stats = pdfforge::compareBitmaps(a, a);
    CHECK(stats.differentPixels == 0);
    CHECK(stats.psnr >= 40);
    CHECK(stats.ssim > 0.99);
}

TEST(ImageMetricsDetectsChange) {
    pdfforge::Bitmap a;
    a.width = 4;
    a.height = 1;
    a.stride = 16;
    a.bgra = {0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255};
    auto b = a;
    b.bgra[0] = 255;
    b.bgra[1] = 255;
    b.bgra[2] = 255;
    const auto stats = pdfforge::compareBitmaps(a, b);
    CHECK(stats.differentPixels == 1);
    CHECK(stats.psnr < 40);
}
