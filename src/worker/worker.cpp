#include "worker/worker.hpp"

#include <utility>

namespace worker
{

Worker::Worker(std::unique_ptr<processor::Processor> instance) : instance(std::move(instance)) {}

std::expected<void, error::Error> Worker::dispatch()
{
        if (!this->instance)
                return std::unexpected(error::makeError(error::Code::WORKER_DISPATCH_FAILED));

        this->thread = std::jthread(
                [instance = this->instance.get()](std::stop_token st) { instance->run(st); });

        return {};
}

std::expected<void, error::Error> Worker::abort()
{
        if (!this->instance)
                return std::unexpected(error::makeError(error::Code::WORKER_DISPATCH_FAILED));

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
