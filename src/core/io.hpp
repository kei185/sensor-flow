
#pragma once

#include "utility/error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <span>
#include <string>

#include <termios.h>

namespace io
{

struct Port
{
        int            fd;
        struct termios tty;

        std::mutex mutex;

        Port(std::string);
        ~Port();

        Port(const Port&)            = delete;
        Port& operator=(const Port&) = delete;

        std::expected<void, error::Error> readRaw(std::span<uint8_t>);
        std::expected<void, error::Error> writeRaw(std::span<const uint8_t>);
};

} // namespace io
