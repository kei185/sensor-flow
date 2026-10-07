#include "manager.hpp"

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "utility/logger.hpp"
#include "utility/unwrap.hpp"
#include "worker/distributor.hpp"
#include "worker/parser.hpp"
#include "worker/receiver.hpp"
#include "worker/transmitter.hpp"

#include <cstddef>
#include <expected>
#include <map>
#include <memory>

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

} // namespace

DataStreams Manager::initDataStreams()
{
        DataStreams streams;
        streams.emplace(
                frame::Type::SYSTEM,
                std::make_unique<xqueue::Queue<frame::systemMessage>>());
        streams.emplace(frame::Type::LIDAR, std::make_unique<xqueue::Queue<frame::LidarPoint>>());
        streams.emplace(frame::Type::IMU, std::make_unique<xqueue::Queue<frame::Imu>>());
        return streams;
}

// TODO: low priority constructor injection　のほうがわかりやすいかも
Manager::Manager(const std::string file)
    : port(file),
      frameStreams(std::make_unique<std::map<frame::Type, xqueue::Queue<frame::Frame>>>()),
      dataStreams(initDataStreams()), transmitter(), receiverWorker(), parsers(), distributors()
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

        logger::log("RECEIVER DISPATCHED");

        // prepare parsers
        unwrap(Manager::initParsers(this->parsers, *this->frameStreams, this->dataStreams));

        logger::log("PARSERS DISPATCHED");

        // prepare distributors
        unwrap(Manager::initDistributors(
                this->distributors,
                this->dataStreams,
                *this->transmitter));

        logger::log("DISTRIBUTORS DISPATCHED");

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
                if (auto _result = p.dispatch(); !_result.has_value())
                        return _result;

        for (auto& [_, d] : this->distributors)
                if (auto _result = d.dispatch(); !_result.has_value())
                        return _result;

        this->receiverWorker.thread.join();

        return {};
}

std::expected<void, Error> Manager::initParsers(
        std::map<frame::Type, ParserWorker>&                parsers,
        std::map<frame::Type, xqueue::Queue<frame::Frame>>& frameStreams,
        DataStreams&                                        streams)
{
        auto* system = findDataStream<frame::systemMessage>(streams, frame::Type::SYSTEM);
        auto* lidar  = findDataStream<frame::LidarPoint>(streams, frame::Type::LIDAR);
        auto* imu    = findDataStream<frame::Imu>(streams, frame::Type::IMU);
        if (!system || !lidar || !imu)
                return std::unexpected<Error>(Error::PARSER_INIT_FAILED);

        for (auto type : frame::TYPES)
                if (auto [it, success] = parsers.try_emplace(type); !success)
                        return std::unexpected<Error>(Error::PARSER_INIT_FAILED);

        frame::Type type;

        type                   = frame::Type::SYSTEM;
        parsers[type].instance = std::make_unique<parser::Parser<frame::systemMessage>>(
                type,
                frameStreams[type],
                *system);

        type                   = frame::Type::LIDAR;
        parsers[type].instance = std::make_unique<parser::Parser<frame::LidarPoint>>(
                type,
                frameStreams[type],
                *lidar);

        type = frame::Type::IMU;
        parsers[type].instance =
                std::make_unique<parser::Parser<frame::Imu>>(type, frameStreams[type], *imu);

        // TODO 追加する

        return {};
}

std::expected<void, Error> Manager::initDistributors(
        std::map<frame::Type, DistributorWorker>& distributors,
        DataStreams&                              streams,
        transmitter::Transmitter&                 transmitter)
{
        auto* system = findDataStream<frame::systemMessage>(streams, frame::Type::SYSTEM);
        auto* lidar  = findDataStream<frame::LidarPoint>(streams, frame::Type::LIDAR);
        auto* imu    = findDataStream<frame::Imu>(streams, frame::Type::IMU);
        if (!system || !lidar || !imu)
                return std::unexpected<Error>(Error::DISTRIBUTOR_INIT_FAILED);

        for (auto type : frame::TYPES)
                if (auto [it, success] = distributors.try_emplace(type); !success)
                        return std::unexpected<Error>(Error::DISTRIBUTOR_INIT_FAILED);

        frame::Type type;

        type = frame::Type::SYSTEM;
        distributors[type].instance =
                std::make_unique<distributor::DeviceController>(type, transmitter, *system);

        type = frame::Type::LIDAR;
        distributors[type].instance =
                std::make_unique<distributor::Plotter<frame::LidarPoint>>(type, *lidar);

        type = frame::Type::IMU;
        distributors[type].instance =
                std::make_unique<distributor::Plotter<frame::Imu>>(type, *imu);

        // TODO 追加する

        return {};
}

} // namespace manager
