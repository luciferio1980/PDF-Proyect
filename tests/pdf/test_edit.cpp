#include "comparison/ImageMetrics.h"
#include "core/SecureTemp.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "pdf/RegionRecognize.h"
#include "tests/TestHarness.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <algorithm>

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
    CHECK(!contains(doc->extractPlainText(0), "Hello PDFForge"));
    CHECK(contains(doc->extractPlainText(0), "TOTAL: 1.250,00 EUR"));
    CHECK(doc->dirty());
    doc->save(dst);

    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "PDFForge Edited"));
    CHECK(!contains(reopened->extractPlainText(0), "Hello PDFForge"));
    CHECK(contains(reopened->extractPlainText(0), "TOTAL: 1.250,00 EUR"));
    std::ifstream in(dst, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!contains(saved, "Hello PDFForge"));
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

TEST(EditChangesRenderedPixelsAndSurvivesReopen) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-render-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-render-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    pdfforge::RenderRequest req;
    req.pageIndex = 0;
    req.dpi = 72.0f;
    const auto before = doc->render(req);
    auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "PDFForge")) {
            target = s;
            break;
        }
    }
    doc->replaceSpanText(target, "ZZZZ EDIT STICKS");
    CHECK(contains(doc->extractPlainText(0), "ZZZZ EDIT STICKS"));
    CHECK(!contains(doc->extractPlainText(0), "Hello PDFForge"));
    CHECK(contains(doc->extractPlainText(0), "TOTAL: 1.250,00 EUR"));
    const auto after = doc->render(req);
    const auto diff = pdfforge::compareBitmaps(before, after);
    CHECK(diff.differentPixels > 10);
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "ZZZZ EDIT STICKS"));
    CHECK(!contains(reopened->extractPlainText(0), "Hello PDFForge"));
    CHECK(contains(reopened->extractPlainText(0), "TOTAL: 1.250,00 EUR"));
    const auto again = reopened->render(req);
    const auto diffSaved = pdfforge::compareBitmaps(before, again);
    CHECK(diffSaved.differentPixels > 10);
    std::ifstream in(dst, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!contains(saved, "Hello PDFForge"));
}

TEST(EditDoesNotStackOriginalTextOnRepeatedReplace) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-stack-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-stack-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "PDFForge")) {
            target = s;
            break;
        }
    }
    doc->replaceSpanText(target, "FIRST REPLACE");
    auto mid = doc->extractText(0);
    CHECK(!mid.empty());
    pdfforge::TextSpan again = mid.front();
    for (const auto& s : mid) {
        if (contains(s.text, "FIRST REPLACE")) {
            again = s;
            break;
        }
    }
    doc->replaceSpanText(again, "SECOND REPLACE");
    const auto plain = doc->extractPlainText(0);
    CHECK(contains(plain, "SECOND REPLACE"));
    CHECK(!contains(plain, "FIRST REPLACE"));
    CHECK(!contains(plain, "Hello PDFForge"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    const auto savedPlain = reopened->extractPlainText(0);
    CHECK(contains(savedPlain, "SECOND REPLACE"));
    CHECK(!contains(savedPlain, "FIRST REPLACE"));
    CHECK(!contains(savedPlain, "Hello PDFForge"));
}

TEST(EditReplacesEveryRunInAGroupedSpan) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-split-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-split-dst");
    const auto src = copyFixture("TEST_10_SPLIT_TEXT_RUNS.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "Cuantia")) {
            target = s;
            break;
        }
    }
    CHECK(contains(target.text, "Cuantia"));
    doc->replaceSpanText(target, "3 Contrato de mandato");
    const auto plain = doc->extractPlainText(0);
    CHECK(contains(plain, "3 Contrato de mandato"));
    CHECK(!contains(plain, "Cuantia"));
    CHECK(!contains(plain, "contrato"));
    CHECK(contains(plain, "Keep this sibling line"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    const auto savedPlain = reopened->extractPlainText(0);
    CHECK(contains(savedPlain, "3 Contrato de mandato"));
    CHECK(!contains(savedPlain, "Cuantia"));
    CHECK(contains(savedPlain, "Keep this sibling line"));
    std::ifstream in(dst, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!contains(saved, "Cuantia"));
}

TEST(EditReplacesTextInsideFormXObject) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-form-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-form-dst");
    const auto src = copyFixture("TEST_11_FORM_XOBJECT_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "FormText")) {
            target = s;
            break;
        }
    }
    CHECK(contains(target.text, "FormText"));
    doc->replaceSpanText(target, "Edited FormText");
    CHECK(contains(doc->extractPlainText(0), "Edited FormText"));
    CHECK(!contains(doc->extractPlainText(0), "Hello FormText"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "Edited FormText"));
    CHECK(!contains(reopened->extractPlainText(0), "Hello FormText"));
    std::ifstream in(dst, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!contains(saved, "Hello FormText"));
}

TEST(AddImageStampSurvivesSave) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-stamp-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-stamp-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const auto before = doc->extractImages(0).size();
    pdfforge::Bitmap stamp;
    stamp.width = 16;
    stamp.height = 8;
    stamp.stride = 16 * 4;
    stamp.bgra.assign(static_cast<std::size_t>(stamp.stride * stamp.height), 0);
    for (int y = 0; y < stamp.height; ++y) {
        for (int x = 0; x < stamp.width; ++x) {
            auto* px = stamp.bgra.data() +
                       static_cast<std::size_t>(y) * static_cast<std::size_t>(stamp.stride) +
                       static_cast<std::size_t>(x) * 4u;
            px[0] = 40;
            px[1] = 40;
            px[2] = 200;
            px[3] = 255;
        }
    }
    doc->addImage(0, pdfforge::RectF{72.0f, 80.0f, 96.0f, 36.0f}, stamp);
    auto images = doc->extractImages(0);
    CHECK(images.size() == before + 1);
    pdfforge::ImageObject found{};
    bool hasSign = false;
    for (const auto& img : images) {
        if (img.isSignature) {
            found = img;
            hasSign = true;
        }
    }
    CHECK(hasSign);
    CHECK(found.bounds.width > 90.0f);
    doc->setImageRect(found, pdfforge::RectF{70.0f, 78.0f, 140.0f, 52.5f});
    images = doc->extractImages(0);
    hasSign = false;
    for (const auto& img : images) {
        if (img.isSignature) {
            found = img;
            hasSign = true;
        }
    }
    CHECK(hasSign);
    CHECK(found.bounds.width > 130.0f);
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    images = reopened->extractImages(0);
    CHECK(images.size() == before + 1);
    hasSign = false;
    for (const auto& img : images) {
        if (img.isSignature) {
            found = img;
            hasSign = true;
        }
    }
    CHECK(hasSign);
    reopened->deleteImage(found);
    CHECK(reopened->extractImages(0).size() == before);
    pdfforge::SecureTempFile goneTmp("pdfforge-stamp-gone");
    const auto gone = goneTmp.path().string() + ".pdf";
    reopened->save(gone);
    auto afterDelete = pdfforge::PdfDocument::open(runtime, gone);
    CHECK(afterDelete->extractImages(0).size() == before);
}

TEST(RecognizeRegionFindsTextAndFont) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    auto doc = pdfforge::PdfDocument::open(runtime, fixture("TEST_01_SIMPLE_TEXT.pdf"));
    const auto spans = doc->extractText(0);
    CHECK(!spans.empty());
    pdfforge::TextSpan target = spans.front();
    for (const auto& s : spans) {
        if (contains(s.text, "PDFForge") || contains(s.text, "TOTAL")) {
            target = s;
            break;
        }
    }
    const pdfforge::RectF box{target.x - 4.0f, target.y - 4.0f, target.width + 8.0f,
                              target.height + 8.0f};
    const auto read = pdfforge::recognizeRegion(*doc, 0, box);
    CHECK(!read.text.empty());
    CHECK(contains(read.text, target.text.substr(0, std::min<std::size_t>(4, target.text.size()))) ||
          contains(read.text, "PDFForge") || contains(read.text, "TOTAL"));
    CHECK(read.fontSize > 0);
    CHECK(!read.spans.empty());
    CHECK(!read.usedOcr);
}

TEST(ReplaceRegionEditsMarqueeSpans) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-region-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-region-dst");
    const auto src = copyFixture("TEST_10_SPLIT_TEXT_RUNS.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    pdfforge::TextSpan target;
    for (const auto& s : doc->extractText(0)) {
        if (contains(s.text, "Cuantia")) {
            target = s;
            break;
        }
    }
    CHECK(contains(target.text, "Cuantia"));
    const pdfforge::RectF box{target.x - 2.0f, target.y - 2.0f, target.width + 4.0f,
                              target.height + 4.0f};
    auto read = pdfforge::recognizeRegion(*doc, 0, box);
    CHECK(!read.spans.empty());
    doc->replaceRegion(read.spans, "3 Contrato de mandato", target.fontSize, target.color);
    CHECK(contains(doc->extractPlainText(0), "3 Contrato de mandato"));
    CHECK(!contains(doc->extractPlainText(0), "Cuantia"));
    CHECK(contains(doc->extractPlainText(0), "Keep this sibling line"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "3 Contrato de mandato"));
    CHECK(contains(reopened->extractPlainText(0), "Keep this sibling line"));
}
