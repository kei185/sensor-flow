#include "core/protocol.hpp"

#include "utility/error.hpp"
#include "utility/logger.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <print>
#include <stop_token>

#include <poll.h>
#include <unistd.h>

namespace protocol
{

static constexpr auto HANDSHAKE_INTERVAL = std::chrono::seconds(3);

static std::expected<frame::systemMessage, error::Error>
waitFor(std::stop_token st, xqueue::Queue<frame::systemMessage>& mQueue, frame::Type type)
{
        const auto deadline = std::chrono::steady_clock::now() + frame::OPERATION_TIMEOUT;
        if (st.stop_requested())
                return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));

        if (!mQueue.waitData(st, deadline)) {
                const auto code = st.stop_requested() ? error::Code::THREAD_ABORTED
                                                      : error::Code::OPERATION_TIMEOUT;
                return std::unexpected(error::makeError(code));
        }

        auto message = mQueue.pop();
        logger::log(std::format("DEVICE '{}'", message.message));
        if (message.type != type)
                return std::unexpected(error::makeError(error::Code::PROTOCOL_ERROR));

        return message;
}

static std::expected<void, error::Error> handshake(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        logger::log("WAITING FOR HANDSHAKE ACK...");
        while (!st.stop_requested()) {
                // A new handshake starts a fresh controller session.
                while (!mQueue.empty())
                        mQueue.pop();

                const auto retryAt = std::chrono::steady_clock::now() + HANDSHAKE_INTERVAL;
                auto       result  = transmitter.session(
                        st,
                        mQueue,
                        frame::OperationType::HANDSHAKE,
                        {},
                        HANDSHAKE_INTERVAL);
                if (result)
                        return {};
                if (result.error().code == error::Code::THREAD_ABORTED)
                        return std::unexpected(result.error());

                logger::log(result.error());
                // Failed writes and invalid replies also keep the three-second retry interval.
                std::mutex                  mutex;
                std::condition_variable_any condition;
                std::unique_lock            lock(mutex);
                condition.wait_until(lock, st, retryAt, [] { return false; });
        }

        return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));
}

static std::expected<void, error::Error>
waitForReady(std::stop_token st, xqueue::Queue<frame::systemMessage>& mQueue)
{
        logger::log("WAITING FOR READY...");
        for (auto type : {frame::Type::INITIALIZING,
                          frame::Type::DEVICE_INFO,
                          frame::Type::HEALTH_STATUS,
                          frame::Type::READY}) {
                auto result = waitFor(st, mQueue, type);
                if (!result)
                        return std::unexpected(result.error());
        }

        return {};
}

static std::expected<void, error::Error> timeSync(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        auto ack = transmitter.session(st, mQueue, frame::OperationType::START_TIME_SYNC);
        if (!ack)
                return std::unexpected(ack.error());

        std::array<uint8_t, 16> payload = {};
        for (size_t i = 0; i < sizeof(uint64_t); ++i)
                payload[i] = static_cast<uint8_t>(ack->timestamp >> (8 * i));

        const uint64_t sendTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        if (sendTime < ack->timestamp)
                return std::unexpected(error::makeError(error::Code::PROTOCOL_ERROR));

        for (size_t i = 0; i < sizeof(uint64_t); ++i)
                payload[sizeof(uint64_t) + i] = static_cast<uint8_t>(sendTime >> (8 * i));

        auto result = transmitter.session(st, mQueue, frame::OperationType::TIME, payload);
        if (!result)
                return std::unexpected(result.error());

        auto report = waitFor(st, mQueue, frame::Type::TIME_REPORT);
        if (!report)
                return std::unexpected(report.error());

        logger::log("TIME SYNC DONE");
        return {};
}

static std::expected<void, error::Error>
waitForStart(std::stop_token st, xqueue::Queue<frame::systemMessage>& mQueue)
{
        std::println("press ENTER to start scan");
        while (!st.stop_requested()) {
                if (!mQueue.empty()) {
                        logger::log(std::format("DEVICE '{}'", mQueue.pop().message));
                        return std::unexpected(error::makeError(error::Code::PROTOCOL_ERROR));
                }

                pollfd    input{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
                const int ready = poll(&input, 1, 100);
                if (ready < 0) {
                        if (errno == EINTR)
                                continue;
                        return std::unexpected(error::makeError(error::Code::IO_READ_FAILED));
                }
                if (ready == 0)
                        continue;

                char       byte = {};
                const auto size = read(STDIN_FILENO, &byte, 1);
                if (size < 0) {
                        if (errno == EINTR)
                                continue;
                        return std::unexpected(error::makeError(error::Code::IO_READ_FAILED));
                }
                // Closing stdin ends the controller without sending a start command.
                if (size == 0)
                        return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));
                if (byte == '\n')
                        return {};
        }

        return std::unexpected(error::makeError(error::Code::THREAD_ABORTED));
}

static std::expected<void, error::Error>
startup(std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        auto result = handshake(st, transmitter, mQueue);
        if (!result)
                return result;
        result = waitForReady(st, mQueue);
        if (!result)
                return result;
        result = timeSync(st, transmitter, mQueue);
        if (!result)
                return result;
        result = waitForStart(st, mQueue);
        if (!result)
                return result;

        auto ack = transmitter.session(st, mQueue, frame::OperationType::START_SCAN);
        if (!ack)
                return std::unexpected(ack.error());

        logger::log("SCAN STARTED");
        return {};
}

void run(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        while (!st.stop_requested()) {
                auto result = startup(st, transmitter, mQueue);
                if (!result) {
                        if (result.error().code == error::Code::THREAD_ABORTED)
                                return;
                        logger::log(result.error());
                        logger::log("RESTARTING HANDSHAKE...");
                        continue;
                }

                // The handshake is a connection request, not a periodic heartbeat.
                while (!st.stop_requested()) {
                        if (!mQueue.waitData(st))
                                return;
                        auto message = mQueue.pop();
                        logger::log(std::format("DEVICE '{}'", message.message));
                        if (message.type != frame::Type::MOTOR_ACK) {
                                logger::log("CONTROLLER SESSION ENDED; RESTARTING HANDSHAKE...");
                                break;
                        }
                }
        }
}

} // namespace protocol
