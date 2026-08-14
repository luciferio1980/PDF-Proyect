#include "pdf/PdfDocument.h"

#include "core/Logger.h"
#include "core/Utf.h"
#include "pdf/PageClassifier.h"
#include "pdf/PdfTextExtractor.h"
#include "pdf/PdfiumRuntime.h"

#include "fpdf_doc.h"
#include "fpdf_edit.h"
#include "fpdf_text.h"
#include "fpdfview.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>

#if PDFFORGE_HAS_QPDF
#include "pdf/QpdfBridge.h"
#endif

namespace pdfforge {
namespace {

Status statusFromPdfium(unsigned long code, bool passwordProvided) {
    switch (code) {
        case FPDF_ERR_SUCCESS:
            return Status::Ok;
        case FPDF_ERR_FILE:
            return Status::FileNotFound;
        case FPDF_ERR_FORMAT:
            return Status::InvalidPdf;
        case FPDF_ERR_PASSWORD:
            return passwordProvided ? Status::PasswordIncorrect : Status::PasswordRequired;
        case FPDF_ERR_SECURITY:
            return Status::EncryptedUnsupported;
        case FPDF_ERR_PAGE:
            return Status::PageOutOfRange;
        default:
            return Status::InvalidPdf;
    }
}

std::string metaField(FPDF_DOCUMENT doc, const char* tag) {
    unsigned long bytes = FPDF_GetMetaText(doc, tag, nullptr, 0);
    if (bytes <= 2) {
        return {};
    }
    std::vector<unsigned short> buf(bytes / sizeof(unsigned short));
    FPDF_GetMetaText(doc, tag, buf.data(), bytes);
    return utf16LeToUtf8(buf.data(), buf.size());
}

int clampRotation(int quarterTurns) {
    int r = quarterTurns % 4;
    if (r < 0) {
        r += 4;
    }
    return r;
}

}  // namespace

std::unique_ptr<PdfDocument> PdfDocument::open(std::shared_ptr<PdfiumRuntime> runtime,
                                               const std::filesystem::path& path,
                                               const OpenOptions& options) {
    if (!runtime) {
        throw Error(Status::InternalError, "PDFium runtime is null");
    }
    if (path.empty()) {
        throw Error(Status::InvalidArgument, "empty path");
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        throw Error(Status::FileNotFound, "path does not exist");
    }
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) {
        throw Error(Status::IoError, "cannot stat file");
    }
    if (fileSize > options.maxFileBytes) {
        throw Error(Status::FileTooLarge, "file larger than maxFileBytes");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw Error(Status::IoError, "cannot open file for reading");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (fileSize > 0) {
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(fileSize));
        if (in.gcount() != static_cast<std::streamsize>(fileSize)) {
            throw Error(Status::IoError, "short read");
        }
    }

    auto api = runtime->lock();
    const char* password = options.password.empty() ? nullptr : options.password.c_str();
    FPDF_DOCUMENT doc =
        FPDF_LoadMemDocument64(bytes.data(), bytes.size(), password);
    if (!doc) {
        const auto err = FPDF_GetLastError();
        throw Error(statusFromPdfium(err, !options.password.empty()),
                    "FPDF_LoadMemDocument64 failed");
    }

    const int pages = FPDF_GetPageCount(doc);
    if (pages < 0) {
        FPDF_CloseDocument(doc);
        throw Error(Status::InvalidPdf, "negative page count");
    }
    if (pages > options.maxPages) {
        FPDF_CloseDocument(doc);
        throw Error(Status::TooManyPages, "page count exceeds limit");
    }

    Logger::instance().info("pdf", "opened document pages=" + std::to_string(pages));
    auto owned = std::unique_ptr<PdfDocument>(
        new PdfDocument(std::move(runtime), path, std::move(bytes), doc));
    owned->pageCount_ = pages;
    return owned;
}

PdfDocument::PdfDocument(std::shared_ptr<PdfiumRuntime> runtime, std::filesystem::path path,
                         std::vector<std::uint8_t> bytes, void* document)
    : runtime_(std::move(runtime)),
      path_(std::move(path)),
      bytes_(std::move(bytes)),
      document_(document) {}

PdfDocument::~PdfDocument() {
    if (runtime_ && document_) {
        auto api = runtime_->lock();
        FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(document_));
        document_ = nullptr;
    }
}

SizeF PdfDocument::pageSize(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "pageSize");
    }
    auto api = runtime_->lock();
    double w = 0;
    double h = 0;
    if (!FPDF_GetPageSizeByIndex(static_cast<FPDF_DOCUMENT>(document_), pageIndex, &w, &h)) {
        throw Error(Status::InternalError, "FPDF_GetPageSizeByIndex failed");
    }
    return SizeF{static_cast<float>(w), static_cast<float>(h)};
}

int PdfDocument::pageRotation(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "pageRotation");
    }
    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::InternalError, "FPDF_LoadPage failed");
    }
    const int rot = FPDFPage_GetRotation(page);
    FPDF_ClosePage(page);
    return rot;
}

Metadata PdfDocument::metadata() const {
    auto api = runtime_->lock();
    auto* doc = static_cast<FPDF_DOCUMENT>(document_);
    Metadata m;
    m.title = metaField(doc, "Title");
    m.author = metaField(doc, "Author");
    m.subject = metaField(doc, "Subject");
    m.keywords = metaField(doc, "Keywords");
    m.creator = metaField(doc, "Creator");
    m.producer = metaField(doc, "Producer");
    m.creationDate = metaField(doc, "CreationDate");
    m.modificationDate = metaField(doc, "ModDate");
    return m;
}

std::vector<TextSpan> PdfDocument::extractText(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "extractText");
    }
    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::InternalError, "FPDF_LoadPage failed");
    }
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    std::vector<TextSpan> spans;
    if (text) {
        spans = extractTextSpans(text, pageIndex);
        FPDFText_ClosePage(text);
    }
    FPDF_ClosePage(page);
    return spans;
}

std::string PdfDocument::extractPlainText(int pageIndex) const {
    const auto spans = extractText(pageIndex);
    std::string out;
    for (const auto& span : spans) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += span.text;
    }
    return out;
}

PageClassification PdfDocument::classifyPage(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "classifyPage");
    }
    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::InternalError, "FPDF_LoadPage failed");
    }
    const auto size = [page]() {
        return SizeF{FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page)};
    }();
    FPDF_TEXTPAGE text = FPDFText_LoadPage(page);
    const auto classification = classifyLoadedPage(page, text, size);
    if (text) {
        FPDFText_ClosePage(text);
    }
    FPDF_ClosePage(page);
    return classification;
}

std::vector<ImageObject> PdfDocument::extractImages(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "extractImages");
    }
    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        throw Error(Status::InternalError, "FPDF_LoadPage failed");
    }
    std::vector<ImageObject> images;
    const int count = FPDFPage_CountObjects(page);
    for (int i = 0; i < count; ++i) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
        if (!obj || FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE) {
            continue;
        }
        float left = 0, bottom = 0, right = 0, top = 0;
        ImageObject image;
        image.pageIndex = pageIndex;
        image.pdfObjectIndex = i;
        if (FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top)) {
            image.bounds = RectF{left, bottom, right - left, top - bottom};
        }
        images.push_back(image);
    }
    FPDF_ClosePage(page);
    return images;
}

Bitmap PdfDocument::render(const RenderRequest& request) const {
    if (request.pageIndex < 0 || request.pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "render");
    }
    if (request.dpi <= 0 || request.dpi > 1200) {
        throw Error(Status::InvalidArgument, "dpi out of range");
    }

    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), request.pageIndex);
    if (!page) {
        throw Error(Status::RenderFailed, "FPDF_LoadPage failed");
    }

    const float pageW = FPDF_GetPageWidthF(page);
    const float pageH = FPDF_GetPageHeightF(page);
    const int rot = clampRotation(request.rotationQuarterTurns);
    const bool swapped = (rot % 2) == 1;
    const float srcW = swapped ? pageH : pageW;
    const float srcH = swapped ? pageW : pageH;
    const float scale = request.dpi / 72.0f;
    int width = static_cast<int>(std::lround(srcW * scale));
    int height = static_cast<int>(std::lround(srcH * scale));
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    // Cap extreme allocations (security: memory).
    constexpr int kMaxDim = 8192;
    if (width > kMaxDim || height > kMaxDim) {
        FPDF_ClosePage(page);
        throw Error(Status::InvalidArgument, "render bitmap exceeds 8192px");
    }

    FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 1);
    if (!bitmap) {
        FPDF_ClosePage(page);
        throw Error(Status::RenderFailed, "FPDFBitmap_Create failed");
    }
    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
    const int flags = FPDF_ANNOT | FPDF_LCD_TEXT;
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, rot, flags);

    Bitmap out;
    out.width = FPDFBitmap_GetWidth(bitmap);
    out.height = FPDFBitmap_GetHeight(bitmap);
    out.stride = FPDFBitmap_GetStride(bitmap);
    const auto* src = static_cast<const std::uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
    const std::size_t bytes = static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(out.height);
    out.bgra.assign(src, src + bytes);

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
    return out;
}

bool PdfDocument::deviceToPage(int pageIndex, const RenderRequest& request, float deviceX,
                               float deviceY, PointF& pagePoint) const {
    if (pageIndex < 0 || pageIndex >= pageCount_) {
        throw Error(Status::PageOutOfRange, "deviceToPage");
    }
    auto api = runtime_->lock();
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(document_), pageIndex);
    if (!page) {
        return false;
    }
    const float pageW = FPDF_GetPageWidthF(page);
    const float pageH = FPDF_GetPageHeightF(page);
    const int rot = clampRotation(request.rotationQuarterTurns);
    const bool swapped = (rot % 2) == 1;
    const float scale = request.dpi / 72.0f;
    const int width = std::max(1, static_cast<int>(std::lround((swapped ? pageH : pageW) * scale)));
    const int height = std::max(1, static_cast<int>(std::lround((swapped ? pageW : pageH) * scale)));
    double px = 0;
    double py = 0;
    const bool ok = FPDF_DeviceToPage(page, 0, 0, width, height, rot, static_cast<int>(deviceX),
                                      static_cast<int>(deviceY), &px, &py) != 0;
    FPDF_ClosePage(page);
    if (ok) {
        pagePoint = PointF{static_cast<float>(px), static_cast<float>(py)};
    }
    return ok;
}

void PdfDocument::writeCopy(const std::filesystem::path& destination) const {
    if (destination.empty()) {
        throw Error(Status::InvalidArgument, "empty destination");
    }
    std::error_code eqEc;
    if (std::filesystem::exists(destination) &&
        std::filesystem::equivalent(destination, path_, eqEc) && !eqEc) {
        throw Error(Status::InvalidArgument, "refusing to overwrite the original in writeCopy");
    }
#if PDFFORGE_HAS_QPDF
    qpdfWriteCopy(path_, destination);
#else
    std::error_code ec;
    std::filesystem::copy_file(path_, destination, std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        throw Error(Status::IoError, "copy_file failed");
    }
#endif
}

}  // namespace pdfforge
