#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <filesystem>

namespace {

std::filesystem::path pdf(const char* name) {
    return std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / name;
}

}  // namespace

TEST(OpenSimpleTextDocument) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, pdf("TEST_01_SIMPLE_TEXT.pdf"));
    CHECK(doc != nullptr);
    CHECK(doc->pageCount() == 2);
    const auto size = doc->pageSize(0);
    CHECK(size.width > 600);
    CHECK(size.height > 700);
}

TEST(RejectMissingFile) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    bool threw = false;
    try {
        pdfforge::PdfDocument::open(runtime, pdf("DOES_NOT_EXIST.pdf"));
    } catch (const pdfforge::Error& ex) {
        threw = true;
        CHECK(ex.status() == pdfforge::Status::FileNotFound);
    }
    CHECK(threw);
}

TEST(OpenAllFixtures) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    const char* files[] = {
        "TEST_01_SIMPLE_TEXT.pdf",     "TEST_02_MULTIPLE_FONTS.pdf", "TEST_03_BOLD_ITALIC.pdf",
        "TEST_04_ROTATED_TEXT.pdf",    "TEST_05_SCANNED_DOCUMENT.pdf", "TEST_06_TABLE.pdf",
        "TEST_07_MULTICOLUMN.pdf",     "TEST_08_EMBEDDED_FONT.pdf",  "TEST_09_MIXED_CONTENT.pdf",
    };
    for (const char* f : files) {
        auto doc = pdfforge::PdfDocument::open(runtime, pdf(f));
        CHECK(doc->pageCount() >= 1);
    }
}
