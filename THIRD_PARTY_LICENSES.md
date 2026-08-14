# Third-party licenses

PDFForge is a proprietary application. It **dynamically links** several open
source libraries. This table is the working inventory for Alpha 0.1.

Sources consulted: project LICENSE files and official documentation (Qt,
QPDF, Tesseract, PDFium), not secondary blogs.

| Library | Version (this environment / pin) | License | Commercial use | Obligations |
| ------- | -------------------------------- | ------- | -------------- | ----------- |
| Qt 6 (Core, Gui, Widgets) | 6.4.2 on Ubuntu CI; 6.x on Windows | LGPLv3 (Community) **or** Qt commercial | Yes, if you comply with the chosen Qt license | **LGPL path:** dynamic link only; allow relinking against a modified Qt; provide Qt source corresponding to the binaries you ship; do not statically link Qt. **Commercial Qt path:** follow The Qt Company agreement. Some Qt modules are GPL-only and are **not** used. Official: https://doc.qt.io/qt-6/licensing.html |
| PDFium | 153.0.7999.0 (`chromium/7999`), non-V8 | Apache-2.0 / BSD-3-Clause (project LICENSE) plus third-party notices in the binary `licenses/` tree (FreeType, zlib, libpng, libjpeg-turbo, OpenJPEG, LCMS, ICU, …) | Yes | Retain copyright notices; ship the aggregated `licenses/` files with the binary. Official: https://pdfium.googlesource.com/pdfium/+/refs/heads/main/LICENSE |
| pdfium-binaries packaging | chromium/7999 | MIT (packaging scripts) | Yes | Include the packaging MIT notice. https://github.com/bblanchon/pdfium-binaries |
| QPDF | 11.9.0 (Ubuntu) | Apache-2.0 | Yes | Preserve NOTICE/copyright; Apache-2.0 terms. Official: https://qpdf.readthedocs.io/en/stable/license.html |
| Tesseract OCR | 5.3.4 | Apache-2.0 | Yes | Preserve notices. Official: https://github.com/tesseract-ocr/tesseract |
| Leptonica (via Tesseract) | 1.82.0 | BSD-2-Clause | Yes | Preserve copyright. |
| Language data (`*.traineddata`) | tessdata for eng/spa/fra/deu/ita/por | Apache-2.0 (standard tessdata) | Yes | Redistribute the traineddata files you ship, with notices. |

## Rejected for the proprietary product

| Library | License | Why it was not used |
| ------- | ------- | ------------------- |
| Poppler | GPL-2.0 | Copyleft would apply to a proprietary desktop editor. |
| MuPDF | AGPL-3.0 (or a paid Artifex license) | AGPL is not compatible with a closed-source product unless you buy a commercial license from Artifex. We did not assume that license. |
| Qt PDF Widgets as the only engine | LGPLv3 | Usable, but the public QtPdf API does not expose per-character font/color/baseline as completely as PDFium’s C API, which we need for later editing. |

## Engine decision

**Render + extract:** PDFium (non-V8).  
**Structural save/copy (and later content-stream rewrite):** QPDF.  
**OCR:** Tesseract, local only.  
**UI:** Qt 6 Widgets, dynamically linked.

PDFium is used for display and for reconstructing `TextSpan` geometry. QPDF is
not used as a renderer. Editing will rewrite PDF objects through QPDF (or
PDFium edit APIs where they are sufficient); it will not paint white rectangles
over text.

## Shipping checklist (before any sale)

1. Confirm whether PDFForge will use Qt under LGPLv3 or a Qt commercial license.
2. Copy PDFium `licenses/` into the installer.
3. Include this file and Qt LGPL notices (if applicable) in the About dialog.
4. Do not statically link Qt on the LGPL path.
5. Have counsel review the set. This table is engineering diligence, not a legal opinion.
