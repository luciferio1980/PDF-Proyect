#include "pdf/PdfTextExtractor.h"

#include "core/Geometry.h"
#include "fonts/FontDetector.h"

#include "fpdf_text.h"

#include <cmath>
#include <cctype>

namespace pdfforge {
namespace {

bool isCombiningOrControl(char32_t ch) {
    return ch < 32 && ch != '\t' && ch != '\n' && ch != '\r';
}

bool sameStyle(const TextSpan& span, const std::string& font, float size, int weight, bool italic,
               const Color& color, float rotation) {
    return span.fontName == font && std::fabs(span.fontSize - size) < 0.15f &&
           span.fontWeight == weight && span.italic == italic &&
           std::fabs(span.color.c0 - color.c0) < 0.02f &&
           std::fabs(span.color.c1 - color.c1) < 0.02f &&
           std::fabs(span.color.c2 - color.c2) < 0.02f &&
           std::fabs(span.rotation - rotation) < 1.0f;
}

bool adjacent(const TextSpan& span, float x, float y, float width, float height,
              [[maybe_unused]] float rotation) {
    (void)width;
    const float fs = std::max(span.fontSize, 1.0f);
    float rot = span.rotation;
    while (rot < 0) {
        rot += 360.0f;
    }
    while (rot >= 360.0f) {
        rot -= 360.0f;
    }
    const bool vertical = (rot > 45.0f && rot < 135.0f) || (rot > 225.0f && rot < 315.0f);
    if (vertical) {
        const float across = std::fabs(x - span.x);
        const float along = std::min(std::fabs(y - (span.y + span.height)),
                                     std::fabs(span.y - (y + height)));
        return across < fs * 0.85f && along < fs * 1.75f;
    }
    const float gapX = x - (span.x + span.width);
    const float midA = span.y + span.height * 0.5f;
    const float midB = y + height * 0.5f;
    const float yTol = fs * 0.85f;
    return gapX > -fs * 0.4f && gapX < fs * 1.6f && std::fabs(midA - midB) < yTol;
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace

std::vector<TextSpan> extractTextSpans(FPDF_TEXTPAGE textPage, int pageIndex) {
    std::vector<TextSpan> spans;
    if (!textPage) {
        return spans;
    }
    const int count = FPDFText_CountChars(textPage);
    TextSpan current;
    bool open = false;

    for (int i = 0; i < count; ++i) {
        const unsigned int uni = FPDFText_GetUnicode(textPage, i);
        if (uni == 0 || isCombiningOrControl(static_cast<char32_t>(uni))) {
            continue;
        }
        if (uni == '\n' || uni == '\r') {
            if (open) {
                spans.push_back(std::move(current));
                current = {};
                open = false;
            }
            continue;
        }

        double left = 0, right = 0, bottom = 0, top = 0;
        if (!FPDFText_GetCharBox(textPage, i, &left, &right, &bottom, &top)) {
            continue;
        }
        const float x = static_cast<float>(left);
        const float y = static_cast<float>(bottom);
        const float w = static_cast<float>(right - left);
        const float h = static_cast<float>(top - bottom);
        const float fontSize = static_cast<float>(FPDFText_GetFontSize(textPage, i));
        int weight = FPDFText_GetFontWeight(textPage, i);
        if (weight <= 0) {
            weight = 400;
        }
        int flags = 0;
        char fontBuf[256];
        const unsigned long fontBytes =
            FPDFText_GetFontInfo(textPage, i, fontBuf, sizeof(fontBuf), &flags);
        std::string fontName;
        if (fontBytes > 1) {
            fontName = fontBuf;
        }
        unsigned r = 0, g = 0, b = 0, a = 255;
        Color color = Color::rgb(0, 0, 0);
        if (FPDFText_GetFillColor(textPage, i, &r, &g, &b, &a)) {
            color = Color::fromBytes(r, g, b, a);
        }
        float angleRad = FPDFText_GetCharAngle(textPage, i);
        if (angleRad < 0) {
            angleRad = 0;
        }
        const float rotation = radiansToDegrees(angleRad);
        const FontEstimate estimate = interpretPdfFontFlags(fontName, flags, weight);

        if (uni == ' ' || uni == '\t') {
            if (open) {
                current.text.push_back(uni == '\t' ? ' ' : ' ');
                current.pdfCharEnd = i + 1;
                current.width = std::max(current.width, x + w - current.x);
            }
            continue;
        }

        if (!open || !sameStyle(current, fontName, fontSize, estimate.weight, estimate.italic, color,
                                rotation) ||
            !adjacent(current, x, y, w, h, rotation)) {
            if (open) {
                spans.push_back(std::move(current));
            }
            current = TextSpan{};
            current.pageIndex = pageIndex;
            current.fontName = fontName;
            current.fontSize = fontSize;
            current.fontWeight = estimate.weight;
            current.italic = estimate.italic;
            current.serif = estimate.serif;
            current.color = color;
            current.x = x;
            current.y = y;
            current.width = w;
            current.height = h;
            current.baseline = y;
            current.rotation = rotation;
            current.pdfCharStart = i;
            current.pdfCharEnd = i + 1;
            current.text.clear();
            appendUtf8(current.text, static_cast<char32_t>(uni));
            open = true;
            continue;
        }

        appendUtf8(current.text, static_cast<char32_t>(uni));
        current.pdfCharEnd = i + 1;
        const float newRight = x + w;
        const float newTop = y + h;
        current.width = std::max(current.width, newRight - current.x);
        current.y = std::min(current.y, y);
        current.height = std::max(current.height, newTop - current.y);
        current.baseline = std::min(current.baseline, y);
    }

    if (open && !current.text.empty()) {
        spans.push_back(std::move(current));
    }
    return spans;
}

}  // namespace pdfforge
