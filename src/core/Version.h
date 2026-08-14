#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pdfforge {

inline constexpr std::string_view kProductName = "PDFForge";
inline constexpr std::string_view kProductTagline =
    "Independent professional PDF editor";
inline constexpr std::string_view kVersionString = PDFFORGE_VERSION_STRING;
inline constexpr int kVersionMajor = PDFFORGE_VERSION_MAJOR;
inline constexpr int kVersionMinor = PDFFORGE_VERSION_MINOR;
inline constexpr int kVersionPatch = PDFFORGE_VERSION_PATCH;

}  // namespace pdfforge
