#include "pdf/PdfDocument.h"
#include "pdf/PdfSearch.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <filesystem>

TEST(SearchFindsAmount) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    const auto spans = doc->extractText(0);
    pdfforge::SearchQuery q;
    q.needle = "1.250,00";
    const auto hits = pdfforge::searchSpans(spans, q);
    CHECK(!hits.empty());
    if (!hits.empty()) {
        CHECK(hits.front().pageIndex == 0);
    }
}

TEST(SearchCaseInsensitive) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::SearchQuery q;
    q.needle = "pdfforge";
    q.caseInsensitive = true;
    const auto hits = pdfforge::searchSpans(doc->extractText(0), q);
    CHECK(!hits.empty());
}
