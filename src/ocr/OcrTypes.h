#pragma once

#include "core/Cancellation.h"
#include "core/Geometry.h"

#include <string>
#include <vector>

namespace pdfforge {

struct OcrWord {
    std::string text;
    float confidence = 0;  // 0..100
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};

struct OcrLine {
    std::string text;
    float confidence = 0;
    RectF bounds;
    std::vector<OcrWord> words;
};

struct OcrParagraph {
    std::string text;
    RectF bounds;
    std::vector<OcrLine> lines;
};

struct OcrPageResult {
    std::string text;
    std::string language;
    std::vector<OcrWord> words;
    std::vector<OcrLine> lines;
    std::vector<OcrParagraph> paragraphs;
    float meanConfidence = 0;
};

struct OcrOptions {
    std::string language = "eng";  // ISO-like tesseract codes, + joined
    int dpi = 300;
    CancellationToken* cancel = nullptr;
};

class OcrEngine {
public:
    [[nodiscard]] static bool available();
    [[nodiscard]] static std::vector<std::string> supportedLanguages();

    OcrPageResult recognize(const class Bitmap& bitmap, const OcrOptions& options = {});
};

}  // namespace pdfforge
