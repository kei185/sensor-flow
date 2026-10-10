#include "core/protocol.hpp"

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "utility/logger.hpp"
#include "core/transmitter.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <iostream>
#include <print>
#include <stop_token>
#include <string>

namespace protocol
{

static std::expected<frame::systemMessage, error::Error>
waitFor(std::stop_token, xqueue::Queue<frame::systemMessage>&, frame::Type);

std::expected<frame::systemMessage, error::Error>
timeSync(std::stop_token, transmitter::Transmitter&, xqueue::Queue<frame::systemMessage>&);

void run(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        while (!st.stop_requested()) {
                logger::log("PROTOCOL STARTED");

                while (!mQueue.empty())
                        mQueue.pop();

                auto result = transmitter.session(st, mQueue, frame::OperationType::HANDSHAKE);
                if (!result) {
                        logger::log(result.error());
                        continue;
                }

                result = waitFor(st, mQueue, frame::Type::READY);
                if (!result) {
                        logger::log(result.error());
                        continue;
                }

                result = timeSync(st, transmitter, mQueue);
                if (!result) {
                        logger::log(result.error());
                        continue;
                }

                result = waitFor(st, mQueue, frame::Type::TIME_REPORT);
                if (!result) {
                        logger::log(result.error());
                        continue;
                }

                std::println("press ENTER to start scan");
                std::string s;
                std::getline(std::cin, s);
                auto ack = transmitter.session(st, mQueue, frame::OperationType::START_SCAN);
                if (ack)
                        return;
                logger::log(ack.error());
        }
};

static std::expected<frame::systemMessage, error::Error>
waitFor(std::stop_token st, xqueue::Queue<frame::systemMessage>& mQueue, frame::Type type)
{
        logger::log(std::format("WAITING FOR {}", frame::toString(type)));

        const auto deadline = std::chrono::steady_clock::now() + frame::OPERATION_TIMEOUT;

        frame::systemMessage sm;
        do {
                if (st.stop_requested())
                        return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));

                if (!mQueue.waitData(st, deadline)) {
                        const auto code = st.stop_requested() ? error::Code::THREAD_ABORTED
                                                              : error::Code::OPERATION_TIMEOUT;
                        return std::unexpected(error::makeError(code));
                }

                sm = mQueue.pop();

                logger::log(std::format("DEVICE '{}'", sm.message));

                if (sm.type == frame::Type::STARTUP_FAILED)
                        return std::unexpected(error::makeError(error::Code::PROTOCOL_ERROR));
        } while (sm.type != type);

        return sm;
}

std::expected<frame::systemMessage, error::Error> timeSync(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        // send start time sync
        auto res = transmitter.session(st, mQueue, frame::OperationType::START_TIME_SYNC);
        if (!res)
                return std::unexpected(res.error());

        uint64_t sendTimeInt = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

        std::array<uint8_t, 16> payload = {};
        for (size_t i = 0; i < sizeof(uint64_t); ++i) {
                payload[i]                    = static_cast<uint8_t>(res->timestamp >> (8 * i));
                payload[sizeof(uint64_t) + i] = static_cast<uint8_t>(sendTimeInt >> (8 * i));
        }

        // send time
        auto ack = transmitter.session(st, mQueue, frame::OperationType::TIME, std::span(payload));
        if (!ack)
                return std::unexpected(ack.error());

        logger::log("TIME SYNC DONE");

        return ack;
}

} // namespace protocol
