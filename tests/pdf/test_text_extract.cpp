#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <algorithm>
#include <filesystem>

namespace {

std::filesystem::path pdf(const char* name) {
    return std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / name;
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace

TEST(ExtractSimpleAmountText) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, pdf("TEST_01_SIMPLE_TEXT.pdf"));
    const auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    std::string joined;
    for (const auto& s : spans) {
        joined += s.text;
        joined += ' ';
        CHECK(s.fontSize > 0);
        CHECK(s.width > 0);
        CHECK(s.height > 0);
        CHECK(s.pdfCharStart >= 0);
    }
    CHECK(contains(joined, "TOTAL"));
    CHECK(contains(joined, "1.250,00"));
    CHECK(contains(joined, "PDFForge") || contains(doc->extractPlainText(0), "PDFForge"));
}

TEST(ExtractMultipleFonts) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, pdf("TEST_02_MULTIPLE_FONTS.pdf"));
    const auto spans = doc->extractText(0);
    CHECK(spans.size() >= 2);
    std::vector<std::string> fonts;
    for (const auto& s : spans) {
        if (!s.fontName.empty()) {
            fonts.push_back(s.fontName);
        }
    }
    CHECK(!fonts.empty());
}

TEST(ExtractBoldItalic) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, pdf("TEST_03_BOLD_ITALIC.pdf"));
    const auto spans = doc->extractText(0);
    bool anyBold = false;
    bool anyItalic = false;
    for (const auto& s : spans) {
        anyBold = anyBold || s.fontWeight >= 600;
        anyItalic = anyItalic || s.italic;
    }
    CHECK(anyBold);
    CHECK(anyItalic);
}

TEST(ExtractRotatedTextHasAngle) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, pdf("TEST_04_ROTATED_TEXT.pdf"));
    const auto spans = doc->extractText(0);
    bool angled = false;
    for (const auto& s : spans) {
        if (s.rotation > 10.0f) {
            angled = true;
        }
    }
    CHECK(angled);
    CHECK(contains(doc->extractPlainText(0), "ROTATED"));
}
