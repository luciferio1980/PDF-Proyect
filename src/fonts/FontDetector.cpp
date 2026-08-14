#include "fonts/FontDetector.h"

#include <algorithm>
#include <cctype>

namespace pdfforge {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

}  // namespace

FontEstimate interpretPdfFontFlags(const std::string& fontName, int pdfFlags, int pdfWeight) {
    FontEstimate e;
    e.family = fontName;
    e.weight = pdfWeight > 0 ? pdfWeight : 400;
    e.italic = (pdfFlags & (1 << 6)) != 0;       // bit 7
    e.serif = (pdfFlags & (1 << 1)) != 0;        // bit 2
    e.monospace = (pdfFlags & (1 << 0)) != 0;    // bit 1
    if (pdfFlags & (1 << 18)) {                  // bit 19 ForceBold
        e.weight = std::max(e.weight, 700);
    }
    const std::string n = lower(fontName);
    if (n.find("bold") != std::string::npos) {
        e.weight = std::max(e.weight, 700);
    }
    if (n.find("italic") != std::string::npos || n.find("oblique") != std::string::npos) {
        e.italic = true;
    }
    if (n.find("times") != std::string::npos || n.find("georgia") != std::string::npos ||
        n.find("garamond") != std::string::npos || n.find("serif") != std::string::npos) {
        e.serif = true;
    }
    if (n.find("arial") != std::string::npos || n.find("helvetica") != std::string::npos ||
        n.find("sans") != std::string::npos || n.find("calibri") != std::string::npos) {
        e.serif = false;
    }
    if (n.find("courier") != std::string::npos || n.find("mono") != std::string::npos) {
        e.monospace = true;
    }
    return e;
}

}  // namespace pdfforge
