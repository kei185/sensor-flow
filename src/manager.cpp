#include "manager.hpp"

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "core/transmitter.hpp"
#include "utility/error.hpp"
#include "utility/logger.hpp"
#include "utility/unwrap.hpp"
#include "worker/distributor.hpp"
#include "worker/parser.hpp"
#include "worker/receiver.hpp"

#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <utility>

#include <unistd.h>

using namespace error;

namespace manager
{

namespace
{

template <typename T> xqueue::Queue<T>* findDataStream(DataStreams& streams, frame::Type type)
{
        auto stream = streams.find(type);
        if (stream == streams.end())
                return nullptr;

        return dynamic_cast<xqueue::Queue<T>*>(stream->second.get());
}

DataStreams makeDataStreams()
{
        DataStreams streams;

        for (auto type : frame::TYPES) {
                std::unique_ptr<xqueue::QueueBase> stream;

                switch (type) {
                        case frame::Type::SYSTEM:
                                stream = std::make_unique<xqueue::Queue<frame::systemMessage>>();
                                break;
                        case frame::Type::LIDAR:
                                stream = std::make_unique<xqueue::Queue<frame::LidarPoint>>();
                                break;
                        case frame::Type::IMU:
                                stream = std::make_unique<xqueue::Queue<frame::Imu>>();
                                break;
                        case frame::Type::ENCODER:
                                stream = std::make_unique<xqueue::Queue<frame::Encoder>>();
                                break;
                        default:
                                continue;
                }

                streams.try_emplace(type, std::move(stream));
        }

        return streams;
}

} // namespace

// TODO: low priority constructor injection　のほうがわかりやすいかも
Manager::Manager(const std::string file)
    : port(file),
      frameStreams(std::make_unique<std::map<frame::Type, xqueue::Queue<frame::Frame>>>()),
      dataStreams(makeDataStreams()), transmitter(), receiverWorker(), parsers(), distributors()
{
        // prepare transmitter
        this->transmitter = std::make_unique<transmitter::Transmitter>(this->port);
        logger::log("TRANSMITTER DISPATCHED");

        // prepare frame streams
        for (auto type : frame::TYPES)
                this->frameStreams->try_emplace(type);

        // prepare receiver
        this->receiverWorker.instance =
                std::make_unique<receiver::Receiver>(this->port, *this->frameStreams);

        logger::log("RECEIVER INITIALIZED");

        // prepare parsers
        unwrap(Manager::initParsers(this->parsers, *this->frameStreams, this->dataStreams));

        logger::log("PARSERS INITIALIZED");

        // prepare distributors
        unwrap(Manager::initDistributors(
                this->distributors,
                this->dataStreams,
                *this->transmitter));

        logger::log("DISTRIBUTORS INITIALIZED");

        logger::log("APPLICATION RUNNING\n");
}

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

        this->receiverWorker.thread.join();

        return {};
}

std::expected<void, Error> Manager::initParsers(
        std::map<frame::Type, ParserWorker>&                parsers,
        std::map<frame::Type, xqueue::Queue<frame::Frame>>& frameStreams,
        DataStreams&                                        streams)
{
        for (auto type : frame::TYPES) {
                auto frameStream = frameStreams.find(type);
                if (frameStream == frameStreams.end())
                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);

                ParserWorker worker;

                switch (type) {
                        case frame::Type::SYSTEM: {
                                auto* stream = findDataStream<frame::systemMessage>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
                                worker.instance =
                                        std::make_unique<parser::Parser<frame::systemMessage>>(
                                                type,
                                                frameStream->second,
                                                *stream);
                                break;
                        }
                        case frame::Type::LIDAR: {
                                auto* stream = findDataStream<frame::LidarPoint>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
                                worker.instance =
                                        std::make_unique<parser::Parser<frame::LidarPoint>>(
                                                type,
                                                frameStream->second,
                                                *stream);
                                break;
                        }
                        case frame::Type::IMU: {
                                auto* stream = findDataStream<frame::Imu>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
                                worker.instance = std::make_unique<parser::Parser<frame::Imu>>(
                                        type,
                                        frameStream->second,
                                        *stream);
                                break;
                        }
                        case frame::Type::ENCODER: {
                                auto* stream = findDataStream<frame::Encoder>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
                                worker.instance = std::make_unique<parser::Parser<frame::Encoder>>(
                                        type,
                                        frameStream->second,
                                        *stream);
                                break;
                        }
                        default:
                                return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
                }

                if (!parsers.try_emplace(type, std::move(worker)).second)
                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);
        }

        return {};
}

std::expected<void, Error> Manager::initDistributors(
        std::map<frame::Type, DistributorWorker>& distributors,
        DataStreams&                              streams,
        transmitter::Transmitter&                 transmitter)
{
        for (auto type : frame::TYPES) {
                DistributorWorker worker;

                switch (type) {
                        case frame::Type::SYSTEM: {
                                auto* stream = findDataStream<frame::systemMessage>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(
                                                Error::DISTRIBUTOR_INIT_FAILED);
                                worker.instance = std::make_unique<distributor::DeviceController>(
                                        type,
                                        transmitter,
                                        *stream);
                                break;
                        }
                        case frame::Type::LIDAR: {
                                auto* stream = findDataStream<frame::LidarPoint>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(
                                                Error::DISTRIBUTOR_INIT_FAILED);
                                worker.instance =
                                        std::make_unique<distributor::Plotter<frame::LidarPoint>>(
                                                type,
                                                *stream);
                                break;
                        }
                        case frame::Type::IMU: {
                                auto* stream = findDataStream<frame::Imu>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(
                                                Error::DISTRIBUTOR_INIT_FAILED);
                                worker.instance =
                                        std::make_unique<distributor::Plotter<frame::Imu>>(
                                                type,
                                                *stream);
                                break;
                        }
                        case frame::Type::ENCODER: {
                                auto* stream = findDataStream<frame::Encoder>(streams, type);
                                if (!stream)
                                        return std::unexpected<Error>(
                                                Error::DISTRIBUTOR_INIT_FAILED);
                                worker.instance =
                                        std::make_unique<distributor::Plotter<frame::Encoder>>(
                                                type,
                                                *stream);
                                break;
                        }
                        default:
                                return std::unexpected<Error>(Error::DISTRIBUTOR_INIT_FAILED);
                }

                if (!distributors.try_emplace(type, std::move(worker)).second)
                        return std::unexpected<Error>(Error::DISTRIBUTOR_INIT_FAILED);
        }

        return {};
}

} // namespace manager
