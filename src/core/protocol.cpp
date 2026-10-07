#include "core/protocol.hpp"

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "utility/logger.hpp"
#include "utility/unwrap.hpp"
#include "core/transmitter.hpp"

#include <iostream>
#include <print>
#include <stop_token>
#include <string>

namespace protocol
{

static void waitforReady(std::stop_token& st, xqueue::Queue<frame::systemMessage>& mQueue)
{
        logger::log("WAITING FOR READY...");

        frame::systemMessage sm;
        do {
                if (st.stop_requested())
                        return;

                if (mQueue.empty())
                        continue;

                sm = mQueue.pop();

                logger::log(std::format("DEVICE '{}'", sm.message));
        } while (sm.type != frame::Type::READY);
}

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
        unwrap(transmitter.request(st, frame::OperationType::START_SCAN, mQueue));
};

} // namespace protocol
