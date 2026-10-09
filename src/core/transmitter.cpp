#include "core/transmitter.hpp"

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/logger.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <ranges>
#include <span>
#include <stop_token>
#include <vector>

namespace transmitter
{

Transmitter::Transmitter(io::Port& port) : port(port) {}

/**
 * Transmits a frame of the given type of operation.
 */
std::expected<void, error::Error>
Transmitter::transmit(frame::OperationType type, std::span<uint8_t> data)
{
        logger::log(std::format("TRANSMIT {} TRY", frame::toString(type)));

        const auto command = frame::OPERATION.at(type);

        std::vector<uint8_t> frame(command.begin(), command.end());
        if (!data.empty())
                frame.insert(frame.end(), data.begin(), data.end());

        logger::log(std::format("frame: {}", frame | std::views::transform([](uint8_t x) {
                                                     return std::format("{:#x}", x);
                                             })));

        auto result = this->port.writeRaw(std::span<const uint8_t>(frame));
        if (!result)
                return result;

        logger::log(std::format("TRANSMIT {} DONE", frame::toString(type)));

        return {};
}

/**
 * Sends a request to the transmitter and waits for an ACK response.
 */
std::expected<void, error::Error> Transmitter::session(
        std::stop_token                      st,
        xqueue::Queue<frame::systemMessage>& mQueue,
        frame::OperationType                 type,
        std::span<uint8_t>                   data)
{
        // transmit
        if (auto result = this->transmit(type, data); !result)
                return result;

        // set timeout
        const auto deadline = std::chrono::steady_clock::now() + frame::OPERATION_TIMEOUT;

        while (1) {
                // check stop request
                if (st.stop_requested())
                        return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));

                // check buffer
                if (mQueue.empty()) {
                        // if timeout
                        if (std::chrono::steady_clock::now() >= deadline)
                                return std::unexpected(
                                        error::makeError(error::Code::OPERATION_TIMEOUT));

                        continue;
                }

                auto res = mQueue.pop();

                logger::log(std::format("RECEIVED {}", res.message));
                if (res.type == frame::toAckType(type))
                        return {};
        }
};

} // namespace transmitter
