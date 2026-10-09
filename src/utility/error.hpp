
#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace error
{

enum class Code
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
        Code                 code;
        std::source_location location;
};

Error makeError(Code, std::source_location location = std::source_location::current()) noexcept;

constexpr std::string_view toString(Code code)
{
        switch (code) {
                case Code::OPEN_FILE_FAILED:
                        return "OPEN_FILE_FAILED";
                case Code::CLOSE_FILE_FAILED:
                        return "CLOSE_FILE_FAILED";
                case Code::RECEIVER_INIT_FAILED:
                        return "RECEIVER_INIT_FAILED";
                case Code::PARSER_INIT_FAILED:
                        return "PARSER_INIT_FAILED";
                case Code::DISTRIBUTOR_INIT_FAILED:
                        return "DISTRIBUTOR_INIT_FAILED";
                case Code::WORKER_DISPATCH_FAILED:
                        return "WORKER_DISPATCH_FAILED";
                case Code::FILE_INTERNAL_ERROR:
                        return "FILE_INTERNAL_ERROR";
                case Code::IO_READ_FAILED:
                        return "IO_READ_FAILED";
                case Code::IO_WRITE_FAILED:
                        return "IO_WRITE_FAILED";
                case Code::THREAD_ABORTED:
                        return "THREAD_ABORTED";
                case Code::OPERATION_TIMEOUT:
                        return "OPERATION_TIMEOUT";
                case Code::INVALID_CRC:
                        return "INVALID_CRC";

                default:
                        return "UNKNOWN";
        }
}

std::string toString(const Error&);

} // namespace error
