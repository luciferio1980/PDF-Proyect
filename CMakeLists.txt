cmake_minimum_required(VERSION 3.22)

project(PDFForge
    VERSION 0.2.12
    DESCRIPTION "Independent professional PDF editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

option(PDFFORGE_BUILD_APP "Build the Qt desktop application" ON)
option(PDFFORGE_BUILD_TESTS "Build unit and integration tests" ON)
option(PDFFORGE_WITH_QPDF "Use QPDF for lossless copy/save" ON)
option(PDFFORGE_WITH_TESSERACT "Use Tesseract for local OCR" ON)

include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchPDFium.cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# pkg-config is typical on Linux; Windows portable builds do not require it.
find_package(PkgConfig QUIET)
if(NOT PkgConfig_FOUND)
    if(PDFFORGE_WITH_QPDF)
        message(STATUS "pkg-config not found; disabling QPDF")
        set(PDFFORGE_WITH_QPDF OFF)
    endif()
    if(PDFFORGE_WITH_TESSERACT)
        message(STATUS "pkg-config not found; disabling Tesseract")
        set(PDFFORGE_WITH_TESSERACT OFF)
    endif()
endif()

if(PDFFORGE_WITH_QPDF)
    pkg_check_modules(QPDF IMPORTED_TARGET libqpdf)
    if(NOT QPDF_FOUND)
        message(WARNING "QPDF not found; save-copy via QPDF will be disabled")
        set(PDFFORGE_WITH_QPDF OFF)
    endif()
endif()

if(PDFFORGE_WITH_TESSERACT)
    pkg_check_modules(TESSERACT IMPORTED_TARGET tesseract)
    if(NOT TESSERACT_FOUND)
        find_library(TESSERACT_LIB tesseract)
        find_path(TESSERACT_INCLUDE tesseract/baseapi.h)
        if(TESSERACT_LIB AND TESSERACT_INCLUDE)
            add_library(pdfforge_tesseract INTERFACE)
            target_link_libraries(pdfforge_tesseract INTERFACE ${TESSERACT_LIB})
            target_include_directories(pdfforge_tesseract INTERFACE ${TESSERACT_INCLUDE})
            set(TESSERACT_FOUND TRUE)
        else()
            message(WARNING "Tesseract not found; OCR will be disabled")
            set(PDFFORGE_WITH_TESSERACT OFF)
        endif()
    endif()
endif()

set(PDFFORGE_CORE_SOURCES
    src/core/Logger.cpp
    src/core/SecureTemp.cpp
    src/core/Utf.cpp
    src/pdf/PdfiumRuntime.cpp
    src/pdf/PdfDocument.cpp
    src/pdf/PdfMutate.cpp
    src/pdf/PdfTextExtractor.cpp
    src/pdf/PageClassifier.cpp
    src/pdf/PdfSearch.cpp
    src/pdf/RegionRecognize.cpp
    src/renderer/PdfRenderer.cpp
    src/renderer/RenderCache.cpp
    src/fonts/FontMatcher.cpp
    src/fonts/FontDetector.cpp
    src/comparison/ImageMetrics.cpp
    src/ocr/OcrEngine.cpp
)

add_library(pdfforge_core STATIC ${PDFFORGE_CORE_SOURCES})
add_library(pdfforge::core ALIAS pdfforge_core)
set_target_properties(pdfforge_core PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

target_include_directories(pdfforge_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pdfforge_core
    PUBLIC
        pdfium
)

target_compile_definitions(pdfforge_core
    PUBLIC
        PDFFORGE_VERSION_STRING="${PROJECT_VERSION}"
        PDFFORGE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PDFFORGE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PDFFORGE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(PDFFORGE_WITH_QPDF)
    target_sources(pdfforge_core PRIVATE src/pdf/QpdfBridge.cpp)
    target_link_libraries(pdfforge_core PUBLIC PkgConfig::QPDF)
    target_compile_definitions(pdfforge_core PUBLIC PDFFORGE_HAS_QPDF=1 POINTERHOLDER_TRANSITION=0)
else()
    target_compile_definitions(pdfforge_core PUBLIC PDFFORGE_HAS_QPDF=0)
endif()

if(PDFFORGE_WITH_TESSERACT)
    if(TARGET PkgConfig::TESSERACT)
        target_link_libraries(pdfforge_core PUBLIC PkgConfig::TESSERACT)
    elseif(TARGET pdfforge_tesseract)
        target_link_libraries(pdfforge_core PUBLIC pdfforge_tesseract)
    endif()
    target_compile_definitions(pdfforge_core PUBLIC PDFFORGE_HAS_TESSERACT=1)
else()
    target_compile_definitions(pdfforge_core PUBLIC PDFFORGE_HAS_TESSERACT=0)
endif()

if(MSVC)
    target_compile_options(pdfforge_core PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_definitions(pdfforge_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(pdfforge_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

set(PDFFORGE_UI_SOURCES
    src/ui/Theme.cpp
    src/ui/PdfCanvas.cpp
    src/ui/ThumbnailPane.cpp
    src/ui/SearchPanel.cpp
    src/ui/MainWindow.cpp
    src/ui/InspectorPanel.cpp
    src/ui/HomeView.cpp
    src/ui/SignaturePad.cpp
    src/ui/SignatureLibrary.cpp
)

if(PDFFORGE_BUILD_APP)
    set(CMAKE_AUTOMOC ON)
    add_executable(pdfforge
        src/app/main.cpp
        ${PDFFORGE_UI_SOURCES}
    )
    target_link_libraries(pdfforge
        PRIVATE
            pdfforge_core
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )
    set_target_properties(pdfforge PROPERTIES
        OUTPUT_NAME PDFForge
        AUTOMOC ON
    )
    if(WIN32)
        set_target_properties(pdfforge PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    if(MSVC)
        target_compile_options(pdfforge PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(pdfforge PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_custom_command(TARGET pdfforge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:pdfium>"
            "$<TARGET_FILE_DIR:pdfforge>"
        COMMENT "Copy PDFium next to PDFForge"
    )
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(PDFFORGE_TEST_PDF_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_documents")
file(MAKE_DIRECTORY "${PDFFORGE_TEST_PDF_DIR}")
add_custom_command(
    OUTPUT
        "${PDFFORGE_TEST_PDF_DIR}/TEST_01_SIMPLE_TEXT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_02_MULTIPLE_FONTS.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_03_BOLD_ITALIC.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_04_ROTATED_TEXT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_05_SCANNED_DOCUMENT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_06_TABLE.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_07_MULTICOLUMN.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_08_EMBEDDED_FONT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_09_MIXED_CONTENT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_10_SPLIT_TEXT_RUNS.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_11_FORM_XOBJECT_TEXT.pdf"
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/test_documents/generate_test_pdfs.py"
        "${PDFFORGE_TEST_PDF_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/test_documents/generate_test_pdfs.py"
    COMMENT "Generating PDFForge test documents"
)

add_custom_target(pdfforge_test_pdfs ALL
    DEPENDS
        "${PDFFORGE_TEST_PDF_DIR}/TEST_01_SIMPLE_TEXT.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_10_SPLIT_TEXT_RUNS.pdf"
        "${PDFFORGE_TEST_PDF_DIR}/TEST_11_FORM_XOBJECT_TEXT.pdf"
)

if(PDFFORGE_BUILD_APP)
    add_dependencies(pdfforge pdfforge_test_pdfs)
    add_custom_command(TARGET pdfforge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PDFFORGE_TEST_PDF_DIR}"
            "$<TARGET_FILE_DIR:pdfforge>/test_documents"
        COMMENT "Copy sample PDFs next to PDFForge"
    )
endif()

if(PDFFORGE_BUILD_TESTS)
    enable_testing()
    add_executable(pdfforge_tests
        tests/TestHarness.cpp
        tests/pdf/test_open.cpp
        tests/pdf/test_text_extract.cpp
        tests/pdf/test_search.cpp
        tests/pdf/test_classifier.cpp
        tests/pdf/test_save_copy.cpp
        tests/pdf/test_edit.cpp
        tests/rendering/test_render.cpp
        tests/fonts/test_font_matcher.cpp
        tests/ocr/test_ocr.cpp
        tests/comparison/test_image_metrics.cpp
        tests/core/test_logger.cpp
    )
    set_target_properties(pdfforge_tests PROPERTIES AUTOMOC OFF)
    target_link_libraries(pdfforge_tests PRIVATE pdfforge_core)
    target_include_directories(pdfforge_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(pdfforge_tests PRIVATE
        PDFFORGE_TEST_PDF_DIR="${PDFFORGE_TEST_PDF_DIR}"
    )
    add_dependencies(pdfforge_tests pdfforge_test_pdfs)
    add_custom_command(TARGET pdfforge_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:pdfium>"
            "$<TARGET_FILE_DIR:pdfforge_tests>"
    )
    if(MSVC)
        target_compile_options(pdfforge_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(pdfforge_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME pdfforge_tests COMMAND pdfforge_tests)
    set_tests_properties(pdfforge_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(PDFFORGE_BUILD_APP)
    install(TARGETS pdfforge RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
