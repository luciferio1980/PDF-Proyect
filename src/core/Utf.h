#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pdfforge {

std::string utf16LeToUtf8(const unsigned short* data, std::size_t unitsIncludingNul);
std::vector<std::uint16_t> utf8ToUtf16Le(std::string_view utf8);
std::string replaceUtf8Once(const std::string& haystack, const std::string& needle,
                            const std::string& replacement);
std::string narrowPath(const std::filesystem::path& path);

}  // namespace pdfforge
