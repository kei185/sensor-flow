#pragma once

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "core/transmitter.hpp"
#include "worker/processor.hpp"

#include <stop_token>

#include <fcntl.h>
#include <unistd.h>

namespace distributor
{

struct DeviceController : processor::Processor
{
        frame::Type                          type;
        transmitter::Transmitter&            transmitter;
        xqueue::Queue<frame::systemMessage>& inQueue;

        void run(std::stop_token) override;

        DeviceController(
                frame::Type,
                transmitter::Transmitter&,
                xqueue::Queue<frame::systemMessage>&);
};

template <typename T> struct Plotter : processor::Processor
{
        frame::Type       type;
        xqueue::Queue<T>& inQueue;

        Plotter(frame::Type type, xqueue::Queue<T>& inQueue) : type(type), inQueue(inQueue) {}
        ~Plotter() {}

        void run(std::stop_token) override;
};

} // namespace distributor
