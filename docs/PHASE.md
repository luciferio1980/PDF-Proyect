# Alpha 0.2 status

Completed:

- Environment audit
- License review and engine selection (PDFium + QPDF + Tesseract + Qt 6)
- CMake project
- Core document/render/extract/search/classify/OCR/font matching
- Qt viewer: open, zoom, navigate, rotate, thumbnails, search
- Portable Windows zip with Visual C++ runtime
- Content-stream text editing via PDFium page objects (not overlay)
- Text replace regenerates the original content stream so new glyphs do not stack on the old ones
- Edits collect every text run in the selected span (split Tj / Form XObjects) before rewriting
- Inspector: text, size, color
- Add / delete text objects
- Save / Save As of the edited in-memory document
- Page rotate / delete / insert blank / insert PDF
- Optional OCR → invisible searchable text layer (Linux builds)
- Undo of recent mutations
- Automated tests and generated fixtures

Not started:

- Scanned-page inpainting / visible OCR reconstruction
- Annotations, forms, digital signatures, redaction
- Crash recovery UI
- Windows installer
