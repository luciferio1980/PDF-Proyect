# Testing

## Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

The `pdfforge_tests` binary also runs directly:

```bash
./build/pdfforge_tests
```

## Fixtures

`resources/test_documents/generate_test_pdfs.py` writes:

| File | Intent |
| ---- | ------ |
| TEST_01_SIMPLE_TEXT.pdf | Two pages, `TOTAL: 1.250,00 EUR` |
| TEST_02_MULTIPLE_FONTS.pdf | Helvetica / Times / Courier |
| TEST_03_BOLD_ITALIC.pdf | Bold and oblique |
| TEST_04_ROTATED_TEXT.pdf | 90° text matrix |
| TEST_05_SCANNED_DOCUMENT.pdf | Image-only page → OCR_REQUIRED |
| TEST_06_TABLE.pdf | Grid of positioned strings |
| TEST_07_MULTICOLUMN.pdf | Two columns |
| TEST_08_EMBEDDED_FONT.pdf | Type3 embedded glyph |
| TEST_09_MIXED_CONTENT.pdf | Text + image |
| TEST_10_SPLIT_TEXT_RUNS.pdf | Adjacent Tj runs that extract as one span |
| TEST_11_FORM_XOBJECT_TEXT.pdf | Text inside a Form XObject |

## Coverage in Alpha 0.1

- Open / missing file / all fixtures
- Text extraction of the amount string, fonts, bold/italic
- Search
- Classification including scanned pages
- Save copy then reopen
- Render size, rotation, cache, PSNR/SSIM
- Font matcher
- OCR on a rendered page (skipped if Tesseract is absent)
- Logger redaction

## Rule

BUG → FIX → TEST. New regressions get a case under `tests/`.
