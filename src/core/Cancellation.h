#pragma once

#include <atomic>
#include <cstdint>

namespace pdfforge {

class CancellationToken {
public:
    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> cancelled_{false};
};

}  // namespace pdfforge
