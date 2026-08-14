#include "core/SecureTemp.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "tests/TestHarness.h"

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path fixture(const char* name) {
    return std::filesystem::path(PDFFORGE_TEST_PDF_DIR) / name;
}

std::filesystem::path copyFixture(const char* name, pdfforge::SecureTempFile& tmp) {
    const auto dest = tmp.path().string() + ".pdf";
    std::filesystem::copy_file(fixture(name), dest,
                               std::filesystem::copy_options::overwrite_existing);
    return dest;
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace

TEST(EditReplacesTextObjectAndSaves) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-edit-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-edit-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";

    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "PDFForge")) {
            target = s;
            break;
        }
    }
    CHECK(contains(target.text, "PDFForge"));
    doc->replaceSpanText(target, "PDFForge Edited");
    CHECK(contains(doc->extractPlainText(0), "PDFForge Edited"));
    CHECK(!contains(doc->extractPlainText(0), target.text));
    CHECK(doc->dirty());
    doc->save(dst);

    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "PDFForge Edited"));
    CHECK(!contains(reopened->extractPlainText(0), "Hello PDFForge"));
}

TEST(EditUndoRestoresPreviousText) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-undo-src");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const auto before = doc->extractPlainText(0);
    const auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    doc->replaceSpanText(spans.front(), "UNDOTEST");
    CHECK(contains(doc->extractPlainText(0), "UNDOTEST"));
    CHECK(doc->canUndo());
    CHECK(doc->undo());
    CHECK(doc->extractPlainText(0) == before);
}

TEST(DeletePageChangesCount) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-delpage");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    CHECK(doc->pageCount() >= 2);
    const int before = doc->pageCount();
    doc->deletePage(1);
    CHECK(doc->pageCount() == before - 1);
}

TEST(InsertBlankPageAndAddText) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-addpage");
    pdfforge::SecureTempFile dstTmp("pdfforge-addpage-out");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const int before = doc->pageCount();
    doc->insertBlankPage(before, doc->pageSize(0));
    CHECK(doc->pageCount() == before + 1);
    doc->addText(before, pdfforge::PointF{72.0f, 720.0f}, "Inserted by PDFForge", 18.0f,
                 pdfforge::Color::rgb(0, 0, 0));
    CHECK(contains(doc->extractPlainText(before), "Inserted by PDFForge"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(reopened->pageCount() == before + 1);
    CHECK(contains(reopened->extractPlainText(before), "Inserted by PDFForge"));
}

TEST(EditColorAndSizeKeepText) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-style");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    doc->editSpan(target, target.text, target.fontSize + 4.0f, pdfforge::Color::rgb(0.8f, 0.1f, 0.1f));
    const auto after = doc->extractText(0);
    CHECK(!after.empty());
    bool found = false;
    for (const auto& s : after) {
        if (contains(s.text, target.text) || contains(doc->extractPlainText(0), target.text)) {
            found = true;
        }
    }
    CHECK(found);
}
