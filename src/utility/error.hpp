
#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace error
{

enum class ErrorCode
{
        OPEN_FILE_FAILED,
        CLOSE_FILE_FAILED,
        RECEIVER_INIT_FAILED,
        PARSER_INIT_FAILED,
        DISTRIBUTOR_INIT_FAILED,
        WORKER_DISPATCH_FAILED,
        FILE_INTERNAL_ERROR,
        IO_READ_FAILED,
        IO_WRITE_FAILED,
        THREAD_ABORTED,
        OPERATION_TIMEOUT,
        INVALID_CRC,
};

struct Error
{
        ErrorCode            code;
        std::source_location location;
};

Error makeError(
        ErrorCode,
        std::source_location location = std::source_location::current()) noexcept;

constexpr std::string_view toString(ErrorCode code)
{
        switch (code) {
                case ErrorCode::OPEN_FILE_FAILED:
                        return "OPEN_FILE_FAILED";
                case ErrorCode::CLOSE_FILE_FAILED:
                        return "CLOSE_FILE_FAILED";
                case ErrorCode::RECEIVER_INIT_FAILED:
                        return "RECEIVER_INIT_FAILED";
                case ErrorCode::PARSER_INIT_FAILED:
                        return "PARSER_INIT_FAILED";
                case ErrorCode::DISTRIBUTOR_INIT_FAILED:
                        return "DISTRIBUTOR_INIT_FAILED";
                case ErrorCode::WORKER_DISPATCH_FAILED:
                        return "WORKER_DISPATCH_FAILED";
                case ErrorCode::FILE_INTERNAL_ERROR:
                        return "FILE_INTERNAL_ERROR";
                case ErrorCode::IO_READ_FAILED:
                        return "IO_READ_FAILED";
                case ErrorCode::IO_WRITE_FAILED:
                        return "IO_WRITE_FAILED";
                case ErrorCode::THREAD_ABORTED:
                        return "THREAD_ABORTED";
                case ErrorCode::OPERATION_TIMEOUT:
                        return "OPERATION_TIMEOUT";
                case ErrorCode::INVALID_CRC:
                        return "INVALID_CRC";

                default:
                        return "UNKNOWN";
        }
}

std::string toString(const Error&);

} // namespace error
