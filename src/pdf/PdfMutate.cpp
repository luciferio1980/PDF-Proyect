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
#include <fstream>

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

void generateOrThrow(FPDF_PAGE page) {
    if (!FPDFPage_GenerateContent(page)) {
        throw Error(Status::EditFailed, "FPDFPage_GenerateContent failed");
    }
}

unsigned toByte(float c) {
    const int v = static_cast<int>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
    return static_cast<unsigned>(std::clamp(v, 0, 255));
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

void PdfDocument::replaceSpanText(const TextSpan& span, const std::string& utf8) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "replaceSpanText");
    }
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), span.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    FPDF_PAGEOBJECT obj = objectFromSpan(page, text, span);
    if (!obj) {
        if (text) {
            FPDFText_ClosePage(text);
        }
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "no PDF text object for span");
    }

    if (utf8.empty()) {
        if (text) {
            FPDFText_ClosePage(text);
        }
        if (!FPDFPage_RemoveObject(page, obj)) {
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "FPDFPage_RemoveObject failed");
        }
        FPDFPageObj_Destroy(obj);
        generateOrThrow(page);
        FPDF_ClosePage(page);
        return;
    }

    std::string next = utf8;
    if (text) {
        const std::string current = textObjectUtf8(obj, text);
        if (!span.text.empty() && current.find(span.text) != std::string::npos &&
            current != span.text) {
            next = replaceUtf8Once(current, span.text, utf8);
        }
        FPDFText_ClosePage(text);
        text = nullptr;
    }

    bool ok = setObjectText(obj, next);
    if (!ok) {
        FS_MATRIX matrix{};
        FPDFPageObj_GetMatrix(obj, &matrix);
        float size = span.fontSize > 0 ? span.fontSize : 12.0f;
        FPDFTextObj_GetFontSize(obj, &size);
        unsigned r = 0, g = 0, b = 0, a = 255;
        FPDFPageObj_GetFillColor(obj, &r, &g, &b, &a);
        FPDF_PAGEOBJECT neu = FPDFPageObj_NewTextObj(
            static_cast<FPDF_DOCUMENT>(document_), standardFontFor(span.fontWeight, span.italic),
            size);
        if (!neu || !setObjectText(neu, next)) {
            if (neu) {
                FPDFPageObj_Destroy(neu);
            }
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "FPDFText_SetText failed");
        }
        FPDFPageObj_SetMatrix(neu, &matrix);
        FPDFPageObj_SetFillColor(neu, r, g, b, a);
        if (!FPDFPage_RemoveObject(page, obj)) {
            FPDFPageObj_Destroy(neu);
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "could not replace text object");
        }
        FPDFPageObj_Destroy(obj);
        FPDFPage_InsertObject(page, neu);
        ok = true;
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
}

void PdfDocument::setSpanColor(const TextSpan& span, const Color& color) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "setSpanColor");
    }
    const Color rgb = color.toRgb();
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), span.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    FPDF_PAGEOBJECT obj = objectFromSpan(page, text, span);
    if (text) {
        FPDFText_ClosePage(text);
    }
    if (!obj) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "no PDF text object for span");
    }
    if (!FPDFPageObj_SetFillColor(obj, toByte(rgb.c0), toByte(rgb.c1), toByte(rgb.c2),
                                  toByte(rgb.alpha))) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "FPDFPageObj_SetFillColor failed");
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
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
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), span.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    FPDF_PAGEOBJECT obj = objectFromSpan(page, text, span);
    if (text) {
        FPDFText_ClosePage(text);
    }
    if (!obj) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "no PDF text object for span");
    }
    if (!FPDFTextObj_SetFontSize(obj, fontSize)) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "FPDFTextObj_SetFontSize failed");
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
}

void PdfDocument::editSpan(const TextSpan& span, const std::string& utf8, float fontSize,
                           const Color& color) {
    if (span.pageIndex < 0 || span.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "editSpan");
    }
    if (fontSize <= 0.0f || fontSize > 200.0f) {
        throw Error(Status::InvalidArgument, "font size out of range");
    }
    const Color rgb = color.toRgb();
    auto api = runtime_->lock();
    markDirtyLocked();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), span.pageIndex);
    if (!page) {
        throw Error(Status::EditFailed, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    FPDF_PAGEOBJECT obj = objectFromSpan(page, text, span);
    std::string next = utf8;
    if (text && obj) {
        const std::string current = textObjectUtf8(obj, text);
        if (!span.text.empty() && !utf8.empty() && current.find(span.text) != std::string::npos &&
            current != span.text) {
            next = replaceUtf8Once(current, span.text, utf8);
        }
        FPDFText_ClosePage(text);
        text = nullptr;
    } else if (text) {
        FPDFText_ClosePage(text);
    }
    if (!obj) {
        FPDF_ClosePage(page);
        throw Error(Status::EditFailed, "no PDF text object for span");
    }
    FPDFTextObj_SetFontSize(obj, fontSize);
    FPDFPageObj_SetFillColor(obj, toByte(rgb.c0), toByte(rgb.c1), toByte(rgb.c2), toByte(rgb.alpha));
    if (next.empty()) {
        if (!FPDFPage_RemoveObject(page, obj)) {
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "FPDFPage_RemoveObject failed");
        }
        FPDFPageObj_Destroy(obj);
        generateOrThrow(page);
        FPDF_ClosePage(page);
        return;
    }
    if (!setObjectText(obj, next)) {
        FS_MATRIX matrix{};
        FPDFPageObj_GetMatrix(obj, &matrix);
        FPDF_PAGEOBJECT neu = FPDFPageObj_NewTextObj(
            static_cast<FPDF_DOCUMENT>(document_), standardFontFor(span.fontWeight, span.italic),
            fontSize);
        if (!neu || !setObjectText(neu, next)) {
            if (neu) {
                FPDFPageObj_Destroy(neu);
            }
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "FPDFText_SetText failed");
        }
        FPDFPageObj_SetMatrix(neu, &matrix);
        FPDFPageObj_SetFillColor(neu, toByte(rgb.c0), toByte(rgb.c1), toByte(rgb.c2),
                                 toByte(rgb.alpha));
        if (!FPDFPage_RemoveObject(page, obj)) {
            FPDFPageObj_Destroy(neu);
            FPDF_ClosePage(page);
            throw Error(Status::EditFailed, "could not replace text object");
        }
        FPDFPageObj_Destroy(obj);
        FPDFPage_InsertObject(page, neu);
    }
    generateOrThrow(page);
    FPDF_ClosePage(page);
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
