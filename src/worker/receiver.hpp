#pragma once

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "worker/processor.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <map>
#include <memory>
#include <span>
#include <stop_token>

#include <unistd.h>

using namespace error;

namespace receiver
{

struct Receiver : processor::Processor
{
        io::Port&                                                            port;
        std::map<frame::Type, std::unique_ptr<xqueue::Queue<frame::Frame>>>& frameStreams;

        Receiver(io::Port&, std::map<frame::Type, std::unique_ptr<xqueue::Queue<frame::Frame>>>&);

        void run(std::stop_token) override;

        static std::expected<bool, Error>               findSOF(std::stop_token&, io::Port&);
        static std::expected<frame::FrameHeader, Error> getFrameHeader(io::Port&);
        static std::expected<frame::Frame, Error> getPayload(io::Port&, const frame::FrameHeader&);
        static bool isValidCRC(std::span<const uint8_t, frame::RAW_HEADER_SIZE>);
};
} // namespace receiver
