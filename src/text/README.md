Text editing rewrites PDF page objects through PDFium
(`FPDFText_SetText` + `FPDFPage_GenerateContent`). It does not cover text
with a white rectangle.

The inspector and canvas caret are UI only. Committing an edit changes the
in-memory document; Save writes those objects back out.
