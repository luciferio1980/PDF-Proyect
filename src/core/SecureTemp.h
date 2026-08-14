#pragma once

#include <filesystem>
#include <string>

namespace pdfforge {

class SecureTempFile {
public:
    SecureTempFile();
    explicit SecureTempFile(std::string prefix);
    ~SecureTempFile();

    SecureTempFile(const SecureTempFile&) = delete;
    SecureTempFile& operator=(const SecureTempFile&) = delete;
    SecureTempFile(SecureTempFile&&) noexcept;
    SecureTempFile& operator=(SecureTempFile&&) noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    void keep() noexcept { keep_ = true; }

private:
    void cleanup() noexcept;

    std::filesystem::path path_;
    bool keep_ = false;
};

}  // namespace pdfforge
