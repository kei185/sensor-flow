#pragma once

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "core/transmitter.hpp"
#include "utility/error.hpp"
#include "worker/worker.hpp"

#include <expected>
#include <map>
#include <memory>

using namespace error;

namespace manager
{

using FrameStreams = std::map<frame::Type, std::unique_ptr<xqueue::Queue<frame::Frame>>>;
using DataStreams  = std::map<frame::Type, std::unique_ptr<xqueue::QueueBase>>;

struct Manager
{
        std::unique_ptr<io::Port> port;

        std::unique_ptr<FrameStreams> frameStreams;
        std::unique_ptr<DataStreams>  dataStreams;

        std::unique_ptr<transmitter::Transmitter> transmitter;

        worker::Worker                        receiverWorker;
        std::map<frame::Type, worker::Worker> parsers;
        std::map<frame::Type, worker::Worker> distributors;

        Manager(std::unique_ptr<io::Port>,
                std::unique_ptr<FrameStreams>,
                std::unique_ptr<DataStreams>,
                std::unique_ptr<transmitter::Transmitter>,
                worker::Worker,
                std::map<frame::Type, worker::Worker>,
                std::map<frame::Type, worker::Worker>);
        ~Manager();
        std::expected<void, Error> run();
};

} // namespace manager
