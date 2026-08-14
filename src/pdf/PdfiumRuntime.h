#pragma once

#include <memory>
#include <mutex>

namespace pdfforge {

// Process-wide PDFium lifetime. PDFium's C API is not thread-safe; every call
// must be made while holding lock(). JavaScript is not initialized: we use the
// non-V8 binary distribution.
class PdfiumRuntime {
public:
    static std::shared_ptr<PdfiumRuntime> acquire();

    PdfiumRuntime(const PdfiumRuntime&) = delete;
    PdfiumRuntime& operator=(const PdfiumRuntime&) = delete;

    std::unique_lock<std::mutex> lock();

    ~PdfiumRuntime();

private:
    PdfiumRuntime();

    std::mutex mutex_;
};

}  // namespace pdfforge
