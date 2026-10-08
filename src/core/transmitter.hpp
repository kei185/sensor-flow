#pragma once

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"

#include <expected>
#include <stop_token>

namespace transmitter
{

struct Transmitter
{
        io::Port& port;

      public:
        Transmitter(io::Port&);

        std::expected<void, error::Error> transmit(frame::OperationType, std::span<uint8_t> = {});

        std::expected<void, error::Error>
        session(std::stop_token,
                xqueue::Queue<frame::systemMessage>&,
                frame::OperationType,
                std::span<uint8_t> = {});
};

} // namespace transmitter
