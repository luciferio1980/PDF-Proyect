#pragma once

#include <filesystem>

namespace pdfforge {

void qpdfWriteCopy(const std::filesystem::path& source, const std::filesystem::path& destination);

}  // namespace pdfforge
