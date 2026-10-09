#include "worker/receiver.hpp"

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "utility/toInt.hpp"
#include "utility/unwrap.hpp"

#include <boost/crc.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include <unistd.h>

using namespace error;

namespace receiver
{

Receiver::Receiver(
        io::Port&                                                            port,
        std::map<frame::Type, std::unique_ptr<xqueue::Queue<frame::Frame>>>& frameStreams)
    : port(port), frameStreams(frameStreams)
{}

void Receiver::run(std::stop_token st)
{
        std::expected<frame::FrameHeader, Error> frh;
        frame::Frame                             fr;

        while (1) {
                // drain bytes until SOF is found
                // return if thread aborted
                if (!unwrap(this->findSOF(st, this->port)))
                        return;

                // read frame header
                frh = this->getFrameHeader(this->port);
                if (!frh.has_value()) {
                        if (frh.error().code == error::ErrorCode::INVALID_CRC)
                                continue;
                        else
                                abortByError(frh.error());
                }

                // read payload
                fr = unwrap(this->getPayload(this->port, frh.value()));

                // push frame to queue
                auto type = frame::frameQueueMUX(fr.type);
                if (type == frame::Type::UNKNOWN)
                        continue;

                this->frameStreams.at(type)->push(std::move(fr));
        }

        return;
}

/**
 * @return false if the thread is stopped, true if the SOF is found
 */
std::expected<bool, Error> Receiver::findSOF(std::stop_token& st, io::Port& port)
{
        std::array<uint8_t, 1> byte       = {};
        bool                   firstFound = false;

        while (1) {
                if (st.stop_requested())
                        return false;

                if (auto result = port.readRaw(byte); !result)
                        return std::unexpected<Error>(result.error());

                if (firstFound && byte[0] == frame::START_OF_FRAME[1])
                        break;

                firstFound = byte[0] == frame::START_OF_FRAME[0];
        }

        return true;
};

std::expected<frame::FrameHeader, Error> Receiver::getFrameHeader(io::Port& port)
{
        std::array<uint8_t, frame::RAW_HEADER_SIZE> rawHeader = {};

        auto result = port.readRaw(rawHeader);
        if (!result)
                return std::unexpected<Error>(result.error());

        if (!Receiver::isValidCRC(rawHeader))
                return std::unexpected(error::makeError(error::ErrorCode::INVALID_CRC));

        // TODO: low priority fix hard code
        frame::FrameHeader frh = {
                .length    = toInt16(rawHeader.data()),
                .type      = static_cast<frame::Type>(rawHeader[frame::TYPE_OFFSET]),
                .timestamp = toInt32(rawHeader.data() + frame::TIMESTAMP_OFFSET),
        };

        return frh;
}

std::expected<frame::Frame, Error>
Receiver::getPayload(io::Port& port, const frame::FrameHeader& frh)
{
        frame::Frame fr;
        fr.length  = frh.length;
        fr.type    = frh.type;
        fr.payload = std::vector<uint8_t>(fr.length);

        auto result = port.readRaw(std::span<uint8_t>(fr.payload));
        if (!result.has_value())
                return std::unexpected<Error>(result.error());

        return fr;
}

bool Receiver::isValidCRC(std::span<const uint8_t, frame::RAW_HEADER_SIZE> rawHeader)
{
        const frame::Crc expected = rawHeader[frame::CRC_OFFSET];

        boost::crc_optimal<8, 0x31, 0, 0, false, false> crc;

        crc.process_bytes(rawHeader.data(), frame::CRC_OFFSET);

        return crc.checksum() == expected;
}

} // namespace receiver
