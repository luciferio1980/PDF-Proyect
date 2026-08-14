#include "pdf/RegionRecognize.h"

#include "core/Error.h"
#include "core/Utf.h"
#include "fonts/FontMatcher.h"
#include "ocr/OcrEngine.h"
#include "pdf/PdfDocument.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace pdfforge {
namespace {

std::string trimCopy(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string collapseWs(std::string s) {
    std::string out;
    out.reserve(s.size());
    bool space = false;
    for (unsigned char c : s) {
        if (std::isspace(c)) {
            if (!space && !out.empty()) {
                out.push_back(' ');
                space = true;
            }
            continue;
        }
        out.push_back(static_cast<char>(c));
        space = false;
    }
    return out;
}

bool spanInBox(const TextSpan& span, const RectF& box) {
    const RectF b = span.bounds();
    if (b.empty() || !b.intersects(box)) {
        return false;
    }
    if (box.contains(b.x + b.width * 0.5f, b.y + b.height * 0.5f)) {
        return true;
    }
    const RectF hit = b.intersection(box);
    return hit.area() >= b.area() * 0.25f;
}

Bitmap cropBitmap(const Bitmap& src, int x, int y, int w, int h) {
    if (src.empty() || w < 1 || h < 1) {
        return {};
    }
    x = std::clamp(x, 0, src.width - 1);
    y = std::clamp(y, 0, src.height - 1);
    w = std::min(w, src.width - x);
    h = std::min(h, src.height - y);
    if (w < 1 || h < 1) {
        return {};
    }
    Bitmap out;
    out.width = w;
    out.height = h;
    out.stride = w * 4;
    out.bgra.resize(static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(h));
    for (int row = 0; row < h; ++row) {
        const auto* srcLine = src.bgra.data() +
                              static_cast<std::size_t>(y + row) * static_cast<std::size_t>(src.stride) +
                              static_cast<std::size_t>(x) * 4u;
        auto* dstLine = out.bgra.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(out.stride);
        std::copy(srcLine, srcLine + static_cast<std::size_t>(w) * 4u, dstLine);
    }
    return out;
}

Color sampleInk(const Bitmap& bmp) {
    if (bmp.empty()) {
        return Color::rgb(0, 0, 0);
    }
    long r = 0, g = 0, b = 0, n = 0;
    for (int y = 0; y < bmp.height; ++y) {
        for (int x = 0; x < bmp.width; ++x) {
            const auto* px = bmp.pixel(x, y);
            const int lum = (static_cast<int>(px[2]) * 3 + static_cast<int>(px[1]) * 6 +
                             static_cast<int>(px[0])) /
                            10;
            if (lum < 160) {
                r += px[2];
                g += px[1];
                b += px[0];
                ++n;
            }
        }
    }
    if (n == 0) {
        return Color::rgb(0, 0, 0);
    }
    return Color::fromBytes(static_cast<unsigned>(r / n), static_cast<unsigned>(g / n),
                            static_cast<unsigned>(b / n));
}

std::string pickOcrLanguage() {
    const char* env = std::getenv("TESSDATA_PREFIX");
    std::filesystem::path tess;
    if (env && *env) {
        tess = env;
    } else {
        const char* candidates[] = {
            "/usr/share/tesseract-ocr/5/tessdata",
            "/usr/share/tesseract-ocr/4.00/tessdata",
            "/usr/share/tessdata",
        };
        for (const char* c : candidates) {
            if (std::filesystem::exists(std::filesystem::path(c) / "eng.traineddata")) {
                tess = c;
                break;
            }
        }
    }
    const bool spa = std::filesystem::exists(tess / "spa.traineddata");
    const bool eng = std::filesystem::exists(tess / "eng.traineddata");
    if (spa && eng) {
        return "spa+eng";
    }
    if (spa) {
        return "spa";
    }
    return "eng";
}

void fillFontFromSpan(RegionRead& out, const TextSpan& span) {
    out.fontName = span.fontName;
    out.fontSize = span.fontSize > 0 ? span.fontSize : 12.0f;
    out.fontWeight = span.fontWeight;
    out.italic = span.italic;
    out.color = span.color;
    FontEstimate estimate;
    estimate.family = span.fontName;
    estimate.weight = span.fontWeight;
    estimate.italic = span.italic;
    estimate.serif = span.serif;
    estimate.size = span.fontSize;
    const auto matches = matchFonts(estimate, 1);
    if (!matches.empty()) {
        out.matchedFamily = matches.front().family;
    }
}

}  // namespace

RegionRead recognizeRegion(PdfDocument& document, int pageIndex, RectF pageRect) {
    RegionRead out;
    if (pageIndex < 0 || pageIndex >= document.pageCount() || pageRect.width < 1.0f ||
        pageRect.height < 1.0f) {
        return out;
    }
    out.bounds = pageRect;
    out.marquee = pageRect;
    const auto spans = document.extractText(pageIndex);
    for (const auto& span : spans) {
        if (spanInBox(span, pageRect)) {
            out.spans.push_back(span);
        }
    }
    std::sort(out.spans.begin(), out.spans.end(), [](const TextSpan& a, const TextSpan& b) {
        const float tol = std::max(a.fontSize, b.fontSize) * 0.55f;
        if (std::fabs(a.y - b.y) > tol) {
            return a.y > b.y;
        }
        return a.x < b.x;
    });

    std::ostringstream joined;
    for (std::size_t i = 0; i < out.spans.size(); ++i) {
        if (i > 0) {
            const float tol = std::max(out.spans[i - 1].fontSize, out.spans[i].fontSize) * 0.55f;
            joined << (std::fabs(out.spans[i].y - out.spans[i - 1].y) > tol ? '\n' : ' ');
        }
        joined << out.spans[i].text;
    }
    out.text = trimCopy(joined.str());

    if (!out.spans.empty()) {
        const TextSpan* best = &out.spans.front();
        float bestArea = best->bounds().area();
        for (const auto& span : out.spans) {
            const float area = span.bounds().area();
            if (area > bestArea) {
                best = &span;
                bestArea = area;
            }
        }
        fillFontFromSpan(out, *best);
        RectF unionBox = out.spans.front().bounds();
        for (const auto& span : out.spans) {
            unionBox = unionBox.united(span.bounds());
        }
        out.bounds = unionBox;
    }

    if (!out.text.empty() || !OcrEngine::available()) {
        return out;
    }

    constexpr float kDpi = 200.0f;
    RenderRequest req;
    req.pageIndex = pageIndex;
    req.dpi = kDpi;
    Bitmap page = document.render(req);
    PointF a;
    PointF b;
    if (!document.pageToDevice(pageIndex, req, pageRect.x, pageRect.y + pageRect.height, a) ||
        !document.pageToDevice(pageIndex, req, pageRect.x + pageRect.width, pageRect.y, b)) {
        return out;
    }
    const int x0 = static_cast<int>(std::floor(std::min(a.x, b.x)));
    const int y0 = static_cast<int>(std::floor(std::min(a.y, b.y)));
    const int x1 = static_cast<int>(std::ceil(std::max(a.x, b.x)));
    const int y1 = static_cast<int>(std::ceil(std::max(a.y, b.y)));
    Bitmap crop = cropBitmap(page, x0, y0, x1 - x0, y1 - y0);
    if (crop.empty()) {
        return out;
    }

    OcrEngine engine;
    OcrOptions opt;
    opt.language = pickOcrLanguage();
    opt.dpi = static_cast<int>(kDpi);
    opt.pageSegMode = (pageRect.width > pageRect.height * 3.0f) ? 7 : 6;
    OcrPageResult ocr;
    try {
        ocr = engine.recognize(crop, opt);
    } catch (const Error&) {
        return out;
    }
    out.text = collapseWs(trimCopy(ocr.text));
    out.usedOcr = !out.text.empty();
    out.color = sampleInk(crop);
    if (!ocr.words.empty()) {
        float h = 0;
        for (const auto& w : ocr.words) {
            h += w.height;
        }
        h /= static_cast<float>(ocr.words.size());
        out.fontSize = std::clamp(h * 72.0f / kDpi, 6.0f, 72.0f);
    } else {
        out.fontSize = std::clamp(pageRect.height * 0.8f, 8.0f, 36.0f);
    }
    out.fontName = "OCR";
    out.matchedFamily = "Helvetica";
    return out;
}

}  // namespace pdfforge
