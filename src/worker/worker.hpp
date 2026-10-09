#pragma once

#include "core/frame.hpp"
#include "core/transmitter.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "worker/distributor.hpp"
#include "worker/parser.hpp"
#include "worker/processor.hpp"

#include <expected>
#include <memory>
#include <thread>

namespace worker
{

struct Worker
{
        std::unique_ptr<processor::Processor> instance;
        std::jthread                          thread;

        Worker() = default;
        explicit Worker(std::unique_ptr<processor::Processor>);

        std::expected<void, error::Error> dispatch();
        std::expected<void, error::Error> abort();
};

template <typename Payload>
Worker
worker(frame::Type                  type,
       xqueue::Queue<frame::Frame>& frameStream,
       xqueue::Queue<Payload>&      dataStream)
{
        return Worker(std::make_unique<parser::Parser<Payload>>(type, frameStream, dataStream));
}

template <typename Payload> Worker worker(frame::Type type, xqueue::Queue<Payload>& dataStream)
{
        return Worker(std::make_unique<distributor::Plotter<Payload>>(type, dataStream));
}

Worker worker(frame::Type, transmitter::Transmitter&, xqueue::Queue<frame::systemMessage>&);

} // namespace worker
