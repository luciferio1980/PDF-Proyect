#include "core/Error.h"
#include "core/Logger.h"
#include "core/Version.h"
#include "pdf/PdfiumRuntime.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
std::filesystem::path executableDirectory() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buf).parent_path();
}

void showFatal(const std::string& message) {
    MessageBoxA(nullptr, message.c_str(), "PDFForge", MB_OK | MB_ICONERROR);
}
#else
std::filesystem::path executableDirectory() {
    return std::filesystem::current_path();
}

void showFatal(const std::string& message) {
    std::fprintf(stderr, "PDFForge: %s\n", message.c_str());
}
#endif

void writeStartupLog(const std::filesystem::path& dir, const std::string& line) {
    std::ofstream out(dir / "startup.log", std::ios::app);
    if (out) {
        out << line << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto exeDir = executableDirectory();
    writeStartupLog(exeDir, "PDFForge starting");

#ifdef _WIN32
    SetDllDirectoryW(exeDir.wstring().c_str());
    const auto pluginDir = (exeDir / "platforms").wstring();
    SetEnvironmentVariableW(L"QT_QPA_PLATFORM_PLUGIN_PATH", pluginDir.c_str());
    SetEnvironmentVariableW(L"QT_PLUGIN_PATH", exeDir.wstring().c_str());
#endif

#ifdef _WIN32
    const QString exeDirQt = QString::fromStdWString(exeDir.wstring());
#else
    const QString exeDirQt = QString::fromStdString(exeDir.string());
#endif

    try {
        QApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("PDFForge"));
        app.setApplicationDisplayName(QStringLiteral("PDFForge"));
        app.setOrganizationName(QStringLiteral("PDFForge"));
        app.setApplicationVersion(QString::fromUtf8(pdfforge::kVersionString.data(),
                                                    static_cast<int>(pdfforge::kVersionString.size())));
        QCoreApplication::addLibraryPath(exeDirQt);

        auto logDir = std::filesystem::path(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) /
                      "logs";
        std::error_code ec;
        std::filesystem::create_directories(logDir, ec);
        if (!ec) {
            pdfforge::Logger::instance().addSink(
                std::make_shared<pdfforge::FileLogSink>((logDir / "pdfforge.log").string()));
        }
        pdfforge::Logger::instance().addSink(
            std::make_shared<pdfforge::FileLogSink>((exeDir / "pdfforge.log").string()));
        pdfforge::Logger::instance().info("app", "PDFForge starting");

        pdfforge::ui::applyApplicationTheme(app);
        auto runtime = pdfforge::PdfiumRuntime::acquire();
        (void)runtime;

        pdfforge::ui::MainWindow window;
        window.show();
        if (argc > 1) {
            window.openPath(QString::fromLocal8Bit(argv[1]));
        } else {
            const QString sample =
                QDir(exeDirQt).filePath(QStringLiteral("test_documents/TEST_01_SIMPLE_TEXT.pdf"));
            if (QFileInfo::exists(sample)) {
                window.openPath(sample);
            }
        }
        writeStartupLog(exeDir, "event loop");
        return app.exec();
    } catch (const pdfforge::Error& ex) {
        writeStartupLog(exeDir, std::string("error: ") + ex.what());
        showFatal(ex.userMessage());
        return 1;
    } catch (const std::exception& ex) {
        writeStartupLog(exeDir, std::string("exception: ") + ex.what());
        showFatal(std::string("PDFForge no pudo arrancar:\n") + ex.what());
        return 1;
    } catch (...) {
        writeStartupLog(exeDir, "unknown crash");
        showFatal("PDFForge no pudo arrancar por un error interno.");
        return 1;
    }
}
