#include "pdf/PageClassifier.h"

#include "fpdf_edit.h"
#include "fpdf_text.h"

#include <algorithm>
#include <cmath>

namespace pdfforge {

PageClassification classifyLoadedPage(FPDF_PAGE page, FPDF_TEXTPAGE text, SizeF pageSize) {
    PageClassification result;
    if (text) {
        const int chars = FPDFText_CountChars(text);
        for (int i = 0; i < chars; ++i) {
            const unsigned uni = FPDFText_GetUnicode(text, i);
            if (uni > 32 && uni != 0xA0) {
                ++result.textCharCount;
            }
        }
    }

    float imageArea = 0;
    if (page) {
        const int count = FPDFPage_CountObjects(page);
        for (int i = 0; i < count; ++i) {
            FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
            if (!obj) {
                continue;
            }
            if (FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_IMAGE) {
                ++result.imageCount;
                float left = 0, bottom = 0, right = 0, top = 0;
                if (FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top)) {
                    imageArea += std::max(0.0f, right - left) * std::max(0.0f, top - bottom);
                }
            }
        }
    }

    const float pageArea = std::max(1.0f, pageSize.width * pageSize.height);
    result.imageCoverage = imageArea / pageArea;
    const bool significantText = result.textCharCount >= 12;
    const bool largeImage = result.imageCoverage >= 0.55f && result.imageCount >= 1;

    if (!significantText && largeImage) {
        result.kind = PageContentKind::ProbablyScanned;
        result.ocr = OcrNeed::Required;
        result.reason = "little or no extractable text and a large image covering the page";
    } else if (!significantText && result.imageCount > 0) {
        result.kind = PageContentKind::ImageOnly;
        result.ocr = OcrNeed::Required;
        result.reason = "images present without significant text";
    } else if (significantText && result.imageCount > 0) {
        result.kind = PageContentKind::Mixed;
        result.ocr = OcrNeed::NotNeeded;
        result.reason = "text and images";
    } else if (significantText) {
        result.kind = PageContentKind::TextOnly;
        result.ocr = OcrNeed::NotNeeded;
        result.reason = "extractable text present";
    } else {
        result.kind = PageContentKind::Empty;
        result.ocr = OcrNeed::NotNeeded;
        result.reason = "no significant text or images";
    }
    return result;
}

}  // namespace pdfforge
