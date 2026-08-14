# Security

## Default posture

- All document processing is local to the machine.
- PDF JavaScript is not executed. PDFForge links the **non-V8** PDFium build.
- Telemetry is off and is not compiled into Alpha 0.1.
- Passwords, private keys, and document contents are not written to logs.
- Automatic OCR is not started when a file is opened. Pages may be marked
  `OCR_REQUIRED` without running the recognizer.

## Limits enforced on open

| Limit | Default | Purpose |
| ----- | ------- | ------- |
| File size | 512 MiB | Memory bound |
| Page count | 10 000 | CPU / memory bound |
| Render bitmap | 8192 px on a side | Memory bound |
| Render DPI | 1–1200 | CPU bound |

## Untrusted documents

Treat every PDF as untrusted input. PDFium is a large C/C++ parser. Keep the
library updated. Do not load documents from hostile sources in a privileged
process.

## Secrets

- Certificate private keys must never be logged, uploaded, or stored in plain
  text. Digital signatures are not implemented in this milestone.
- File log sink writes under the per-user application data directory.

## Reporting

If you find a vulnerability in PDFForge itself, report it privately to the
maintainers. Do not attach customer documents.
