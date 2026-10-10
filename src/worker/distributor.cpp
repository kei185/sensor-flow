#include "core/protocol.hpp"
#include "worker/distributor.hpp"
#include "demo/gnuplot.hpp"

#include <stop_token>

namespace distributor
{

/**
 * device
 */

DeviceController::DeviceController(
        frame::Type                          type,
        transmitter::Transmitter&            transmiter,
        xqueue::Queue<frame::systemMessage>& inQueue)
    : type(type), transmitter(transmiter), inQueue(inQueue) {};

void DeviceController::run(std::stop_token st)
{
        protocol::run(st, this->transmitter, this->inQueue);
}

/**
 * Lidar
 */

template <> void Plotter<frame::LidarPoint>::run(std::stop_token st)
{
        demo::run(st, this->inQueue);
}

/**
 * IMU
 */

template <> void Plotter<frame::Imu>::run(std::stop_token st)
{
        demo::runImuOrientation(st, this->inQueue);
}

/**
 * Encoder
 */

template <> void Plotter<frame::Encoder>::run(std::stop_token st)
{
        // TODO
        while (!st.stop_requested()) {
                if (!this->inQueue.waitData(st))
                        break;

                this->inQueue.pop();
        }
}

} // namespace distributor
