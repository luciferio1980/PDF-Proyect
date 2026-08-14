#pragma once

#include "fonts/FontDetector.h"

#include <string>
#include <vector>

namespace pdfforge {

struct FontCandidate {
    std::string family;
    int weight = 400;
    bool italic = false;
    float score = 0;
    std::string reason;
};

std::vector<FontCandidate> matchFonts(const FontEstimate& detected, int maxResults = 5);

}  // namespace pdfforge
