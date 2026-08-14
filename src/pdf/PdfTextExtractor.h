#pragma once

#include "model/Objects.h"

#include "fpdf_text.h"

#include <vector>

namespace pdfforge {

std::vector<TextSpan> extractTextSpans(FPDF_TEXTPAGE textPage, int pageIndex);

}  // namespace pdfforge
