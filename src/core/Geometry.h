#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace pdfforge {

struct PointF {
    float x = 0;
    float y = 0;
};

struct SizeF {
    float width = 0;
    float height = 0;
};

struct RectF {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    [[nodiscard]] float left() const { return x; }
    [[nodiscard]] float right() const { return x + width; }
    [[nodiscard]] float bottom() const { return y; }
    [[nodiscard]] float top() const { return y + height; }
    [[nodiscard]] float area() const { return std::max(0.0f, width) * std::max(0.0f, height); }
    [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }

    [[nodiscard]] bool contains(float px, float py) const {
        return px >= x && py >= y && px <= x + width && py <= y + height;
    }

    [[nodiscard]] bool intersects(const RectF& other) const {
        return left() < other.right() && other.left() < right() && bottom() < other.top() &&
               other.bottom() < top();
    }

    [[nodiscard]] RectF intersection(const RectF& other) const {
        const float l = std::max(left(), other.left());
        const float b = std::max(bottom(), other.bottom());
        const float r = std::min(right(), other.right());
        const float t = std::min(top(), other.top());
        if (r <= l || t <= b) {
            return {};
        }
        return RectF{l, b, r - l, t - b};
    }

    [[nodiscard]] RectF united(const RectF& other) const {
        if (empty()) {
            return other;
        }
        if (other.empty()) {
            return *this;
        }
        const float l = std::min(left(), other.left());
        const float b = std::min(bottom(), other.bottom());
        const float r = std::max(right(), other.right());
        const float t = std::max(top(), other.top());
        return RectF{l, b, r - l, t - b};
    }
};

struct Color {
    enum class Space { Rgb, Gray, Cmyk };

    Space space = Space::Rgb;
    float c0 = 0;
    float c1 = 0;
    float c2 = 0;
    float c3 = 0;
    float alpha = 1;

    static Color rgb(float r, float g, float b, float a = 1.0f) {
        return Color{Space::Rgb, r, g, b, 0, a};
    }

    static Color gray(float y, float a = 1.0f) {
        return Color{Space::Gray, y, 0, 0, 0, a};
    }

    static Color fromBytes(unsigned r, unsigned g, unsigned b, unsigned a = 255) {
        return rgb(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                   static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f);
    }

    [[nodiscard]] Color toRgb() const {
        switch (space) {
            case Space::Gray:
                return rgb(c0, c0, c0, alpha);
            case Space::Cmyk: {
                const float k = c3;
                const float r = (1.0f - c0) * (1.0f - k);
                const float g = (1.0f - c1) * (1.0f - k);
                const float b = (1.0f - c2) * (1.0f - k);
                return rgb(r, g, b, alpha);
            }
            case Space::Rgb:
            default:
                return *this;
        }
    }
};

inline float radiansToDegrees(float radians) {
    constexpr float kPi = 3.14159265358979323846f;
    return radians * 180.0f / kPi;
}

inline std::string formatRect(const RectF& r) {
    return "(" + std::to_string(r.x) + "," + std::to_string(r.y) + " " +
           std::to_string(r.width) + "x" + std::to_string(r.height) + ")";
}

}  // namespace pdfforge
