#include "core/protocol.hpp"

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "utility/logger.hpp"
#include "utility/unwrap.hpp"
#include "core/transmitter.hpp"

#include <expected>
#include <iostream>
#include <print>
#include <stop_token>
#include <string>

namespace protocol
{

static void waitforReady(std::stop_token&, xqueue::Queue<frame::systemMessage>&);

void run(
        std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{
        waitforReady(st, mQueue);
        if (st.stop_requested())
                return;

        std::println("press ENTER to start scan");
        std::string s;
        std::getline(std::cin, s);
        unwrap(transmitter.session(st, mQueue, frame::OperationType::START_SCAN));
};

static void waitforReady(std::stop_token& st, xqueue::Queue<frame::systemMessage>& mQueue)
{
        logger::log("WAITING FOR READY...");

        frame::systemMessage sm;
        do {
                if (st.stop_requested())
                        return;

                if (!mQueue.waitData(st))
                        return;

                sm = mQueue.pop();

                logger::log(std::format("DEVICE '{}'", sm.message));
        } while (sm.type != frame::Type::READY);
}

std::expected<void, error::Error>
timSync(std::stop_token                      st,
        transmitter::Transmitter&            transmitter,
        xqueue::Queue<frame::systemMessage>& mQueue)
{

        // send start time sync
        auto res = unwrap(transmitter.session(st, mQueue, frame::OperationType::START_TIME_SYNC));

        uint64_t sendTimeInt = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

        auto payload = std::vector<uint8_t>(
                (uint8_t*)&sendTimeInt,
                (uint8_t*)&sendTimeInt + sizeof(uint64_t));

        auto receivedTime = std::vector<uint8_t>(
                (uint8_t*)&sendTimeInt,
                (uint8_t*)&sendTimeInt + sizeof(uint64_t));

        payload.insert(payload.end(), receivedTime.begin(), receivedTime.end());

        // send time
        unwrap(transmitter.session(st, mQueue, frame::OperationType::TIME, std::span(payload)));

        logger::log("TIME SYNC DONE");
        return {};
}

} // namespace protocol
