#pragma once

#include "renderer/Bitmap.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pdfforge {

class RenderCache {
public:
    explicit RenderCache(std::size_t maxEntries = 32);

    [[nodiscard]] std::string makeKey(const RenderRequest& request) const;
    bool tryGet(const std::string& key, Bitmap& out) const;
    void put(std::string key, Bitmap bitmap);
    void clear();

private:
    std::size_t maxEntries_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Bitmap> map_;
    std::vector<std::string> order_;
};

class PdfRenderer {
public:
    explicit PdfRenderer(std::size_t cacheEntries = 32);

    [[nodiscard]] Bitmap renderCached(class PdfDocument& document, const RenderRequest& request);
    void invalidate();

private:
    RenderCache cache_;
};

}  // namespace pdfforge
