#include "pdf/PdfSearch.h"

#include <algorithm>
#include <cctype>

namespace pdfforge {
namespace {

std::string fold(std::string s, bool insensitive) {
    if (!insensitive) {
        return s;
    }
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

}  // namespace

std::vector<SearchHit> searchSpans(const std::vector<TextSpan>& spans, const SearchQuery& query) {
    std::vector<SearchHit> hits;
    if (query.needle.empty()) {
        return hits;
    }
    const std::string needle = fold(query.needle, query.caseInsensitive);
    for (std::size_t i = 0; i < spans.size(); ++i) {
        const std::string hay = fold(spans[i].text, query.caseInsensitive);
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) {
            SearchHit hit;
            hit.pageIndex = spans[i].pageIndex;
            hit.spanIndex = static_cast<int>(i);
            hit.matchedText = spans[i].text.substr(pos, needle.size());
            hit.bounds = spans[i].bounds();
            hit.pdfCharStart = spans[i].pdfCharStart;
            hit.pdfCharEnd = spans[i].pdfCharEnd;
            hits.push_back(std::move(hit));
            pos += needle.empty() ? 1 : needle.size();
        }
    }
    return hits;
}

}  // namespace pdfforge
