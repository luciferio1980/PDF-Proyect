#include "core/SecureTemp.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <filesystem>

TEST(WriteCopyDoesNotTouchOriginalAndReopens) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    const auto src = std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf";
    auto original = pdfforge::PdfDocument::open(runtime, src);
    const auto originalText = original->extractPlainText(0);

    pdfforge::SecureTempFile tmp("pdfforge-copy");
    const auto dest = tmp.path().string() + ".pdf";
    original->writeCopy(dest);

    auto copy = pdfforge::PdfDocument::open(runtime, dest);
    CHECK(copy->pageCount() == original->pageCount());
    CHECK(copy->extractPlainText(0) == originalText);

    auto again = pdfforge::PdfDocument::open(runtime, src);
    CHECK(again->extractPlainText(0) == originalText);
    std::error_code ec;
    std::filesystem::remove(dest, ec);
}
