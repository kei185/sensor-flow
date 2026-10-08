
#pragma once

#include <string_view>

namespace error
{

enum class Error
{
        OPEN_FILE_FAILED,
        CLOSE_FILE_FAILED,
        RECEIVER_INIT_FAILED,
        PARSER_INIT_FAILED,
        DISTRIBUTOR_INIT_FAILED,
        RECEIVER_DISPATCH_FAILED,
        PARSER_DISPATCH_FAILED,
        DISTRIBUTOR_DISPATCH_FAILED,
        FILE_INTERNAL_ERROR,
        IO_READ_FAILED,
        IO_WRITE_FAILED,
        THREAD_ABORTED,
        OPERATION_TIMEOUT,
        INVALID_CRC,
};

constexpr std::string_view toString(Error error)
{
        switch (error) {
                case Error::OPEN_FILE_FAILED:
                        return "OPEN_FILE_FAILED";
                case Error::CLOSE_FILE_FAILED:
                        return "CLOSE_FILE_FAILED";
                case Error::RECEIVER_INIT_FAILED:
                        return "RECEIVER_INIT_FAILED";
                case Error::PARSER_INIT_FAILED:
                        return "PARSER_INIT_FAILED";
                case Error::DISTRIBUTOR_INIT_FAILED:
                        return "DISTRIBUTOR_INIT_FAILED";
                case Error::RECEIVER_DISPATCH_FAILED:
                        return "RECEIVER_DISPATCH_FAILED";
                case Error::PARSER_DISPATCH_FAILED:
                        return "PARSER_DISPATCH_FAILED";
                case Error::DISTRIBUTOR_DISPATCH_FAILED:
                        return "DISTRIBUTOR_DISPATCH_FAILED";
                case Error::FILE_INTERNAL_ERROR:
                        return "FILE_INTERNAL_ERROR";
                case Error::IO_READ_FAILED:
                        return "IO_READ_FAILED";
                case Error::IO_WRITE_FAILED:
                        return "IO_WRITE_FAILED";
                case Error::THREAD_ABORTED:
                        return "THREAD_ABORTED";
                case Error::OPERATION_TIMEOUT:
                        return "OPERATION_TIMEOUT";
                case Error::INVALID_CRC:
                        return "INVALID_CRC";

                default:
                        return "UNKNOWN";
        }
}

} // namespace error
