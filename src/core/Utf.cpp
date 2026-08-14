#include "core/Utf.h"

#include <sstream>

namespace pdfforge {

std::string utf16LeToUtf8(const unsigned short* data, std::size_t unitsIncludingNul) {
    if (data == nullptr || unitsIncludingNul == 0) {
        return {};
    }
    std::size_t units = unitsIncludingNul;
    if (data[unitsIncludingNul - 1] == 0) {
        units -= 1;
    }
    std::string out;
    out.reserve(units);
    for (std::size_t i = 0; i < units; ++i) {
        const char32_t cp = data[i];
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::string narrowPath(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

}  // namespace pdfforge
