# Alpha 0.2 status

Completed:

- Environment audit
- License review and engine selection (PDFium + QPDF + Tesseract + Qt 6)
- CMake project
- Core document/render/extract/search/classify/OCR/font matching
- Qt viewer: open, zoom, navigate, rotate, thumbnails, search
- Portable Windows zip with Visual C++ runtime
- Content-stream text editing via PDFium page objects (not overlay)
- Marquee selection recognizes font and text for editing
- Text replace regenerates the original content stream so new glyphs do not stack on the old ones
- Region apply deletes intersecting text objects and covers leftover image pixels in the selection
- The text edit box can be dragged from its selection outline (four-arrow cursor)
- Selected text uses an Acrobat-style box: hover the border, four-arrow cursor, drag to move
- The inline editor keeps the original font size and grows the selection box if needed
- View menu toggles the Inspector dock
- Edits collect every text run in the selected span (split Tj / Form XObjects) before rewriting
- Home menu: Edit PDF or Sign PDF
- Visual signature stamp (uploaded image or drawn)
- One signature per visit, selectable size, delete while selected
- Saved signature library (up to 8)
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
