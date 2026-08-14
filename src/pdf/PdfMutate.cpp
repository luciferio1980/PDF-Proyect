#include "pdf/PdfDocument.h"

#include "core/Logger.h"
#include "core/Utf.h"
#include "pdf/PdfiumRuntime.h"

#include "fpdf_edit.h"
#include "fpdf_ppo.h"
#include "fpdf_save.h"
#include "fpdf_text.h"
#include "fpdfview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <vector>

namespace pdfforge {
namespace {

struct PdfWriteBuffer : FPDF_FILEWRITE {
    std::vector<std::uint8_t> data;
};

int pdfWriteBlock(FPDF_FILEWRITE* parent, const void* data, unsigned long size) {
    auto* self = static_cast<PdfWriteBuffer*>(parent);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    self->data.insert(self->data.end(), bytes, bytes + size);
    return 1;
}

std::string textObjectUtf8(FPDF_PAGEOBJECT obj, FPDF_TEXTPAGE textPage) {
    const unsigned long bytes = FPDFTextObj_GetText(obj, textPage, nullptr, 0);
    if (bytes <= 2) {
        return {};
    }
    std::vector<FPDF_WCHAR> buf(bytes / sizeof(FPDF_WCHAR));
    FPDFTextObj_GetText(obj, textPage, buf.data(), bytes);
    return utf16LeToUtf8(buf.data(), buf.size());
}

bool setObjectText(FPDF_PAGEOBJECT obj, const std::string& utf8) {
    if (utf8.empty()) {
        return false;
    }
    const auto wide = utf8ToUtf16Le(utf8);
    return FPDFText_SetText(obj, wide.data()) != 0;
}

const char* standardFontFor(int weight, bool italic) {
    if (weight >= 600 && italic) {
        return "Helvetica-BoldOblique";
    }
    if (weight >= 600) {
        return "Helvetica-Bold";
    }
    if (italic) {
        return "Helvetica-Oblique";
    }
    return "Helvetica";
}

FPDF_PAGEOBJECT objectFromSpan(FPDF_PAGE page, FPDF_TEXTPAGE textPage, const TextSpan& span) {
    if (textPage && span.pdfCharStart >= 0) {
        if (FPDF_PAGEOBJECT obj = FPDFText_GetTextObject(textPage, span.pdfCharStart)) {
            return obj;
        }
    }
    if (page && span.pageObjectIndex >= 0 && span.pageObjectIndex < FPDFPage_CountObjects(page)) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, span.pageObjectIndex);
        if (obj && FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_TEXT) {
            return obj;
        }
    }
    return nullptr;
}

void appendUnique(std::vector<FPDF_PAGEOBJECT>& out, FPDF_PAGEOBJECT obj) {
    if (!obj) {
        return;
    }
    if (std::find(out.begin(), out.end(), obj) == out.end()) {
        out.push_back(obj);
    }
}

std::vector<FPDF_PAGEOBJECT> objectsForSpan(FPDF_PAGE page, FPDF_TEXTPAGE textPage,
                                            const TextSpan& span) {
    std::vector<FPDF_PAGEOBJECT> out;
    if (textPage && span.pdfCharStart >= 0) {
        const int end = span.pdfCharEnd > span.pdfCharStart ? span.pdfCharEnd : span.pdfCharStart + 1;
        for (int i = span.pdfCharStart; i < end; ++i) {
            appendUnique(out, FPDFText_GetTextObject(textPage, i));
        }
    }
    if (out.empty()) {
        appendUnique(out, objectFromSpan(page, textPage, span));
    }
    return out;
}

void generateOrThrow(FPDF_PAGE page) {
    if (!FPDFPage_GenerateContent(page)) {
        throw Error(Status::EditFailed, "FPDFPage_GenerateContent failed");
    }
}

unsigned toByte(float c) {
    const int v = static_cast<int>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
    return static_cast<unsigned>(std::clamp(v, 0, 255));
}

constexpr const char kSignMark[] = "PDFForgeSign";

FPDF_PAGEOBJECT findImageObject(FPDF_PAGE page, const ImageObject& image) {
    const int count = FPDFPage_CountObjects(page);
    if (image.pdfObjectIndex >= 0 && image.pdfObjectIndex < count) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, image.pdfObjectIndex);
        if (obj && FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_IMAGE) {
            return obj;
        }
    }
    FPDF_PAGEOBJECT best = nullptr;
    float bestDist = 1.0e12f;
    for (int i = 0; i < count; ++i) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
        if (!obj || FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE) {
            continue;
        }
        float left = 0, bottom = 0, right = 0, top = 0;
        if (!FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top)) {
            continue;
        }
        const float dx = left - image.bounds.x;
        const float dy = bottom - image.bounds.y;
        const float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            best = obj;
        }
    }
    return best;
}

}  // namespace

std::vector<std::uint8_t> PdfDocument::saveToMemoryLocked() const {
    PdfWriteBuffer writer{};
    writer.version = 1;
    writer.WriteBlock = pdfWriteBlock;
    if (!FPDF_SaveAsCopy(static_cast<FPDF_DOCUMENT>(document_), &writer, FPDF_NO_INCREMENTAL)) {
        throw Error(Status::IoError, "FPDF_SaveAsCopy failed");
    }
    return writer.data;
}

void PdfDocument::writeBytesToPath(const std::vector<std::uint8_t>& bytes,
                                   const std::filesystem::path& destination) const {
    std::filesystem::path tmpPath = destination;
    tmpPath += ".pdfforge-tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw Error(Status::IoError, "cannot open destination for writing");
        }
        if (!bytes.empty()) {
            out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        }
        if (!out) {
            throw Error(Status::IoError, "short write");
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, destination, ec);
    if (ec) {
        std::filesystem::copy_file(tmpPath, destination,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath);
        if (ec) {
            throw Error(Status::IoError, "cannot replace destination file");
        }
    }
}

void PdfDocument::reloadFromBytesLocked(std::vector<std::uint8_t> bytes) {
    FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(document_));
    document_ = nullptr;
    FPDF_DOCUMENT doc = FPDF_LoadMemDocument64(bytes.data(), bytes.size(), nullptr);
    if (!doc) {
        // Try to restore the previous buffer if reload fails.
        doc = FPDF_LoadMemDocument64(bytes_.data(), bytes_.size(), nullptr);
        document_ = doc;
        pageCount_ = doc ? FPDF_GetPageCount(doc) : 0;
        throw Error(Status::InvalidPdf, "reload after edit failed");
    }
    bytes_ = std::move(bytes);
    document_ = doc;
    pageCount_ = FPDF_GetPageCount(doc);
}

void PdfDocument::markDirtyLocked() {
    try {
        undo_.push_back(saveToMemoryLocked());
        if (undo_.size() > 8) {
            undo_.erase(undo_.begin());
        }
    } catch (const Error&) {
        // Editing can still proceed without undo if a snapshot fails.
    }
    dirty_ = true;
}

void PdfDocument::bakeLocked() {
    auto baked = saveToMemoryLocked();
    reloadFromBytesLocked(std::move(baked));
}

int indexOfPageObject(FPDF_PAGE page, FPDF_PAGEOBJECT obj) {
    if (!page || !obj) {
        return -1;
    }
    const int n = FPDFPage_CountObjects(page);
    for (int i = 0; i < n; ++i) {
        if (FPDFPage_GetObject(page, i) == obj) {
            return i;
        }
    }
    return -1;
}

void dirtyObjectTree(FPDF_PAGEOBJECT obj) {
    if (!obj) {
        return;
    }
    // Identity transforms are no-ops in PDFium and do not mark the object dirty.
    // A sub-point translation forces GenerateContent to rewrite the original stream.
    FPDFPageObj_Transform(obj, 1, 0, 0, 1, 1e-4, 0);
    if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_FORM) {
        return;
    }
    const int n = FPDFFormObj_CountObjects(obj);
    for (int i = 0; i < n; ++i) {
        dirtyObjectTree(FPDFFormObj_GetObject(obj, static_cast<unsigned long>(i)));
    }
}

void dirtyAllPageObjects(FPDF_PAGE page) {
    const int n = FPDFPage_CountObjects(page);
    for (int i = 0; i < n; ++i) {
        dirtyObjectTree(FPDFPage_GetObject(page, i));
    }
}

bool formContains(FPDF_PAGEOBJECT form, FPDF_PAGEOBJECT target) {
    if (!form || !target) {
        return false;
    }
    const int n = FPDFFormObj_CountObjects(form);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT child = FPDFFormObj_GetObject(form, static_cast<unsigned long>(i));
        if (!child) {
            continue;
        }
        if (child == target) {
            return true;
        }
        if (FPDFPageObj_GetType(child) == FPDF_PAGEOBJ_FORM && formContains(child, target)) {
            return true;
        }
    }
    return false;
}

FPDF_PAGEOBJECT findFormContaining(FPDF_PAGE page, FPDF_PAGEOBJECT target) {
    if (!page || !target) {
        return nullptr;
    }
    const int n = FPDFPage_CountObjects(page);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
        if (obj && FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_FORM && formContains(obj, target)) {
            return obj;
        }
    }
    return nullptr;
}

bool insertOwnedObject(FPDF_PAGE page, FPDF_PAGEOBJECT obj, int at) {
    const int count = FPDFPage_CountObjects(page);
    if (at >= 0 && at <= count) {
        if (FPDFPage_InsertObjectAtIndex(page, obj, static_cast<size_t>(at))) {
            return true;
        }
    }
    return FPDFPage_InsertObject(page, obj) != 0;
}

bool explodeFormToPage(FPDF_PAGE page, FPDF_PAGEOBJECT form) {
    if (!page || !form) {
        return false;
    }
    FS_MATRIX matrix{1, 0, 0, 1, 0, 0};
    FPDFPageObj_GetMatrix(form, &matrix);
    const int childCount = FPDFFormObj_CountObjects(form);
    std::vector<FPDF_PAGEOBJECT> children;
    children.reserve(static_cast<std::size_t>(std::max(0, childCount)));
    for (int i = 0; i < childCount; ++i) {
        if (FPDF_PAGEOBJECT child = FPDFFormObj_GetObject(form, static_cast<unsigned long>(i))) {
            children.push_back(child);
        }
    }
    int at = indexOfPageObject(page, form);
    for (FPDF_PAGEOBJECT child : children) {
        if (!FPDFFormObj_RemoveObject(form, child)) {
            return false;
        }
        FPDFPageObj_Transform(child, matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f);
        if (!insertOwnedObject(page, child, at)) {
            return false;
        }
        if (at >= 0) {
            ++at;
        }
    }
    if (!FPDFPage_RemoveObject(page, form)) {
        return false;
    }
    FPDFPageObj_Destroy(form);
    return true;
}

void promoteTargetsToPage(FPDF_PAGE page, const std::vector<FPDF_PAGEOBJECT>& targets) {
    for (int guard = 0; guard < 24; ++guard) {
        FPDF_PAGEOBJECT form = nullptr;
        for (FPDF_PAGEOBJECT target : targets) {
            if (!target || indexOfPageObject(page, target) >= 0) {
                continue;
            }
            form = findFormContaining(page, target);
            break;
        }
        if (!form) {
            return;
        }
        if (!explodeFormToPage(page, form)) {
            throw Error(Status::EditFailed, "could not flatten form XObject for edit");
        }
    }
    for (FPDF_PAGEOBJECT target : targets) {
        if (target && indexOfPageObject(page, target) < 0) {
            throw Error(Status::EditFailed, "text object is nested deeper than supported");
        }
    }
}

void dropPageObjects(FPDF_PAGE page, const std::vector<FPDF_PAGEOBJECT>& targets) {
    for (FPDF_PAGEOBJECT obj : targets) {
        if (!obj) {
            continue;
        }
        FPDFPageObj_SetIsActive(obj, false);
        if (FPDFPage_RemoveObject(page, obj)) {
            FPDFPageObj_Destroy(obj);
        }
    }
}

RectF pdfObjectBounds(FPDF_PAGEOBJECT obj) {
    float left = 0;
    float bottom = 0;
    float right = 0;
    float top = 0;
    if (!obj || !FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top)) {
        return {};
    }
    return RectF{left, bottom, right - left, top - bottom};
}

RectF clampRectToPage(RectF box, float pageW, float pageH) {
    const float x1 = std::min(pageW, box.x + box.width);
    const float y1 = std::min(pageH, box.y + box.height);
    box.x = std::max(0.0f, box.x);
    box.y = std::max(0.0f, box.y);
    box.width = std::max(0.0f, x1 - box.x);
    box.height = std::max(0.0f, y1 - box.y);
    return box;
}

RectF paddedBox(RectF box, float pad) {
    box.x -= pad;
    box.y -= pad;
    box.width += pad * 2.0f;
    box.height += pad * 2.0f;
    return box;
}

bool shouldDeleteTextObject(const RectF& obj, const RectF& box) {
    if (obj.empty() || !obj.intersects(box)) {
        return false;
    }
    const float objArea = std::max(1.0f, obj.area());
    const float boxArea = std::max(1.0f, box.area());
    // Full-page / letterhead runs stay; a local cover hides the selected slice.
    if (objArea > boxArea * 4.0f &&
        (obj.width > box.width * 2.5f || obj.height > box.height * 2.5f)) {
        return false;
    }
    const RectF hit = obj.intersection(box);
    if (box.contains(obj.x + obj.width * 0.5f, obj.y + obj.height * 0.5f)) {
        return true;
    }
    return hit.area() >= objArea * 0.12f || hit.area() >= boxArea * 0.08f;
}

void flattenIntersectingForms(FPDF_PAGE page, const RectF& box) {
    for (int guard = 0; guard < 24; ++guard) {
        FPDF_PAGEOBJECT form = nullptr;
        const int n = FPDFPage_CountObjects(page);
        for (int i = 0; i < n; ++i) {
            FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
            if (!obj || FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_FORM) {
                continue;
            }
            if (pdfObjectBounds(obj).intersects(box)) {
                form = obj;
                break;
            }
        }
        if (!form) {
            return;
        }
        if (!explodeFormToPage(page, form)) {
            throw Error(Status::EditFailed, "could not flatten form XObject for region edit");
        }
    }
}

Color samplePaperColor(FPDF_PAGE page, const RectF& box) {
    const Color white = Color::rgb(1, 1, 1);
    if (!page || box.empty()) {
        return white;
    }
    const float pageW = FPDF_GetPageWidthF(page);
    const float pageH = FPDF_GetPageHeightF(page);
    int width = std::max(1, static_cast<int>(std::lround(pageW)));
    int height = std::max(1, static_cast<int>(std::lround(pageH)));
    const int maxDim = std::max(width, height);
    if (maxDim > 1600) {
        const float cap = 1600.0f / static_cast<float>(maxDim);
        width = std::max(1, static_cast<int>(std::lround(static_cast<float>(width) * cap)));
        height = std::max(1, static_cast<int>(std::lround(static_cast<float>(height) * cap)));
    }
    FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 1);
    if (!bitmap) {
        return white;
    }
    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFFu);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, 0);

    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    FPDF_PageToDevice(page, 0, 0, width, height, 0, static_cast<double>(box.x),
                      static_cast<double>(box.y + box.height), &ax, &ay);
    FPDF_PageToDevice(page, 0, 0, width, height, 0, static_cast<double>(box.x + box.width),
                      static_cast<double>(box.y), &bx, &by);
    const int x0 = std::clamp(std::min(ax, bx), 0, width - 1);
    const int y0 = std::clamp(std::min(ay, by), 0, height - 1);
    const int x1 = std::clamp(std::max(ax, bx), x0 + 1, width);
    const int y1 = std::clamp(std::max(ay, by), y0 + 1, height);

    const auto* src = static_cast<const std::uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
    const int stride = FPDFBitmap_GetStride(bitmap);
    std::vector<unsigned> lightR;
    std::vector<unsigned> lightG;
    std::vector<unsigned> lightB;
    std::vector<unsigned> allR;
    std::vector<unsigned> allG;
    std::vector<unsigned> allB;
    const int step = std::max(1, (x1 - x0) * (y1 - y0) / 4000);
    for (int y = y0; y < y1; y += step) {
        for (int x = x0; x < x1; x += step) {
            const auto* px = src + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride) +
                             static_cast<std::size_t>(x) * 4u;
            const unsigned b = px[0];
            const unsigned g = px[1];
            const unsigned r = px[2];
            const int lum = (static_cast<int>(r) * 3 + static_cast<int>(g) * 6 + static_cast<int>(b)) / 10;
            allR.push_back(r);
            allG.push_back(g);
            allB.push_back(b);
            if (lum >= 190) {
                lightR.push_back(r);
                lightG.push_back(g);
                lightB.push_back(b);
            }
        }
    }
    FPDFBitmap_Destroy(bitmap);

    auto medianChannel = [](std::vector<unsigned>& v) -> unsigned {
        if (v.empty()) {
            return 255;
        }
        const std::size_t mid = v.size() / 2;
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
        return v[mid];
    };
    if (lightR.size() >= 8) {
        return Color::fromBytes(medianChannel(lightR), medianChannel(lightG), medianChannel(lightB));
    }
    if (!allR.empty()) {
        return Color::fromBytes(medianChannel(allR), medianChannel(allG), medianChannel(allB));
    }
    return white;
}

bool insertCoverRect(FPDF_PAGE page, const RectF& box, unsigned r, unsigned g, unsigned b) {
    if (!page || box.width < 0.5f || box.height < 0.5f) {
        return false;
    }
    FPDF_PAGEOBJECT path = FPDFPageObj_CreateNewRect(box.x, box.y, box.width, box.height);
    if (!path) {
        return false;
    }
    if (!FPDFPath_SetDrawMode(path, FPDF_FILLMODE_WINDING, 0) ||
        !FPDFPageObj_SetFillColor(path, r, g, b, 255) || !FPDFPage_InsertObject(page, path)) {
        FPDFPageObj_Destroy(path);
        return false;
    }
    return true;
}

void PdfDocument::rewriteSpanLocked(const TextSpan& span, const std::string& utf8, float fontSize,
                                    const Color& color) {
    rewriteSpansLocked({span}, utf8, fontSize, color);
}

void PdfDocument::rewriteSpansLocked(const std::vector<TextSpan>& spans, const std::string& utf8,
                                     float fontSize, const Color& color) {
    if (spans.empty()) {
        throw Error(Status::InvalidArgument, "empty region");
    }
    const TextSpan& span = spans.front();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), span.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    std::vector<FPDF_PAGEOBJECT> targets;
    for (const auto& item : spans) {
        for (FPDF_PAGEOBJECT obj : objectsForSpan(page, text, item)) {
            appendUnique(targets, obj);
        }
    }
    FPDF_PAGEOBJECT first = targets.empty() ? nullptr : targets.front();

    double originX = static_cast<double>(span.x);
    double originY = static_cast<double>(span.baseline);
    if (text && span.pdfCharStart >= 0) {
        FPDFText_GetCharOrigin(text, span.pdfCharStart, &originX, &originY);
    }

    FS_MATRIX matrix{1, 0, 0, 1, static_cast<float>(originX), static_cast<float>(originY)};
    bool hasMatrix = first && FPDFPageObj_GetMatrix(first, &matrix) != 0;
    float size = fontSize > 0 ? fontSize : (span.fontSize > 0 ? span.fontSize : 12.0f);
    if (first) {
        float existing = size;
        if (FPDFTextObj_GetFontSize(first, &existing) && existing > 0 && fontSize <= 0) {
            size = existing;
        }
    }
    unsigned r = toByte(color.toRgb().c0);
    unsigned g = toByte(color.toRgb().c1);
    unsigned b = toByte(color.toRgb().c2);
    unsigned a = toByte(color.toRgb().alpha);
    if (first) {
        FPDFPageObj_GetFillColor(first, &r, &g, &b, &a);
        if (color.alpha > 0) {
            const Color rgb = color.toRgb();
            r = toByte(rgb.c0);
            g = toByte(rgb.c1);
            b = toByte(rgb.c2);
            a = toByte(rgb.alpha);
        }
    }

    std::string next = utf8;
    if (text && targets.size() == 1 && spans.size() == 1 && !utf8.empty()) {
        const std::string current = textObjectUtf8(first, text);
        if (!span.text.empty() && current.find(span.text) != std::string::npos &&
            current != span.text) {
            next = replaceUtf8Once(current, span.text, utf8);
        }
    }
    if (text) {
        FPDFText_ClosePage(text);
        text = nullptr;
    }

    if (targets.empty()) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "no PDF text object for span");
    }

    FPDF_PAGEOBJECT neu = nullptr;
    bool inserted = false;
    try {
        promoteTargetsToPage(page, targets);
        generateOrThrow(page);
        const int oldIndex = indexOfPageObject(page, targets.front());
        if (oldIndex >= 0 && FPDFPageObj_GetMatrix(targets.front(), &matrix)) {
            hasMatrix = true;
        }

        if (!next.empty()) {
            const bool sizeInMatrix =
                hasMatrix && targets.size() == 1 && size > 2.0f &&
                std::fabs(std::hypot(matrix.a, matrix.b) - size) < 0.75f;
            neu = FPDFPageObj_NewTextObj(static_cast<FPDF_DOCUMENT>(document_),
                                         standardFontFor(span.fontWeight, span.italic),
                                         sizeInMatrix ? 1.0f : size);
            if (!neu || !setObjectText(neu, next)) {
                throw Error(Status::EditFailed, "could not create replacement text object");
            }
            if (hasMatrix && targets.size() == 1) {
                FPDFPageObj_SetMatrix(neu, &matrix);
            } else {
                FS_MATRIX placed{1, 0, 0, 1, static_cast<float>(originX),
                                 static_cast<float>(originY)};
                FPDFPageObj_SetMatrix(neu, &placed);
            }
            FPDFPageObj_SetFillColor(neu, r, g, b, a);
        }

        dirtyAllPageObjects(page);
        dropPageObjects(page, targets);
        generateOrThrow(page);
        if (neu) {
            if (!insertOwnedObject(page, neu, oldIndex)) {
                throw Error(Status::EditFailed, "could not insert replacement text object");
            }
            inserted = true;
            generateOrThrow(page);
        }
    } catch (...) {
        if (neu && !inserted) {
            FPDFPageObj_Destroy(neu);
        }
        FPDF_ClosePage(page);
        throw;
    }

    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::replaceSpanText(const TextSpan& span, const std::string& utf8) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "replaceSpanText");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    rewriteSpanLocked(span, utf8, span.fontSize, span.color);
}

void PdfDocument::setSpanColor(const TextSpan& span, const Color& color) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "setSpanColor");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    rewriteSpanLocked(span, span.text, span.fontSize, color);
}

void PdfDocument::setSpanFontSize(const TextSpan& span, float fontSize) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "setSpanFontSize");
    }
    if (fontSize <= 0.0f || fontSize > 200.0f) {
        throw Error(Status::InvalidArgument, "font size out of range");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    rewriteSpanLocked(span, span.text, fontSize, span.color);
}

void PdfDocument::editSpan(const TextSpan& span, const std::string& utf8, float fontSize,
                           const Color& color) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "editSpan");
    }
    if (fontSize <= 0.0f || fontSize > 200.0f) {
        throw Error(Status::InvalidArgument, "font size out of range");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    rewriteSpanLocked(span, utf8, fontSize, color);
}

void PdfDocument::rewriteRegionLocked(int pageIndex, RectF box, const std::vector<TextSpan>& spans,
                                      const std::string& utf8, float fontSize, const Color& color) {
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }

    RectF area = box;
    if (area.empty()) {
        for (const auto& span : spans) {
            area = area.united(span.bounds());
        }
    }
    const float pageW = FPDF_GetPageWidthF(page);
    const float pageH = FPDF_GetPageHeightF(page);
    area = clampRectToPage(paddedBox(area, 0.75f), pageW, pageH);
    if (area.empty()) {
        FPDF_ClosePage(page);
        throw Error(Status::InvalidArgument, "empty region");
    }

    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    std::vector<FPDF_PAGEOBJECT> targets;
    for (const auto& item : spans) {
        for (FPDF_PAGEOBJECT obj : objectsForSpan(page, text, item)) {
            appendUnique(targets, obj);
        }
    }

    double originX = static_cast<double>(area.x + 0.5f);
    double originY = static_cast<double>(area.y + 1.0f);
    int fontWeight = 400;
    bool italic = false;
    if (!spans.empty()) {
        originX = static_cast<double>(spans.front().x);
        originY = static_cast<double>(spans.front().baseline);
        fontWeight = spans.front().fontWeight;
        italic = spans.front().italic;
        if (text && spans.front().pdfCharStart >= 0) {
            FPDFText_GetCharOrigin(text, spans.front().pdfCharStart, &originX, &originY);
        }
        if (fontSize <= 0.0f && spans.front().fontSize > 0) {
            fontSize = spans.front().fontSize;
        }
    }
    if (text) {
        FPDFText_ClosePage(text);
        text = nullptr;
    }
    if (fontSize <= 0.0f || fontSize > 200.0f) {
        fontSize = 12.0f;
    }

    const Color rgb = color.toRgb();
    unsigned r = toByte(rgb.c0);
    unsigned g = toByte(rgb.c1);
    unsigned b = toByte(rgb.c2);
    unsigned a = toByte(rgb.alpha);

    FPDF_PAGEOBJECT neu = nullptr;
    bool insertedText = false;
    try {
        if (!targets.empty()) {
            promoteTargetsToPage(page, targets);
        }
        flattenIntersectingForms(page, area);
        generateOrThrow(page);

        const int count = FPDFPage_CountObjects(page);
        for (int i = 0; i < count; ++i) {
            FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
            if (!obj || FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_TEXT) {
                continue;
            }
            if (shouldDeleteTextObject(pdfObjectBounds(obj), area)) {
                appendUnique(targets, obj);
            }
        }

        dirtyAllPageObjects(page);
        dropPageObjects(page, targets);
        generateOrThrow(page);

        const Color paper = samplePaperColor(page, area);
        const Color paperRgb = paper.toRgb();
        insertCoverRect(page, area, toByte(paperRgb.c0), toByte(paperRgb.c1), toByte(paperRgb.c2));

        if (!utf8.empty()) {
            neu = FPDFPageObj_NewTextObj(static_cast<FPDF_DOCUMENT>(document_),
                                         standardFontFor(fontWeight, italic), fontSize);
            if (!neu || !setObjectText(neu, utf8)) {
                throw Error(Status::EditFailed, "could not create replacement text object");
            }
            FS_MATRIX placed{1, 0, 0, 1, static_cast<float>(originX), static_cast<float>(originY)};
            FPDFPageObj_SetMatrix(neu, &placed);
            FPDFPageObj_SetFillColor(neu, r, g, b, a);
            if (!FPDFPage_InsertObject(page, neu)) {
                throw Error(Status::EditFailed, "could not insert replacement text object");
            }
            insertedText = true;
        }
        generateOrThrow(page);
    } catch (...) {
        if (neu && !insertedText) {
            FPDFPageObj_Destroy(neu);
        }
        FPDF_ClosePage(page);
        throw;
    }

    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::replaceRegion(const std::vector<TextSpan>& spans, const std::string& utf8,
                                float fontSize, const Color& color, RectF box) {
    if (spans.empty()) {
        throw Error(Status::InvalidArgument, "empty region");
    }
    replaceRegion(spans.front().pageIndex, box, spans, utf8, fontSize, color);
}

void PdfDocument::replaceRegion(int pageIndex, RectF box, const std::vector<TextSpan>& spans,
                                const std::string& utf8, float fontSize, const Color& color) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "replaceRegion");
    }
    if (spans.empty() && box.empty()) {
        throw Error(Status::InvalidArgument, "empty region");
    }
    if (fontSize <= 0.0f || fontSize > 200.0f) {
        fontSize = (!spans.empty() && spans.front().fontSize > 0) ? spans.front().fontSize : 12.0f;
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    rewriteRegionLocked(pageIndex, box, spans, utf8, fontSize, color);
}

void PdfDocument::save(const std::filesystem::path& destination) {
    if (destination.empty()) {
        throw Error(Status::InvalidArgument, "empty destination");
    }
    auto api = runtime_->lock();
    const auto bytes = saveToMemoryLocked();
    api.unlock();
    writeBytesToPath(bytes, destination);
    path_ = destination;
    bytes_ = bytes;
    dirty_ = false;
}

bool PdfDocument::undo() {
    if (undo_.empty()) {
        return false;
    }
    auto api = runtime_->lock();
    auto previous = std::move(undo_.back());
    undo_.pop_back();
    reloadFromBytesLocked(std::move(previous));
    dirty_ = !undo_.empty();
    return true;
}


void PdfDocument::deleteSpan(const TextSpan& span) {
    replaceSpanText(span, {});
}

void PdfDocument::addText(int pageIndex, PointF pagePoint, const std::string& utf8, float fontSize,
                          const Color& color) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "addText");
    }
    if (utf8.empty()) {
        throw Error(Status::InvalidArgument, "empty text");
    }
    if (fontSize <= 0.0f) {
        fontSize = 12.0f;
    }
    const Color rgb = color.toRgb();
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_PAGEOBJECT obj =
        FPDFPageObj_NewTextObj(static_cast<FPDF_DOCUMENT>(document_), "Helvetica", fontSize);
    if (!obj || !setObjectText(obj, utf8)) {
        if (obj) {
            FPDFPageObj_Destroy(obj);
        }
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "could not create text object");
    }
    FS_MATRIX matrix{1, 0, 0, 1, pagePoint.x, pagePoint.y};
    FPDFPageObj_SetMatrix(obj, &matrix);
    FPDFPageObj_SetFillColor(obj, toByte(rgb.c0), toByte(rgb.c1), toByte(rgb.c2), toByte(rgb.alpha));
    if (!FPDFPage_InsertObject(page, obj)) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "FPDFPage_InsertObject failed");
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::addImage(int pageIndex, RectF pageRect, const Bitmap& bitmap) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "addImage");
    }
    if (bitmap.empty() || bitmap.width > 4096 || bitmap.height > 4096) {
        throw Error(Status::InvalidArgument, "image stamp size");
    }
    if (pageRect.width < 1.0f || pageRect.height < 1.0f) {
        throw Error(Status::InvalidArgument, "image stamp rectangle");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_BITMAP pdfBitmap = FPDFBitmap_Create(bitmap.width, bitmap.height, 1);
    if (!pdfBitmap) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "FPDFBitmap_Create failed");
    }
    auto* dest = static_cast<std::uint8_t*>(FPDFBitmap_GetBuffer(pdfBitmap));
    const int destStride = FPDFBitmap_GetStride(pdfBitmap);
    for (int y = 0; y < bitmap.height; ++y) {
        const auto* src = bitmap.bgra.data() +
                          static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.stride);
        std::memcpy(dest + static_cast<std::size_t>(y) * static_cast<std::size_t>(destStride), src,
                    static_cast<std::size_t>(bitmap.width) * 4u);
    }
    FPDF_PAGEOBJECT obj = FPDFPageObj_NewImageObj(static_cast<FPDF_DOCUMENT>(document_));
    const bool ok = obj && FPDFImageObj_SetBitmap(&page, 1, obj, pdfBitmap) != 0 &&
                    FPDFImageObj_SetMatrix(obj, pageRect.width, 0, 0, pageRect.height, pageRect.x,
                                           pageRect.y) != 0 &&
                    FPDFPage_InsertObject(page, obj) != 0;
    FPDFBitmap_Destroy(pdfBitmap);
    if (!ok) {
        if (obj) {
            FPDFPageObj_Destroy(obj);
        }
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "could not insert image stamp");
    }
    (void)FPDFPageObj_AddMark(obj, kSignMark);
    generateOrThrow(page);
    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::setImageRect(const ImageObject& image, RectF pageRect) {
    if (image.pageIndex < 0 || image.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "setImageRect");
    }
    if (pageRect.width < 1.0f || pageRect.height < 1.0f) {
        throw Error(Status::InvalidArgument, "image stamp rectangle");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), image.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_PAGEOBJECT obj = findImageObject(page, image);
    if (!obj || FPDFImageObj_SetMatrix(obj, pageRect.width, 0, 0, pageRect.height, pageRect.x,
                                       pageRect.y) == 0) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "could not resize image stamp");
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::deleteImage(const ImageObject& image) {
    if (image.pageIndex < 0 || image.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "deleteImage");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), image.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_PAGEOBJECT obj = findImageObject(page, image);
    if (!obj || !FPDFPage_RemoveObject(page, obj)) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "could not remove image stamp");
    }
    FPDFPageObj_Destroy(obj);
    generateOrThrow(page);
    FPDF_ClosePage(page);
    bakeLocked();
}

void PdfDocument::setPageRotation(int pageIndex, int quarterTurns) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "setPageRotation");
    }
    int rot = quarterTurns % 4;
    if (rot < 0) {
        rot += 4;
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDFPage_SetRotation(page, rot);
    FPDF_ClosePage(page);
}

void PdfDocument::deletePage(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "deletePage");
    }
    if (pageCount_ <= 1) {
        throw Error(Status::InvalidArgument, "cannot delete the last page");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDFPage_Delete(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    pageCount_ = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(document_));
}

void PdfDocument::insertBlankPage(int atIndex, SizeF size) {
    if (atIndex < 0 || atIndex > pageCount_) {
        throw Error(Status::PageOutOfRange, "insertBlankPage");
    }
    if (size.width < 1 || size.height < 1) {
        size = SizeF{612, 792};
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDFPage_New(static_cast<FPDF_DOCUMENT>(document_), atIndex, size.width,
                                  size.height);
    if (!page) {
        throw Error(Status::EditFailed, "FPDFPage_New failed");
    }
    FPDF_ClosePage(page);
    pageCount_ = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(document_));
}

void PdfDocument::importPages(const std::filesystem::path& sourcePdf, int atIndex) {
    if (atIndex < 0 || atIndex > pageCount_) {
        throw Error(Status::PageOutOfRange, "importPages");
    }
    std::error_code ec;
    if (!std::filesystem::exists(sourcePdf, ec)) {
        throw Error(Status::FileNotFound, "import source missing");
    }
    const auto fileSize = std::filesystem::file_size(sourcePdf, ec);
    if (ec) {
        throw Error(Status::IoError, "cannot stat import source");
    }
    std::ifstream in(sourcePdf, std::ios::binary);
    if (!in) {
        throw Error(Status::IoError, "cannot open import source");
    }
    std::vector<std::uint8_t> srcBytes(static_cast<std::size_t>(fileSize));
    if (fileSize > 0) {
        in.read(reinterpret_cast<char*>(srcBytes.data()), static_cast<std::streamsize>(fileSize));
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_DOCUMENT src = FPDF_LoadMemDocument64(srcBytes.data(), srcBytes.size(), nullptr);
    if (!src) {
        throw Error(Status::InvalidPdf, "could not open PDF to insert");
    }
    const bool ok =
        FPDF_ImportPages(static_cast<FPDF_DOCUMENT>(document_), src, nullptr, atIndex) != 0;
    FPDF_CloseDocument(src);
    if (!ok) {
        throw Error(Status::EditFailed, "FPDF_ImportPages failed");
    }
    pageCount_ = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(document_));
}

void PdfDocument::addInvisibleOcrLayer(int pageIndex, const OcrPageResult& ocr, float sourceDpi) {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "addInvisibleOcrLayer");
    }
    if (ocr.words.empty()) {
        throw Error(Status::OcrFailed, "OCR returned no words");
    }
    if (sourceDpi <= 0) {
        sourceDpi = 300.0f;
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    const float pageH = FPDF_GetPageHeightF(page);
    const float scale = 72.0f / sourceDpi;
    int added = 0;
    for (const auto& word : ocr.words) {
        if (word.text.empty()) {
            continue;
        }
        const float fontSize = std::max(6.0f, word.height * scale);
        FPDF_PAGEOBJECT obj = FPDFPageObj_NewTextObj(static_cast<FPDF_DOCUMENT>(document_),
                                                     "Helvetica", fontSize);
        if (!obj || !setObjectText(obj, word.text)) {
            if (obj) {
                FPDFPageObj_Destroy(obj);
            }
            continue;
        }
        const float x = word.x * scale;
        const float y = pageH - (word.y + word.height) * scale;
        FS_MATRIX matrix{1, 0, 0, 1, x, y};
        FPDFPageObj_SetMatrix(obj, &matrix);
        FPDFTextObj_SetTextRenderMode(obj, FPDF_TEXTRENDERMODE_INVISIBLE);
        if (FPDFPage_InsertObject(page, obj)) {
            ++added;
        }
    }
    if (added == 0) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "could not insert OCR text objects");
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
    Logger::instance().info("ocr", "inserted invisible text layer");
}

}  // namespace pdfforge
