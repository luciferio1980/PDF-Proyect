#include "pdf/QpdfBridge.h"

#include "core/Error.h"
#include "core/Logger.h"
#include "core/Utf.h"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>

namespace pdfforge {

void qpdfWriteCopy(const std::filesystem::path& source, const std::filesystem::path& destination) {
    try {
        QPDF qpdf;
        qpdf.processFile(narrowPath(source).c_str());
        QPDFWriter writer(qpdf, narrowPath(destination).c_str());
        writer.setPreserveEncryption(true);
        writer.setStaticID(false);
        writer.write();
        Logger::instance().info("qpdf", "wrote lossless copy");
    } catch (const std::exception& ex) {
        Logger::instance().error("qpdf", "write copy failed");
        throw Error(Status::IoError, std::string("QPDF: ") + ex.what());
    }
}

}  // namespace pdfforge
