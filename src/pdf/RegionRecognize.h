#pragma once

#include "model/Objects.h"

#include <string>
#include <vector>

namespace pdfforge {

class PdfDocument;

struct RegionRead {
    std::vector<TextSpan> spans;
    std::string text;
    std::string fontName;
    std::string matchedFamily;
    float fontSize = 12.0f;
    int fontWeight = 400;
    bool italic = false;
    Color color = Color::rgb(0, 0, 0);
    RectF bounds;
    RectF marquee;
    bool usedOcr = false;

    [[nodiscard]] bool empty() const { return text.empty() && spans.empty(); }
};

RegionRead recognizeRegion(PdfDocument& document, int pageIndex, RectF pageRect);

}  // namespace pdfforge
