#include "worker/worker.hpp"

namespace worker
{

DistributorWorker
worker(frame::Type                          type,
       transmitter::Transmitter&            transmitter,
       xqueue::Queue<frame::systemMessage>& dataStream)
{
        DistributorWorker worker;
        worker.instance =
                std::make_unique<distributor::DeviceController>(type, transmitter, dataStream);
        return worker;
}

} // namespace worker
