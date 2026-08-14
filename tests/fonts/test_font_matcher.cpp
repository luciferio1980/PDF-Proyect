#include "fonts/FontDetector.h"
#include "fonts/FontMatcher.h"
#include "tests/TestHarness.h"

TEST(FontFlagsDetectItalicAndSerif) {
    const auto e = pdfforge::interpretPdfFontFlags("Times-BoldItalic", (1 << 1) | (1 << 6), 400);
    CHECK(e.italic);
    CHECK(e.serif);
    CHECK(e.weight >= 700);
}

TEST(FontMatcherPrefersArialGroupForHelvetica) {
    pdfforge::FontEstimate e;
    e.family = "Helvetica-Bold";
    e.weight = 700;
    e.serif = false;
    const auto matches = pdfforge::matchFonts(e, 5);
    CHECK(!matches.empty());
    CHECK(matches.front().score >= 80);
    bool swiss = false;
    for (const auto& m : matches) {
        if (m.family == "Arial" || m.family == "Helvetica" || m.family == "Liberation Sans") {
            swiss = true;
        }
    }
    CHECK(swiss);
}

TEST(FontMatcherPrefersTimesGroup) {
    pdfforge::FontEstimate e;
    e.family = "TimesNewRomanPSMT";
    e.serif = true;
    const auto matches = pdfforge::matchFonts(e, 4);
    CHECK(!matches.empty());
    CHECK(matches.front().family.find("Times") != std::string::npos ||
          matches.front().family.find("Liberation Serif") != std::string::npos);
}
