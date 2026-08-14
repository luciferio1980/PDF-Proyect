#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace pdfforge {

enum class Status {
    Ok = 0,
    InvalidArgument,
    FileNotFound,
    FileTooLarge,
    TooManyPages,
    InvalidPdf,
    PasswordRequired,
    PasswordIncorrect,
    EncryptedUnsupported,
    PageOutOfRange,
    RenderFailed,
    OcrUnavailable,
    OcrFailed,
    IoError,
    Cancelled,
    InternalError,
    EditFailed,
};

inline const char* statusToString(Status status) {
    switch (status) {
        case Status::Ok:
            return "ok";
        case Status::InvalidArgument:
            return "invalid argument";
        case Status::FileNotFound:
            return "file not found";
        case Status::FileTooLarge:
            return "file exceeds the configured size limit";
        case Status::TooManyPages:
            return "document exceeds the configured page limit";
        case Status::InvalidPdf:
            return "the file is not a valid PDF document";
        case Status::PasswordRequired:
            return "this document is password-protected";
        case Status::PasswordIncorrect:
            return "the password is incorrect";
        case Status::EncryptedUnsupported:
            return "this encryption mode is not supported";
        case Status::PageOutOfRange:
            return "page index is out of range";
        case Status::RenderFailed:
            return "the page could not be rendered";
        case Status::OcrUnavailable:
            return "OCR is not available in this build";
        case Status::OcrFailed:
            return "OCR failed";
        case Status::IoError:
            return "file I/O failed";
        case Status::Cancelled:
            return "operation cancelled";
        case Status::InternalError:
            return "internal error";
        case Status::EditFailed:
            return "this PDF object could not be edited";
    }
    return "unknown error";
}

class Error : public std::runtime_error {
public:
    Error(Status status, std::string technical)
        : std::runtime_error(statusToString(status)),
          status_(status),
          technical_(std::move(technical)) {}

    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] const std::string& technical() const noexcept { return technical_; }
    [[nodiscard]] std::string userMessage() const { return statusToString(status_); }

private:
    Status status_;
    std::string technical_;
};

}  // namespace pdfforge
