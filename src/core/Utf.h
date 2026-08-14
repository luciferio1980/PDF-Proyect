#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pdfforge {

std::string utf16LeToUtf8(const unsigned short* data, std::size_t unitsIncludingNul);
std::string narrowPath(const std::filesystem::path& path);

}  // namespace pdfforge
