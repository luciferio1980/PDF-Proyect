#pragma once

#include "model/Objects.h"

#include <string>
#include <vector>

namespace pdfforge {

struct SearchHit {
    int pageIndex = 0;
    int spanIndex = 0;
    std::string matchedText;
    RectF bounds;
    int pdfCharStart = -1;
    int pdfCharEnd = -1;
};

struct SearchQuery {
    std::string needle;
    bool caseInsensitive = true;
};

std::vector<SearchHit> searchSpans(const std::vector<TextSpan>& spans, const SearchQuery& query);

}  // namespace pdfforge
