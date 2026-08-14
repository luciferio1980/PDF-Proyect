#include "renderer/PdfRenderer.h"

#include "pdf/PdfDocument.h"

namespace pdfforge {

PdfRenderer::PdfRenderer(std::size_t cacheEntries) : cache_(cacheEntries) {}

Bitmap PdfRenderer::renderCached(PdfDocument& document, const RenderRequest& request) {
    const auto key = cache_.makeKey(request);
    Bitmap hit;
    if (cache_.tryGet(key, hit)) {
        return hit;
    }
    Bitmap rendered = document.render(request);
    cache_.put(key, rendered);
    return rendered;
}

void PdfRenderer::invalidate() {
    cache_.clear();
}

}  // namespace pdfforge
