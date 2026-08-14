#pragma once

#include "core/Geometry.h"
#include "model/PageClassification.h"

#include "fpdfview.h"

namespace pdfforge {

PageClassification classifyLoadedPage(FPDF_PAGE page, FPDF_TEXTPAGE text, SizeF pageSize);

}  // namespace pdfforge
