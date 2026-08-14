#!/usr/bin/env python3
"""Generate deterministic PDFForge test fixtures. No third-party PDF libraries."""

from __future__ import annotations

import sys
from pathlib import Path


class PdfBuilder:
    def __init__(self) -> None:
        self._objects: list[bytes] = []

    def add(self, body: bytes | str) -> int:
        if isinstance(body, str):
            body = body.encode("latin-1")
        self._objects.append(body)
        return len(self._objects)

    def stream(self, dictionary: str, data: bytes) -> int:
        header = f"<< {dictionary} /Length {len(data)} >>\nstream\n".encode("latin-1")
        return self.add(header + data + b"\nendstream")

    def finish(self, root: int = 1, info: int | None = None) -> bytes:
        out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
        offsets = [0]
        for i, body in enumerate(self._objects, start=1):
            offsets.append(len(out))
            out += f"{i} 0 obj\n".encode("ascii")
            out += body
            if not body.endswith(b"\n"):
                out += b"\n"
            out += b"endobj\n"
        xref = len(out)
        out += f"xref\n0 {len(self._objects) + 1}\n".encode("ascii")
        out += b"0000000000 65535 f \n"
        for off in offsets[1:]:
            out += f"{off:010d} 00000 n \n".encode("ascii")
        trailer = f"<< /Size {len(self._objects) + 1} /Root {root} 0 R"
        if info is not None:
            trailer += f" /Info {info} 0 R"
        trailer += " >>"
        out += f"trailer\n{trailer}\nstartxref\n{xref}\n%%EOF\n".encode("latin-1")
        return bytes(out)


def _font(base: str, flags: int | None = None) -> str:
    extra = f"/Flags {flags} " if flags is not None else ""
    return f"<< /Type /Font /Subtype /Type1 /BaseFont /{base} {extra}>>"


def _content(*ops: str) -> bytes:
    return ("\n".join(ops) + "\n").encode("latin-1")


def write(path: Path, data: bytes) -> None:
    path.write_bytes(data)
    print(f"wrote {path.name} ({len(data)} bytes)")


def make_simple(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 2 /Kids [3 0 R 6 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 24 Tf",
            "1 0 0 1 72 720 Tm",
            "(Hello PDFForge) Tj",
            "0 -36 Td",
            "/F1 12 Tf",
            "(TOTAL: 1.250,00 EUR) Tj",
            "0 -24 Td",
            "(Independent professional PDF editor) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 14 Tf",
            "1 0 0 1 72 720 Tm",
            "(Page two of PDFForge) Tj",
            "ET",
        ),
    )
    info = pdf.add("<< /Producer (PDFForge Test Generator) /Title (TEST_01_SIMPLE_TEXT) >>")
    write(path, pdf.finish(root=1, info=info))


def make_multiple_fonts(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R /F2 6 0 R /F3 7 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 16 Tf",
            "1 0 0 1 72 720 Tm",
            "(Helvetica sample) Tj",
            "0 -28 Td",
            "/F2 16 Tf",
            "(Times Roman sample) Tj",
            "0 -28 Td",
            "/F3 16 Tf",
            "(Courier sample) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    pdf.add(_font("Times-Roman"))
    pdf.add(_font("Courier"))
    write(path, pdf.finish())


def make_bold_italic(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R /F2 6 0 R /F3 7 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 16 Tf",
            "1 0 0 1 72 700 Tm",
            "(Regular Helvetica) Tj",
            "0 -28 Td",
            "/F2 16 Tf",
            "(Bold Helvetica) Tj",
            "0 -28 Td",
            "/F3 16 Tf",
            "(Italic Helvetica) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    pdf.add(_font("Helvetica-Bold"))
    pdf.add(_font("Helvetica-Oblique"))
    write(path, pdf.finish())


def make_rotated(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 18 Tf",
            "1 0 0 1 72 720 Tm",
            "(Upright) Tj",
            "/F1 18 Tf",
            "0 1 -1 0 400 200 Tm",
            "(ROTATED) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def _gray_image(width: int, height: int) -> bytes:
    rows = bytearray()
    for y in range(height):
        for x in range(width):
            v = (x * 3 + y * 5) % 180 + 40
            rows.append(v & 0xFF)
    return bytes(rows)


def make_scanned(path: Path) -> None:
    pdf = PdfBuilder()
    img = _gray_image(128, 128)
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream("", _content("q", "612 0 0 792 0 0 cm", "/Im0 Do", "Q"))
    pdf.stream(
        "/Type /XObject /Subtype /Image /Width 128 /Height 128 "
        "/ColorSpace /DeviceGray /BitsPerComponent 8",
        img,
    )
    write(path, pdf.finish())


def make_table(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>"
    )
    ops = ["BT", "/F1 12 Tf"]
    rows = [
        ("Item", "Qty", "Amount"),
        ("Paper", "10", "12.00"),
        ("Ink", "2", "8.50"),
        ("TOTAL", "12", "20.50"),
    ]
    y = 720
    for row in rows:
        ops.append(f"1 0 0 1 72 {y} Tm")
        ops.append(f"({row[0]}) Tj")
        ops.append(f"1 0 0 1 220 {y} Tm")
        ops.append(f"({row[1]}) Tj")
        ops.append(f"1 0 0 1 320 {y} Tm")
        ops.append(f"({row[2]}) Tj")
        y -= 22
    ops.append("ET")
    pdf.stream("", _content(*ops))
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def make_multicolumn(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 11 Tf",
            "1 0 0 1 72 720 Tm",
            "(Left column paragraph one.) Tj",
            "0 -16 Td",
            "(Left column paragraph two.) Tj",
            "1 0 0 1 320 720 Tm",
            "(Right column paragraph one.) Tj",
            "0 -16 Td",
            "(Right column paragraph two.) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def make_embedded_type3(path: Path) -> None:
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R /F2 6 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 14 Tf",
            "1 0 0 1 72 720 Tm",
            "(Embedded Type3 glyph follows) Tj",
            "/F2 24 Tf",
            "1 0 0 1 72 680 Tm",
            "(A) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    glyph = _content("600 0 0 0 700 700 d1", "20 20 560 660 re", "f")
    charproc = pdf.stream("", glyph)
    pdf.add(
        f"<< /Type /Font /Subtype /Type3 /Name /ForgeT3 /FontBBox [0 0 600 700] "
        f"/FontMatrix [0.001 0 0 0.001 0 0] /CharProcs << /A {charproc} 0 R >> "
        f"/Encoding << /Type /Encoding /Differences [65 /A] >> "
        f"/FirstChar 65 /LastChar 65 /Widths [600] >>"
    )
    write(path, pdf.finish())


def make_mixed(path: Path) -> None:
    pdf = PdfBuilder()
    img = _gray_image(48, 48)
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 6 0 R >> /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "q",
            "120 0 0 120 72 600 cm",
            "/Im0 Do",
            "Q",
            "BT",
            "/F1 14 Tf",
            "1 0 0 1 220 680 Tm",
            "(Mixed text and image) Tj",
            "ET",
        ),
    )
    pdf.stream(
        "/Type /XObject /Subtype /Image /Width 48 /Height 48 "
        "/ColorSpace /DeviceGray /BitsPerComponent 8",
        img,
    )
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def make_split_runs(path: Path) -> None:
    """Adjacent same-style Tj runs that extract as one span (Word-like)."""
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream(
        "",
        _content(
            "BT",
            "/F1 12 Tf",
            "1 0 0 1 72 720 Tm",
            "(3. ) Tj",
            "(Cuantia del contrato) Tj",
            "0 -24 Td",
            "(Keep this sibling line) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def make_form_text(path: Path) -> None:
    """Text lives inside a Form XObject, not the page content stream."""
    pdf = PdfBuilder()
    pdf.add("<< /Type /Catalog /Pages 2 0 R >>")
    pdf.add("<< /Type /Pages /Count 1 /Kids [3 0 R] >>")
    pdf.add(
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /XObject << /Fm0 5 0 R >> >> /Contents 4 0 R >>"
    )
    pdf.stream("", _content("q", "/Fm0 Do", "Q"))
    pdf.stream(
        "/Type /XObject /Subtype /Form /BBox [0 0 612 792] "
        "/Resources << /Font << /F1 6 0 R >> >>",
        _content(
            "BT",
            "/F1 24 Tf",
            "1 0 0 1 72 720 Tm",
            "(Hello FormText) Tj",
            "ET",
        ),
    )
    pdf.add(_font("Helvetica"))
    write(path, pdf.finish())


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_test_pdfs.py DEST_DIR", file=sys.stderr)
        return 2
    dest = Path(sys.argv[1])
    dest.mkdir(parents=True, exist_ok=True)
    make_simple(dest / "TEST_01_SIMPLE_TEXT.pdf")
    make_multiple_fonts(dest / "TEST_02_MULTIPLE_FONTS.pdf")
    make_bold_italic(dest / "TEST_03_BOLD_ITALIC.pdf")
    make_rotated(dest / "TEST_04_ROTATED_TEXT.pdf")
    make_scanned(dest / "TEST_05_SCANNED_DOCUMENT.pdf")
    make_table(dest / "TEST_06_TABLE.pdf")
    make_multicolumn(dest / "TEST_07_MULTICOLUMN.pdf")
    make_embedded_type3(dest / "TEST_08_EMBEDDED_FONT.pdf")
    make_mixed(dest / "TEST_09_MIXED_CONTENT.pdf")
    make_split_runs(dest / "TEST_10_SPLIT_TEXT_RUNS.pdf")
    make_form_text(dest / "TEST_11_FORM_XOBJECT_TEXT.pdf")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
