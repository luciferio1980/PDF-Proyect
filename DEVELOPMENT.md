# Development

## Environment used to build Alpha 0.1

Inspected on 2026-08-14 (Ubuntu 24.04 cloud agent, not Windows):

| Tool | Status |
| ---- | ------ |
| OS | Ubuntu 24.04.4 LTS, x86_64 |
| CMake | 3.28.3 |
| g++ | 13.3.0 (C++20) |
| clang++ | 18.1.3 |
| Ninja | installed via apt |
| Qt 6 | 6.4.2 (`qt6-base-dev`) |
| QPDF | 11.9.0 (`libqpdf-dev`) |
| Tesseract | 5.3.4 + eng/spa/fra/deu/ita/por |
| PDFium | downloaded pin `chromium/7999` |
| MSVC | not available here |
| qmake | not required (CMake) |

Windows/MSVC remains the shipping compiler. GitHub Actions builds Windows with
the same CMake project.

## Configure

```bash
cmake -S . -B build -G Ninja \
  -DPDFFORGE_BUILD_APP=ON \
  -DPDFFORGE_BUILD_TESTS=ON \
  -DPDFFORGE_WITH_QPDF=ON \
  -DPDFFORGE_WITH_TESSERACT=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

PDFium is fetched into `build/third_party/pdfium` during configure.

## Layout

See the tree in the repository root. Core code lives in `src/`. Tests live in
`tests/`. Generated fixtures land in the build directory, not in git.

## Code rules

- C++20, RAII, smart pointers, no PDFium handle leaks
- Do not log PDF contents or passwords
- Do not add GPL/AGPL libraries
- Do not implement text edit as “white rectangle + overlay”

## Known Alpha 0.1 limitations

- Page raster and thumbnail generation can block the UI on huge pages
- Hover highlight mapping is exact for hit-tests; painted highlight assumes
  rotation 0
- No content-stream editing yet
- Embedded font subsetting / licensing of customer fonts is not handled
- Digital signatures are not implemented
