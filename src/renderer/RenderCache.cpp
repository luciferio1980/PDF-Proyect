#include "renderer/PdfRenderer.h"

#include <algorithm>
#include <sstream>

namespace pdfforge {

RenderCache::RenderCache(std::size_t maxEntries) : maxEntries_(maxEntries == 0 ? 1 : maxEntries) {}

std::string RenderCache::makeKey(const RenderRequest& request) const {
    std::ostringstream os;
    os << request.pageIndex << ':' << request.dpi << ':' << request.rotationQuarterTurns << ':'
       << request.clipX << ':' << request.clipY << ':' << request.clipWidth << ':'
       << request.clipHeight;
    return os.str();
}

bool RenderCache::tryGet(const std::string& key, Bitmap& out) const {
    std::lock_guard lock(mutex_);
    const auto it = map_.find(key);
    if (it == map_.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void RenderCache::put(std::string key, Bitmap bitmap) {
    std::lock_guard lock(mutex_);
    if (map_.find(key) == map_.end() && order_.size() >= maxEntries_) {
        const auto oldest = order_.front();
        order_.erase(order_.begin());
        map_.erase(oldest);
    }
    map_[key] = std::move(bitmap);
    order_.erase(std::remove(order_.begin(), order_.end(), key), order_.end());
    order_.push_back(std::move(key));
}

void RenderCache::clear() {
    std::lock_guard lock(mutex_);
    map_.clear();
    order_.clear();
}

}  // namespace pdfforge
