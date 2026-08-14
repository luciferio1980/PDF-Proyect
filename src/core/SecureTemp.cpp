#include "core/SecureTemp.h"

#include "core/Error.h"
#include "core/Logger.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace pdfforge {
namespace {

std::filesystem::path makeTempPath(const std::string& prefix) {
    const auto dir = std::filesystem::temp_directory_path() / "pdfforge";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw Error(Status::IoError, "cannot create temp directory: " + ec.message());
    }

    std::random_device rd;
    std::mt19937_64 gen(rd());
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto n = gen();
    std::ostringstream name;
    name << prefix << '-' << now << '-' << n << ".tmp";
    return dir / name.str();
}

}  // namespace

SecureTempFile::SecureTempFile() : SecureTempFile("pdfforge") {}

SecureTempFile::SecureTempFile(std::string prefix) : path_(makeTempPath(prefix)) {
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw Error(Status::IoError, "cannot create temp file");
    }
}

SecureTempFile::~SecureTempFile() {
    cleanup();
}

SecureTempFile::SecureTempFile(SecureTempFile&& other) noexcept
    : path_(std::move(other.path_)), keep_(other.keep_) {
    other.keep_ = true;
    other.path_.clear();
}

SecureTempFile& SecureTempFile::operator=(SecureTempFile&& other) noexcept {
    if (this != &other) {
        cleanup();
        path_ = std::move(other.path_);
        keep_ = other.keep_;
        other.keep_ = true;
        other.path_.clear();
    }
    return *this;
}

void SecureTempFile::cleanup() noexcept {
    if (keep_ || path_.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(path_, ec);
    if (ec) {
        Logger::instance().warning("temp", "failed to remove temporary file");
    }
    path_.clear();
}

}  // namespace pdfforge
