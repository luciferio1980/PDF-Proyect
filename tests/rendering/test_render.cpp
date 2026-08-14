#include "comparison/ImageMetrics.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "renderer/PdfRenderer.h"
#include "tests/TestHarness.h"

#include <filesystem>

TEST(RenderProducesNonEmptyBitmap) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::RenderRequest req;
    req.pageIndex = 0;
    req.dpi = 72;
    const auto bmp = doc->render(req);
    CHECK(bmp.width > 100);
    CHECK(bmp.height > 100);
    CHECK(!bmp.bgra.empty());
    bool nonWhite = false;
    for (int y = 0; y < bmp.height && !nonWhite; ++y) {
        for (int x = 0; x < bmp.width; ++x) {
            const auto* p = bmp.pixel(x, y);
            if (p[0] < 250 || p[1] < 250 || p[2] < 250) {
                nonWhite = true;
                break;
            }
        }
    }
    CHECK(nonWhite);
}

TEST(RenderRespectsZoomAndRotation) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::RenderRequest a;
    a.dpi = 72;
    pdfforge::RenderRequest b;
    b.dpi = 144;
    const auto lo = doc->render(a);
    const auto hi = doc->render(b);
    CHECK(hi.width > lo.width);
    CHECK(hi.height > lo.height);

    pdfforge::RenderRequest rot;
    rot.dpi = 72;
    rot.rotationQuarterTurns = 1;
    const auto turned = doc->render(rot);
    CHECK(turned.width == lo.height);
    CHECK(turned.height == lo.width);
}

TEST(RenderCacheReturnsSamePixels) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::PdfRenderer renderer;
    pdfforge::RenderRequest req;
    req.dpi = 72;
    const auto first = renderer.renderCached(*doc, req);
    const auto second = renderer.renderCached(*doc, req);
    CHECK(first.width == second.width);
    CHECK(first.bgra == second.bgra);
}

TEST(RenderIdenticalPagesHaveHighPsnr) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::RenderRequest req;
    req.dpi = 72;
    const auto a = doc->render(req);
    const auto b = doc->render(req);
    const auto stats = pdfforge::compareBitmaps(a, b);
    CHECK(stats.differentPixels == 0);
    CHECK(stats.psnr >= 40);
    CHECK(stats.ssim > 0.99);
}
