#pragma once

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "utility/logger.hpp"

#include <stop_token>
#include <vector>

namespace parser
{
struct ParserBase
{
        virtual void run(std::stop_token) = 0;
        virtual ~ParserBase()             = default;
};

template <typename T> struct Parser : public ParserBase
{
        frame::Type                  type;
        xqueue::Queue<frame::Frame>& inQueue;
        xqueue::Queue<T>&            outQueue;

        Parser(frame::Type type, xqueue::Queue<frame::Frame>& inQueue, xqueue::Queue<T>& outQueue)
            : type(type), inQueue(inQueue), outQueue(outQueue)
        {}

        void                  run(std::stop_token) override;
        static std::vector<T> parsePayload(frame::Frame& fr);
};

template <typename T> void Parser<T>::run(std::stop_token st)
{
        logger::log("PARSER DISPATCHED");

        while (!st.stop_requested()) {
                if (this->inQueue.empty())
                        continue;

                frame::Frame fr = this->inQueue.pop();

                this->outQueue.push_range(Parser<T>::parsePayload(fr));
        }

        logger::log("thread requested stop");
};

} // namespace parser
