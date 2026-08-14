#include "comparison/ImageMetrics.h"
#include "core/SecureTemp.h"
#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "pdf/RegionRecognize.h"
#include "tests/TestHarness.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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

int countDarkPixels(const pdfforge::Bitmap& bmp, int x0, int y0, int x1, int y1, int lumaMax) {
    if (bmp.empty()) {
        return 0;
    }
    x0 = std::clamp(x0, 0, bmp.width);
    y0 = std::clamp(y0, 0, bmp.height);
    x1 = std::clamp(x1, 0, bmp.width);
    y1 = std::clamp(y1, 0, bmp.height);
    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const auto* px = bmp.pixel(x, y);
            const int lum = (static_cast<int>(px[2]) * 3 + static_cast<int>(px[1]) * 6 +
                             static_cast<int>(px[0])) /
                            10;
            if (lum <= lumaMax) {
                ++n;
            }
        }
    }
    return n;
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
    CHECK(!read.marquee.empty());
    doc->replaceRegion(0, read.marquee, read.spans, "3 Contrato de mandato", target.fontSize,
                       target.color);
    CHECK(contains(doc->extractPlainText(0), "3 Contrato de mandato"));
    CHECK(!contains(doc->extractPlainText(0), "Cuantia"));
    CHECK(contains(doc->extractPlainText(0), "Keep this sibling line"));
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(0), "3 Contrato de mandato"));
    CHECK(contains(reopened->extractPlainText(0), "Keep this sibling line"));
}

TEST(ReplaceRegionErasesOverlappingTextAndImageBehind) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-erase-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-erase-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const int page = doc->pageCount();
    doc->insertBlankPage(page, pdfforge::SizeF{612.0f, 792.0f});

    pdfforge::Bitmap baked;
    baked.width = 80;
    baked.height = 14;
    baked.stride = baked.width * 4;
    baked.bgra.assign(static_cast<std::size_t>(baked.stride * baked.height), 0);
    for (int y = 0; y < baked.height; ++y) {
        for (int x = 0; x < baked.width; ++x) {
            auto* px = baked.bgra.data() +
                       static_cast<std::size_t>(y) * static_cast<std::size_t>(baked.stride) +
                       static_cast<std::size_t>(x) * 4u;
            px[0] = 36;
            px[1] = 36;
            px[2] = 36;
            px[3] = 255;
        }
    }
    const pdfforge::RectF imageBox{72.0f, 400.0f, 120.0f, 18.0f};
    const pdfforge::PointF textAt{72.0f, 404.0f};
    doc->addImage(page, imageBox, baked);
    doc->addText(page, textAt, "BEHINDSECRET", 16.0f, pdfforge::Color::rgb(0, 0, 0));
    doc->addText(page, textAt, "BEHINDSECRET", 16.0f, pdfforge::Color::rgb(0, 0, 0));
    CHECK(contains(doc->extractPlainText(page), "BEHINDSECRET"));

    const pdfforge::RectF marquee{60.0f, 390.0f, 200.0f, 40.0f};
    auto read = pdfforge::recognizeRegion(*doc, page, marquee);
    CHECK(contains(read.text, "BEHIND"));
    doc->replaceRegion(page, read.marquee, read.spans, "NEWSECRET", 16.0f,
                       pdfforge::Color::rgb(0, 0, 0));

    const std::string plain = doc->extractPlainText(page);
    CHECK(contains(plain, "NEWSECRET"));
    CHECK(!contains(plain, "BEHINDSECRET"));

    pdfforge::RenderRequest req;
    req.pageIndex = page;
    req.dpi = 72.0f;
    const auto bmp = doc->render(req);
    pdfforge::PointF topLeft;
    pdfforge::PointF bottomRight;
    CHECK(doc->pageToDevice(page, req, imageBox.x, imageBox.y + imageBox.height, topLeft));
    CHECK(doc->pageToDevice(page, req, imageBox.x + imageBox.width, imageBox.y, bottomRight));
    const int x0 = static_cast<int>(std::min(topLeft.x, bottomRight.x));
    const int y0 = static_cast<int>(std::min(topLeft.y, bottomRight.y));
    const int x1 = static_cast<int>(std::max(topLeft.x, bottomRight.x));
    const int y1 = static_cast<int>(std::max(topLeft.y, bottomRight.y));
    const int dark = countDarkPixels(bmp, x0, y0, x1, y1, 50);
    const int area = std::max(1, (x1 - x0) * (y1 - y0));
    CHECK(dark < area / 2);

    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    const std::string savedPlain = reopened->extractPlainText(page);
    CHECK(contains(savedPlain, "NEWSECRET"));
    CHECK(!contains(savedPlain, "BEHINDSECRET"));
    std::ifstream in(dst, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!contains(saved, "BEHINDSECRET"));
}

TEST(ReplaceRegionMovesTextToDestination) {
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    pdfforge::SecureTempFile srcTmp("pdfforge-move-src");
    pdfforge::SecureTempFile dstTmp("pdfforge-move-dst");
    const auto src = copyFixture("TEST_01_SIMPLE_TEXT.pdf", srcTmp);
    const auto dst = dstTmp.path().string() + ".pdf";
    auto doc = pdfforge::PdfDocument::open(runtime, src);
    const int page = doc->pageCount();
    doc->insertBlankPage(page, pdfforge::SizeF{612.0f, 792.0f});
    const pdfforge::PointF original{80.0f, 420.0f};
    doc->addText(page, original, "MOVEORIGIN", 16.0f, pdfforge::Color::rgb(0, 0, 0));
    const pdfforge::RectF marquee{60.0f, 400.0f, 180.0f, 40.0f};
    auto read = pdfforge::recognizeRegion(*doc, page, marquee);
    CHECK(contains(read.text, "MOVEORIGIN"));
    const pdfforge::PointF dest{240.0f, 520.0f};
    doc->replaceRegion(page, read.marquee, read.spans, "MOVEDTEXT", 16.0f,
                       pdfforge::Color::rgb(0, 0, 0), dest);
    const std::string plain = doc->extractPlainText(page);
    CHECK(contains(plain, "MOVEDTEXT"));
    CHECK(!contains(plain, "MOVEORIGIN"));
    bool placed = false;
    for (const auto& span : doc->extractText(page)) {
        if (!contains(span.text, "MOVEDTEXT")) {
            continue;
        }
        CHECK(std::fabs(span.x - dest.x) < 12.0f);
        CHECK(std::fabs(span.baseline - dest.y) < 16.0f || std::fabs(span.y - dest.y) < 16.0f);
        placed = true;
    }
    CHECK(placed);
    doc->save(dst);
    auto reopened = pdfforge::PdfDocument::open(runtime, dst);
    CHECK(contains(reopened->extractPlainText(page), "MOVEDTEXT"));
    CHECK(!contains(reopened->extractPlainText(page), "MOVEORIGIN"));
}
