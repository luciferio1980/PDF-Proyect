# Downloads a pinned PDFium binary distribution (non-V8, no JavaScript engine).
# Official project: https://pdfium.googlesource.com/pdfium/
# Binary packaging: https://github.com/bblanchon/pdfium-binaries
#
# We pin Chromium 7999 so builds are reproducible. Bump the tag, hashes, and
# THIRD_PARTY_LICENSES.md together.

set(PDFFORGE_PDFIUM_TAG "chromium/7999")
set(PDFFORGE_PDFIUM_VERSION "153.0.7999.0")

if(WIN32)
    set(PDFFORGE_PDFIUM_ARCHIVE "pdfium-win-x64.tgz")
    set(PDFFORGE_PDFIUM_SHA256 "55329d5cb5de8a379a2fc563106492d7f385a1f795d18970922c71f708f9fbb4")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(PDFFORGE_PDFIUM_ARCHIVE "pdfium-mac-arm64.tgz")
        set(PDFFORGE_PDFIUM_SHA256 "")
    else()
        set(PDFFORGE_PDFIUM_ARCHIVE "pdfium-mac-x64.tgz")
        set(PDFFORGE_PDFIUM_SHA256 "")
    endif()
else()
    set(PDFFORGE_PDFIUM_ARCHIVE "pdfium-linux-x64.tgz")
    set(PDFFORGE_PDFIUM_SHA256 "c3af580f9df0fef9545b44115bc5ea440f286956b5f231df69fb373b8efc4f69")
endif()

set(PDFFORGE_PDFIUM_URL
    "https://github.com/bblanchon/pdfium-binaries/releases/download/${PDFFORGE_PDFIUM_TAG}/${PDFFORGE_PDFIUM_ARCHIVE}")

set(PDFFORGE_PDFIUM_ROOT "${CMAKE_BINARY_DIR}/third_party/pdfium")
set(PDFFORGE_PDFIUM_ARCHIVE_PATH "${CMAKE_BINARY_DIR}/third_party/cache/${PDFFORGE_PDFIUM_ARCHIVE}")

if(NOT EXISTS "${PDFFORGE_PDFIUM_ROOT}/include/fpdfview.h")
    message(STATUS "Downloading PDFium ${PDFFORGE_PDFIUM_VERSION} (${PDFFORGE_PDFIUM_ARCHIVE})")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/third_party/cache")
    if(PDFFORGE_PDFIUM_SHA256 STREQUAL "")
        file(DOWNLOAD "${PDFFORGE_PDFIUM_URL}" "${PDFFORGE_PDFIUM_ARCHIVE_PATH}"
            SHOW_PROGRESS
            STATUS _pdfium_dl_status)
    else()
        file(DOWNLOAD "${PDFFORGE_PDFIUM_URL}" "${PDFFORGE_PDFIUM_ARCHIVE_PATH}"
            SHOW_PROGRESS
            EXPECTED_HASH SHA256=${PDFFORGE_PDFIUM_SHA256}
            STATUS _pdfium_dl_status)
    endif()
    list(GET _pdfium_dl_status 0 _pdfium_dl_code)
    if(NOT _pdfium_dl_code EQUAL 0)
        list(GET _pdfium_dl_status 1 _pdfium_dl_msg)
        message(FATAL_ERROR "Failed to download PDFium: ${_pdfium_dl_msg}")
    endif()
    file(MAKE_DIRECTORY "${PDFFORGE_PDFIUM_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${PDFFORGE_PDFIUM_ARCHIVE_PATH}" DESTINATION "${PDFFORGE_PDFIUM_ROOT}")
endif()

set(PDFium_DIR "${PDFFORGE_PDFIUM_ROOT}")
list(PREPEND CMAKE_PREFIX_PATH "${PDFFORGE_PDFIUM_ROOT}")
find_package(PDFium REQUIRED)

if(UNIX AND NOT APPLE)
    set_property(TARGET pdfium PROPERTY INTERFACE_LINK_LIBRARIES ${CMAKE_DL_LIBS} pthread)
endif()
