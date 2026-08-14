#pragma once

#include <string>

namespace pdfforge {

struct FontEstimate {
    std::string family;
    int weight = 400;
    bool italic = false;
    bool serif = false;
    bool monospace = false;
    float size = 0;
};

FontEstimate interpretPdfFontFlags(const std::string& fontName, int pdfFlags, int pdfWeight);

}  // namespace pdfforge
