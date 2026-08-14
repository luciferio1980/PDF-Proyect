#pragma once

#include "core/Error.h"
#include "core/Geometry.h"
#include "model/Objects.h"
#include "model/PageClassification.h"
#include "ocr/OcrTypes.h"
#include "renderer/Bitmap.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pdfforge {

class PdfiumRuntime;

struct OpenOptions {
    std::string password;
    std::size_t maxFileBytes = 512ull * 1024ull * 1024ull;
    int maxPages = 10000;
};

class PdfDocument {
public:
    static std::unique_ptr<PdfDocument> open(std::shared_ptr<PdfiumRuntime> runtime,
                                             const std::filesystem::path& path,
                                             const OpenOptions& options = {});

    ~PdfDocument();

    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    [[nodiscard]] int pageCount() const noexcept { return pageCount_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] bool canUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] SizeF pageSize(int pageIndex) const;
    [[nodiscard]] int pageRotation(int pageIndex) const;
    [[nodiscard]] Metadata metadata() const;

    [[nodiscard]] std::vector<TextSpan> extractText(int pageIndex) const;
    [[nodiscard]] std::string extractPlainText(int pageIndex) const;
    [[nodiscard]] PageClassification classifyPage(int pageIndex) const;
    [[nodiscard]] std::vector<ImageObject> extractImages(int pageIndex) const;

    [[nodiscard]] Bitmap render(const RenderRequest& request) const;
    [[nodiscard]] bool deviceToPage(int pageIndex, const RenderRequest& request, float deviceX,
                                    float deviceY, PointF& pagePoint) const;
    [[nodiscard]] bool pageToDevice(int pageIndex, const RenderRequest& request, float pageX,
                                    float pageY, PointF& devicePoint) const;

    // Lossless copy of the original file when unmodified; otherwise writes the
    // in-memory edited document. Never overwrites the original path.
    void writeCopy(const std::filesystem::path& destination) const;

    // Persist the in-memory document (including edits) via PDFium SaveAsCopy.
    void save(const std::filesystem::path& destination);

    bool undo();

    void replaceSpanText(const TextSpan& span, const std::string& utf8);
    void setSpanColor(const TextSpan& span, const Color& color);
    void setSpanFontSize(const TextSpan& span, float fontSize);
    void editSpan(const TextSpan& span, const std::string& utf8, float fontSize, const Color& color);
    void replaceRegion(const std::vector<TextSpan>& spans, const std::string& utf8, float fontSize,
                       const Color& color, RectF box = {});
    void replaceRegion(int pageIndex, RectF box, const std::vector<TextSpan>& spans,
                       const std::string& utf8, float fontSize, const Color& color,
                       std::optional<PointF> destOrigin = std::nullopt);
    void deleteSpan(const TextSpan& span);
    void addText(int pageIndex, PointF pagePoint, const std::string& utf8, float fontSize,
                 const Color& color);
    void addImage(int pageIndex, RectF pageRect, const Bitmap& bitmap);
    void setImageRect(const ImageObject& image, RectF pageRect);
    void deleteImage(const ImageObject& image);

    void setPageRotation(int pageIndex, int quarterTurns);
    void deletePage(int pageIndex);
    void insertBlankPage(int atIndex, SizeF size);
    void importPages(const std::filesystem::path& sourcePdf, int atIndex);

    void addInvisibleOcrLayer(int pageIndex, const OcrPageResult& ocr, float sourceDpi);

private:
    PdfDocument(std::shared_ptr<PdfiumRuntime> runtime, std::filesystem::path path,
                std::vector<std::uint8_t> bytes, void* document);

    void markDirtyLocked();
    void bakeLocked();
    void rewriteSpanLocked(const TextSpan& span, const std::string& utf8, float fontSize,
                           const Color& color);
    void rewriteSpansLocked(const std::vector<TextSpan>& spans, const std::string& utf8,
                            float fontSize, const Color& color);
    void rewriteRegionLocked(int pageIndex, RectF box, const std::vector<TextSpan>& spans,
                             const std::string& utf8, float fontSize, const Color& color,
                             std::optional<PointF> destOrigin);
    std::vector<std::uint8_t> saveToMemoryLocked() const;
    void reloadFromBytesLocked(std::vector<std::uint8_t> bytes);
    void writeBytesToPath(const std::vector<std::uint8_t>& bytes,
                          const std::filesystem::path& destination) const;

    std::shared_ptr<PdfiumRuntime> runtime_;
    std::filesystem::path path_;
    std::vector<std::uint8_t> bytes_;
    void* document_ = nullptr;  // FPDF_DOCUMENT
    int pageCount_ = 0;
    bool dirty_ = false;
    std::vector<std::vector<std::uint8_t>> undo_;
};

}  // namespace pdfforge
