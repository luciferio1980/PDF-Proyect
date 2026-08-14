#pragma once

#include "model/Objects.h"

#include "fpdf_edit.h"
#include "fpdf_text.h"
#include "fpdfview.h"

#include <vector>

namespace pdfforge {

std::vector<TextSpan> extractTextSpans(FPDF_PAGE page, FPDF_TEXTPAGE textPage, int pageIndex);

}  // namespace pdfforge
