# PDFForge

Independent professional PDF editor.

PDFForge is a desktop application for viewing, analysing, and (in later
milestones) editing PDF documents locally. It is an original product. It is not
an official alternative to any other vendor.

**Current milestone: Alpha 0.2 — viewer + real text editing (PDF object rewrite).**

## Run it (Windows)

Download the portable zip from the latest Alpha release:

https://github.com/luciferio1980/PDF-Proyect/releases/tag/v0.2.10-alpha

Unzip the whole archive and double-click `PDFForge.exe` (or `Abrir PDFForge.bat`).
The app starts on a home menu: Edit PDF or Sign PDF. Do not run the exe from inside the zip.

Choose Edit PDF, then drag a box around the text to recognize font and content and edit it. Hover the selection outline and drag to move the text. Use View → Inspector to show or hide the inspector. Choose Sign PDF to stamp one signature. File → Save writes the rewritten PDF.

## What works now

- Open PDF files with size and page-count limits
- Render pages at configurable DPI, zoom, and rotation
- Page navigation and thumbnails
- Text extraction with coordinates, font name, size, weight, italic, color, baseline, rotation
- Search
- Edit native PDF text by drawing a selection box (recognizes font and text; applying an edit erases what was in the box)
- Hover the selection outline (four-arrow cursor) and drag to move the text
- View → Inspector shows or hides the inspector panel
- Home menu to choose Edit PDF or Sign PDF
- Visual signature stamp from an image or a mouse-drawn signature
- One signature per Sign visit; click to select, resize, or delete; click away to deselect
- Saved signature library (up to 8, deletable)
- Change font size and color of a text object
- Add / delete text objects
- Save and Save As of the edited document
- Rotate / delete / insert pages, insert another PDF
- Scanned-page detection (`OCR_REQUIRED` when a page has no significant text)
- Local OCR via Tesseract (never runs automatically on open; Linux builds can stamp an invisible text layer)
- Font classification and substitution ranking
- Lossless save-copy of unmodified files (original file is never overwritten by Save copy)
- Pixel comparison helpers (MSE / PSNR / SSIM) for quality tests

Text editing of subsetted/custom encodings may fall back to a standard PDF
font (Helvetica) when the original object cannot encode the new characters.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

The Qt application binary is `build/PDFForge`.

```bash
./build/PDFForge /path/to/file.pdf
```

Headless tests do not require a display.

## Platform

The commercial target is Windows (MSVC + Qt 6). This repository also builds on
Linux so the engine can be developed and tested in CI. See `DEVELOPMENT.md`.

## Privacy

Processing is local. Documents are not uploaded. Telemetry is off. PDF JavaScript
is disabled by using a non-V8 PDFium build.

## License

PDFForge application source is proprietary. Third-party components keep their
own licenses; see `LICENSE.md` and `THIRD_PARTY_LICENSES.md`.
