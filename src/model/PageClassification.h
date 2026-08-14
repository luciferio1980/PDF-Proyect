#pragma once

#include <string>

namespace pdfforge {

enum class PageContentKind {
    Empty,
    TextOnly,
    ImageOnly,
    Mixed,
    ProbablyScanned,
};

enum class OcrNeed {
    NotNeeded,
    Required,
};

struct PageClassification {
    PageContentKind kind = PageContentKind::Empty;
    OcrNeed ocr = OcrNeed::NotNeeded;
    int textCharCount = 0;
    int imageCount = 0;
    float imageCoverage = 0;  // 0..1 of page area
    std::string reason;
};

inline const char* pageContentKindName(PageContentKind kind) {
    switch (kind) {
        case PageContentKind::Empty:
            return "empty";
        case PageContentKind::TextOnly:
            return "text-only";
        case PageContentKind::ImageOnly:
            return "image-only";
        case PageContentKind::Mixed:
            return "mixed";
        case PageContentKind::ProbablyScanned:
            return "probably-scanned";
    }
    return "unknown";
}

}  // namespace pdfforge
