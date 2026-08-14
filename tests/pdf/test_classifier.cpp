#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <filesystem>

TEST(ClassifyTextOnlyPage) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    const auto cls = doc->classifyPage(0);
    CHECK(cls.kind == pdfforge::PageContentKind::TextOnly);
    CHECK(cls.ocr == pdfforge::OcrNeed::NotNeeded);
    CHECK(cls.textCharCount >= 12);
}

TEST(ClassifyScannedPageRequiresOcr) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_05_SCANNED_DOCUMENT.pdf");
    const auto cls = doc->classifyPage(0);
    CHECK(cls.ocr == pdfforge::OcrNeed::Required);
    CHECK(cls.kind == pdfforge::PageContentKind::ProbablyScanned ||
          cls.kind == pdfforge::PageContentKind::ImageOnly);
    CHECK(cls.imageCount >= 1);
}

TEST(ClassifyMixedContent) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_09_MIXED_CONTENT.pdf");
    const auto cls = doc->classifyPage(0);
    CHECK(cls.kind == pdfforge::PageContentKind::Mixed ||
          cls.kind == pdfforge::PageContentKind::TextOnly);
}
