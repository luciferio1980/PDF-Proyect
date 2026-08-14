#include "ocr/OcrEngine.h"

#include "core/Error.h"
#include "core/Logger.h"
#include "renderer/Bitmap.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#if PDFFORGE_HAS_TESSERACT
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#endif

namespace pdfforge {
namespace {

std::string findTessdata() {
    const char* env = std::getenv("TESSDATA_PREFIX");
    if (env && *env) {
        return env;
    }
    const char* candidates[] = {
        "/usr/share/tesseract-ocr/5/tessdata",
        "/usr/share/tesseract-ocr/4.00/tessdata",
        "/usr/share/tessdata",
        "/usr/local/share/tessdata",
    };
    for (const char* c : candidates) {
        if (std::filesystem::exists(std::filesystem::path(c) / "eng.traineddata")) {
            return c;
        }
    }
    return {};
}

}  // namespace

bool OcrEngine::available() {
#if PDFFORGE_HAS_TESSERACT
    return true;
#else
    return false;
#endif
}

std::vector<std::string> OcrEngine::supportedLanguages() {
    return {"eng", "spa", "fra", "deu", "ita", "por"};
}

OcrPageResult OcrEngine::recognize(const Bitmap& bitmap, const OcrOptions& options) {
    if (!available()) {
        throw Error(Status::OcrUnavailable, "built without Tesseract");
    }
    if (bitmap.empty()) {
        throw Error(Status::InvalidArgument, "empty bitmap");
    }
#if PDFFORGE_HAS_TESSERACT
    if (options.cancel && options.cancel->isCancelled()) {
        throw Error(Status::Cancelled, "OCR cancelled before start");
    }
    tesseract::TessBaseAPI api;
    const std::string tessdata = findTessdata();
    const char* datapath = tessdata.empty() ? nullptr : tessdata.c_str();
    if (api.Init(datapath, options.language.c_str())) {
        throw Error(Status::OcrFailed, "TessBaseAPI::Init failed");
    }
    api.SetPageSegMode(tesseract::PSM_AUTO);
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(bitmap.width) *
                                  static_cast<std::size_t>(bitmap.height) * 3u);
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            const auto* px = bitmap.bgra.data() +
                             static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.stride) +
                             static_cast<std::size_t>(x) * 4u;
            auto* dst = rgb.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.width) +
                                      static_cast<std::size_t>(x)) *
                                         3u;
            dst[0] = px[2];
            dst[1] = px[1];
            dst[2] = px[0];
        }
    }
    api.SetImage(rgb.data(), bitmap.width, bitmap.height, 3, bitmap.width * 3);
    if (options.dpi > 0) {
        api.SetSourceResolution(options.dpi);
    }
    if (api.Recognize(nullptr) != 0) {
        api.End();
        throw Error(Status::OcrFailed, "TessBaseAPI::Recognize failed");
    }
    if (options.cancel && options.cancel->isCancelled()) {
        api.End();
        throw Error(Status::Cancelled, "OCR cancelled");
    }

    OcrPageResult result;
    result.language = options.language;
    if (char* text = api.GetUTF8Text()) {
        result.text = text;
        delete[] text;
    }
    result.meanConfidence = static_cast<float>(api.MeanTextConf());

    tesseract::ResultIterator* it = api.GetIterator();
    if (it) {
        do {
            if (it->Empty(tesseract::RIL_WORD)) {
                continue;
            }
            std::unique_ptr<char[]> wordText(it->GetUTF8Text(tesseract::RIL_WORD));
            if (!wordText) {
                continue;
            }
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            it->BoundingBox(tesseract::RIL_WORD, &x1, &y1, &x2, &y2);
            OcrWord word;
            word.text = wordText.get();
            word.confidence = it->Confidence(tesseract::RIL_WORD);
            word.x = static_cast<float>(x1);
            word.y = static_cast<float>(y1);
            word.width = static_cast<float>(x2 - x1);
            word.height = static_cast<float>(y2 - y1);
            result.words.push_back(std::move(word));
        } while (it->Next(tesseract::RIL_WORD));
        delete it;
    }
    api.End();
    Logger::instance().info("ocr", "recognized words=" + std::to_string(result.words.size()));
    return result;
#else
    (void)bitmap;
    (void)options;
    throw Error(Status::OcrUnavailable, "built without Tesseract");
#endif
}

}  // namespace pdfforge
