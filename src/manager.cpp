#include "manager.hpp"

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "utility/logger.hpp"

#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <utility>

#include <unistd.h>

using namespace error;

namespace manager
{

Manager::Manager(
        std::unique_ptr<io::Port>                 port,
        std::unique_ptr<FrameStreams>             frameStreams,
        std::unique_ptr<DataStreams>              dataStreams,
        std::unique_ptr<transmitter::Transmitter> transmitter,
        worker::Worker                            receiverWorker,
        std::map<frame::Type, worker::Worker>     parsers,
        std::map<frame::Type, worker::Worker>     distributors)
    : port(std::move(port)), frameStreams(std::move(frameStreams)),
      dataStreams(std::move(dataStreams)), transmitter(std::move(transmitter)),
      receiverWorker(std::move(receiverWorker)), parsers(std::move(parsers)),
      distributors(std::move(distributors))
{}

Manager::~Manager()
{
        this->receiverWorker.thread.request_stop();

        for (auto& [_, p] : this->parsers)
                if (auto _result = p.abort(); !_result.has_value())
                        logger::log(_result.error());

        for (auto& [_, d] : this->distributors)
                if (auto _result = d.abort(); !_result.has_value())
                        logger::log(_result.error());
}

std::expected<void, Error> Manager::run()
{
        if (auto _result = this->receiverWorker.dispatch(); !_result.has_value())
                return _result;

        for (auto& [_, p] : this->parsers)
                if (auto _result = p.dispatch(); !_result.has_value()) {
                        logger::log(std::format("ERROR PARSER TYPE {}", frame::toString(_)));
                        return _result;
                }

        for (auto& [_, d] : this->distributors)
                if (auto _result = d.dispatch(); !_result.has_value()) {
                        logger::log(std::format("ERROR DISTRIBUTOR TYPE {}", frame::toString(_)));
                        return _result;
                }

        logger::log("ALL THREAD RUNNING\n");
        this->receiverWorker.thread.join();

        return {};
}

} // namespace manager
