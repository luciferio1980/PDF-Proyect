#include "pdf/PdfiumRuntime.h"

#include "core/Logger.h"

#include "fpdfview.h"

#include <stdexcept>

namespace pdfforge {
namespace {

std::mutex gAcquireMutex;
std::weak_ptr<PdfiumRuntime> gInstance;

}  // namespace

std::shared_ptr<PdfiumRuntime> PdfiumRuntime::acquire() {
    std::lock_guard lock(gAcquireMutex);
    if (auto existing = gInstance.lock()) {
        return existing;
    }
    auto created = std::shared_ptr<PdfiumRuntime>(new PdfiumRuntime());
    gInstance = created;
    return created;
}

PdfiumRuntime::PdfiumRuntime() {
    FPDF_LIBRARY_CONFIG config{};
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);
    Logger::instance().info("pdfium", "library initialized (non-V8, JavaScript disabled)");
}

PdfiumRuntime::~PdfiumRuntime() {
    FPDF_DestroyLibrary();
    Logger::instance().info("pdfium", "library destroyed");
}

std::unique_lock<std::mutex> PdfiumRuntime::lock() {
    return std::unique_lock<std::mutex>(mutex_);
}

}  // namespace pdfforge
