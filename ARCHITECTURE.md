# Architecture

## Product goal

PDF → analyse → OCR only if needed → editable structure → edit → reconstruct PDF.

Alpha 0.2 implements the loop for native PDF text: open, render, extract,
classify, search, optional OCR, then rewrite text objects and save. Inline
editing must not start from a “cover with a white box” approach. The canvas
caret is only a typing surface; commit calls `FPDFText_SetText` and
`FPDFPage_GenerateContent`.

## Process layout

```
┌──────────────────────────────────────────────────────────┐
│ Qt UI  (src/ui, src/app)                                 │
│  MainWindow, PdfCanvas, thumbnails, search               │
└──────────────┬───────────────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────────────┐
│ pdfforge_core                                            │
│  PdfDocument  → PDFium (render, text, objects)           │
│  QpdfBridge   → QPDF   (save copy / future rewrite)      │
│  OcrEngine    → Tesseract (local, opt-in)                │
│  FontMatcher / FontDetector                              │
│  ImageMetrics (PSNR / SSIM)                              │
└──────────────────────────────────────────────────────────┘
```

UI never talks to PDFium or Tesseract directly.

## Threads and locking

PDFium is not thread-safe. `PdfiumRuntime` owns the library lifetime and a
mutex. Every PDFium call runs while that mutex is held. Render requests copy
pixels out from under the lock so the UI can paint without holding it longer
than the raster itself.

Alpha 0.2 still rasterises the current page on the UI thread. That is acceptable
for the test documents; large pages should move to a worker.

## Document model

`TextSpan` is the unit later editing will target. It stores text, PDF user-space
box, baseline, font name, size, weight, italic, color, rotation, and the
originating PDFium character range (`pdfCharStart` / `pdfCharEnd`).

Coordinates stay in PDF user space (origin bottom-left). The canvas maps them
for hit-testing through `FPDF_DeviceToPage`.

## Classification

On demand (and when the UI shows a page), each page is classified:

| Kind | OCR |
| ---- | --- |
| TextOnly | NotNeeded |
| Mixed | NotNeeded |
| ImageOnly | Required |
| ProbablyScanned | Required |
| Empty | NotNeeded |

OCR is never started from `open()`.

## Security boundaries

- Non-V8 PDFium: no JS isolate.
- File size / page / bitmap caps in `OpenOptions` and `PdfDocument::render`.
- Logger redacts likely secrets.
- `writeCopy` refuses to overwrite the original path.

## Later modules (directories exist, not in Alpha 0.2)

annotations, forms, signatures, redaction, conversion, comparison UI,
licensing UI, crash recovery.
