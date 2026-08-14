# PDFForge

Independent professional PDF editor.

PDFForge is a desktop application for viewing, analysing, and (in later
milestones) editing PDF documents locally. It is an original product. It is not
an official alternative to any other vendor.

**Current milestone: Alpha 0.1.1 — functional viewer + text extraction + OCR engine.**

## Run it (Windows)

Download the portable zip from the latest Alpha release:

https://github.com/luciferio1980/PDF-Proyect/releases/tag/v0.1.1-alpha

Unzip the whole archive and double-click `PDFForge.exe` (or `Abrir PDFForge.bat`).
A sample PDF opens automatically. Do not run the exe from inside the zip.

## What works now

- Open PDF files with size and page-count limits
- Render pages at configurable DPI, zoom, and rotation
- Page navigation and thumbnails
- Text extraction with coordinates, font name, size, weight, italic, color, baseline, rotation
- Search
- Scanned-page detection (`OCR_REQUIRED` when a page has no significant text)
- Local OCR via Tesseract (never runs automatically on open)
- Font classification and substitution ranking
- Lossless save-copy (original file is never overwritten by Save copy)
- Pixel comparison helpers (MSE / PSNR / SSIM) for quality tests

Text editing of the PDF content stream is **intentionally not in this milestone**.
The viewer and extraction pipeline must stay correct first.

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
