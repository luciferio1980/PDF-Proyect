#include "fonts/FontMatcher.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace pdfforge {
namespace {

struct CatalogEntry {
    const char* family;
    bool serif;
    bool monospace;
    const char* group;  // metric-compatible group id
};

constexpr CatalogEntry kCatalog[] = {
    {"Arial", false, false, "swiss"},
    {"Helvetica", false, false, "swiss"},
    {"Liberation Sans", false, false, "swiss"},
    {"Noto Sans", false, false, "swiss"},
    {"DejaVu Sans", false, false, "swiss"},
    {"Calibri", false, false, "calibrilike"},
    {"Carlito", false, false, "calibrilike"},
    {"Verdana", false, false, "verdana"},
    {"Tahoma", false, false, "tahoma"},
    {"Times New Roman", true, false, "roman"},
    {"Times", true, false, "roman"},
    {"Liberation Serif", true, false, "roman"},
    {"Noto Serif", true, false, "roman"},
    {"Georgia", true, false, "georgia"},
    {"Courier New", false, true, "modern"},
    {"Courier", false, true, "modern"},
    {"Liberation Mono", false, true, "modern"},
    {"Noto Sans Mono", false, true, "modern"},
};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string stripStyle(std::string name) {
    name = lower(std::move(name));
    const char* tails[] = {",bolditalic", ",bold", ",italic", ",oblique", " bold italic",
                           " bold", " italic", " oblique", " regular", " mt"};
    for (const char* t : tails) {
        const auto pos = name.find(t);
        if (pos != std::string::npos) {
            name.erase(pos);
        }
    }
    // PDF subset prefix: ABCDEF+FontName
    const auto plus = name.find('+');
    if (plus != std::string::npos && plus <= 7) {
        name = name.substr(plus + 1);
    }
    return name;
}

}  // namespace

std::vector<FontCandidate> matchFonts(const FontEstimate& detected, int maxResults) {
    const std::string want = stripStyle(detected.family);
    std::vector<FontCandidate> out;
    for (const auto& entry : kCatalog) {
        FontCandidate c;
        c.family = entry.family;
        c.weight = detected.weight;
        c.italic = detected.italic;
        c.score = 0;
        const std::string have = lower(entry.family);
        if (!want.empty() && (have == want || want.find(have) != std::string::npos ||
                              have.find(want) != std::string::npos)) {
            c.score += 100;
            c.reason = "family name";
        } else {
            std::string wantGroup;
            for (const auto& e2 : kCatalog) {
                if (stripStyle(e2.family) == want || want.find(lower(e2.family)) != std::string::npos) {
                    wantGroup = e2.group;
                    break;
                }
            }
            if (!wantGroup.empty() && wantGroup == entry.group) {
                c.score += 82;
                c.reason = "metric-compatible family";
            } else if (entry.serif == detected.serif && entry.monospace == detected.monospace) {
                c.score += 40;
                c.reason = "classification match";
            } else if (entry.monospace == detected.monospace) {
                c.score += 15;
                c.reason = "pitch match";
            }
        }
        if (entry.serif == detected.serif) {
            c.score += 8;
        }
        if (entry.monospace == detected.monospace) {
            c.score += 12;
        }
        out.push_back(std::move(c));
    }
    std::sort(out.begin(), out.end(), [](const FontCandidate& a, const FontCandidate& b) {
        return a.score > b.score;
    });
    if (static_cast<int>(out.size()) > maxResults) {
        out.resize(static_cast<std::size_t>(maxResults));
    }
    return out;
}

}  // namespace pdfforge
