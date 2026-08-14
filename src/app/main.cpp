#include "core/Logger.h"
#include "core/Version.h"
#include "pdf/PdfiumRuntime.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QStandardPaths>
#include <filesystem>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PDFForge"));
    app.setApplicationDisplayName(QStringLiteral("PDFForge"));
    app.setOrganizationName(QStringLiteral("PDFForge"));
    app.setApplicationVersion(QString::fromUtf8(pdfforge::kVersionString.data(),
                                                static_cast<int>(pdfforge::kVersionString.size())));

    auto logDir = std::filesystem::path(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) /
                  "logs";
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (!ec) {
        pdfforge::Logger::instance().addSink(
            std::make_shared<pdfforge::FileLogSink>((logDir / "pdfforge.log").string()));
    }
    pdfforge::Logger::instance().info("app", "PDFForge starting");

    pdfforge::ui::applyApplicationTheme(app);
    auto runtime = pdfforge::PdfiumRuntime::acquire();
    (void)runtime;

    pdfforge::ui::MainWindow window;
    window.show();
    if (argc > 1) {
        window.openPath(QString::fromLocal8Bit(argv[1]));
    }
    return app.exec();
}
