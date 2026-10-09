#include "worker/worker.hpp"

#include <utility>

namespace worker
{

Worker::Worker(std::unique_ptr<processor::Processor> instance) : instance(std::move(instance)) {}

std::expected<void, error::Error> Worker::dispatch()
{
        if (!this->instance)
                return std::unexpected<error::Error>(error::Error::WORKER_DISPATCH_FAILED);

        this->thread = std::jthread(
                [component = this->instance.get()](std::stop_token st) { component->run(st); });

        return {};
}

std::expected<void, error::Error> Worker::abort()
{
        if (!this->instance)
                return std::unexpected<error::Error>(error::Error::WORKER_DISPATCH_FAILED);

        this->thread.request_stop();

        return {};
}

Worker
worker(frame::Type                          type,
       transmitter::Transmitter&            transmitter,
       xqueue::Queue<frame::systemMessage>& dataStream)
{
        return Worker(
                std::make_unique<distributor::DeviceController>(type, transmitter, dataStream));
}

} // namespace worker
