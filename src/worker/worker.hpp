#pragma once

#include "core/frame.hpp"
#include "core/transmitter.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "worker/distributor.hpp"
#include "worker/parser.hpp"
#include "worker/receiver.hpp"

#include <expected>
#include <memory>
#include <stop_token>
#include <thread>

namespace worker
{

template <typename T> struct Worker
{
        std::unique_ptr<T> instance;
        std::jthread       thread;

        std::expected<void, error::Error> dispatch()
        {
                if (!this->instance)
                        return std::unexpected<error::Error>(error::Error::WORKER_DISPATCH_FAILED);

                this->thread = std::jthread([component = this->instance.get()](std::stop_token st) {
                        component->run(st);
                });

                return {};
        }

        std::expected<void, error::Error> abort()
        {
                if (!this->instance)
                        return std::unexpected<error::Error>(Error::WORKER_DISPATCH_FAILED);

                this->thread.request_stop();

                return {};
        }
};

using ReceiverWorker    = Worker<receiver::Receiver>;
using ParserWorker      = Worker<parser::ParserBase>;
using DistributorWorker = Worker<distributor::Distributor>;

template <typename Payload>
ParserWorker
worker(frame::Type                  type,
       xqueue::Queue<frame::Frame>& frameStream,
       xqueue::Queue<Payload>&      dataStream)
{
        ParserWorker worker;
        worker.instance = std::make_unique<parser::Parser<Payload>>(type, frameStream, dataStream);
        return worker;
}

template <typename Payload>
DistributorWorker worker(frame::Type type, xqueue::Queue<Payload>& dataStream)
{
        DistributorWorker worker;
        worker.instance = std::make_unique<distributor::Plotter<Payload>>(type, dataStream);
        return worker;
}

DistributorWorker
worker(frame::Type, transmitter::Transmitter&, xqueue::Queue<frame::systemMessage>&);

} // namespace worker
