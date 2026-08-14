#pragma once

#include "core/Geometry.h"

#include <string>
#include <vector>

namespace pdfforge {

struct TextSpan {
    std::string text;
    std::string fontName;
    float fontSize = 0;
    int fontWeight = 400;
    bool italic = false;
    bool serif = false;
    Color color;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float baseline = 0;
    float rotation = 0;  // degrees, PDF user space
    float charSpacing = 0;
    int pageIndex = 0;
    int pdfCharStart = -1;
    int pdfCharEnd = -1;

    [[nodiscard]] RectF bounds() const { return RectF{x, y, width, height}; }
};

struct TextObject {
    std::vector<TextSpan> spans;
    RectF bounds;
    int pageIndex = 0;
};

struct ImageObject {
    RectF bounds;
    int pageIndex = 0;
    int pdfObjectIndex = -1;
    float widthPx = 0;
    float heightPx = 0;
};

struct PathObject {
    RectF bounds;
    int pageIndex = 0;
};

struct Annotation {
    std::string subtype;
    std::string contents;
    RectF bounds;
    int pageIndex = 0;
};

struct FormField {
    std::string name;
    std::string value;
    std::string type;
    RectF bounds;
    int pageIndex = 0;
};

struct Signature {
    std::string name;
    RectF bounds;
    int pageIndex = 0;
    bool visualOnly = true;
};

struct Metadata {
    std::string title;
    std::string author;
    std::string subject;
    std::string keywords;
    std::string creator;
    std::string producer;
    std::string creationDate;
    std::string modificationDate;
};

}  // namespace pdfforge
