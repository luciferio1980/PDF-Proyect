#include "ocr/OcrEngine.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

TEST(OcrReportsAvailability) {
    if (!pdfforge::OcrEngine::available()) {
        return;
    }
    const auto langs = pdfforge::OcrEngine::supportedLanguages();
    CHECK(std::find(langs.begin(), langs.end(), "eng") != langs.end());
    CHECK(std::find(langs.begin(), langs.end(), "spa") != langs.end());
}

TEST(OcrOnRenderedTextPageFindsPdfForge) {
    if (!pdfforge::OcrEngine::available()) {
        return;
    }
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(
        runtime, std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / "TEST_01_SIMPLE_TEXT.pdf");
    pdfforge::RenderRequest req;
    req.dpi = 200;
    const auto bmp = doc->render(req);
    pdfforge::OcrEngine engine;
    pdfforge::OcrOptions opt;
    opt.language = "eng";
    opt.dpi = 200;
    const auto result = engine.recognize(bmp, opt);
    std::string folded = result.text;
    std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    CHECK(folded.find("PDFFORGE") != std::string::npos || folded.find("TOTAL") != std::string::npos);
    CHECK(!result.words.empty());
    CHECK(result.words.front().width > 0);
    CHECK(result.words.front().height > 0);
}
