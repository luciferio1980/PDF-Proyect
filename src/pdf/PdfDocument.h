#pragma once

#include "core/Error.h"
#include "core/Geometry.h"
#include "model/Objects.h"
#include "model/PageClassification.h"
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

    void writeCopy(const std::filesystem::path& destination) const;

private:
    PdfDocument(std::shared_ptr<PdfiumRuntime> runtime, std::filesystem::path path,
                std::vector<std::uint8_t> bytes, void* document);

    std::shared_ptr<PdfiumRuntime> runtime_;
    std::filesystem::path path_;
    std::vector<std::uint8_t> bytes_;
    void* document_ = nullptr;  // FPDF_DOCUMENT
    int pageCount_ = 0;
};

}  // namespace pdfforge
