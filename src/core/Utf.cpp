#include "core/Utf.h"

#include <sstream>

namespace pdfforge {
namespace {

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace

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
        char32_t cp = data[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units) {
            const char32_t low = data[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        appendUtf8(out, cp);
    }
    return out;
}

std::vector<std::uint16_t> utf8ToUtf16Le(std::string_view utf8) {
    std::vector<std::uint16_t> out;
    out.reserve(utf8.size() + 1);
    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        char32_t cp = 0;
        std::size_t n = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6 && i + 1 < utf8.size()) {
            cp = (static_cast<char32_t>(c & 0x1F) << 6) |
                 (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
            n = 2;
        } else if ((c >> 4) == 0xE && i + 2 < utf8.size()) {
            cp = (static_cast<char32_t>(c & 0x0F) << 12) |
                 ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
            n = 3;
        } else if ((c >> 3) == 0x1E && i + 3 < utf8.size()) {
            cp = (static_cast<char32_t>(c & 0x07) << 18) |
                 ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
            n = 4;
        } else {
            ++i;
            continue;
        }
        i += n;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<std::uint16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<std::uint16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<std::uint16_t>(cp));
        }
    }
    out.push_back(0);
    return out;
}

std::string replaceUtf8Once(const std::string& haystack, const std::string& needle,
                            const std::string& replacement) {
    if (needle.empty()) {
        return replacement;
    }
    const auto pos = haystack.find(needle);
    if (pos == std::string::npos) {
        return replacement;
    }
    std::string out = haystack;
    out.replace(pos, needle.size(), replacement);
    return out;
}

std::string narrowPath(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

}  // namespace pdfforge
