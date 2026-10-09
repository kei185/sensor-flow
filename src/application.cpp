#include "application.hpp"
#include "core/frame.hpp"
#include "worker/worker.hpp"

#include <map>
#include <memory>
#include <utility>

namespace application
{

std::expected<std::unique_ptr<manager::Manager>, error::Error> init(const std::string& path)
{
        auto port         = std::make_unique<io::Port>(path);
        auto frameStreams = std::make_unique<manager::FrameStreams>();
        auto dataStreams  = std::make_unique<manager::DataStreams>();
        auto transmitter  = std::make_unique<transmitter::Transmitter>(*port);

        worker::ReceiverWorker receiver;
        receiver.instance = std::make_unique<receiver::Receiver>(*port, *frameStreams);

        std::map<frame::Type, worker::ParserWorker>      parsers;
        std::map<frame::Type, worker::DistributorWorker> distributors;

        auto type = frame::Type::SYSTEM;
        frameStreams->try_emplace(type, std::make_unique<xqueue::Queue<frame::Frame>>());
        auto systemStream  = std::make_unique<xqueue::Queue<frame::systemMessage>>();
        parsers[type]      = worker::worker(type, *frameStreams->at(type), *systemStream);
        distributors[type] = worker::worker(type, *transmitter, *systemStream);
        dataStreams->try_emplace(type, std::move(systemStream));

        type = frame::Type::LIDAR;
        frameStreams->try_emplace(type, std::make_unique<xqueue::Queue<frame::Frame>>());
        auto lidarStream   = std::make_unique<xqueue::Queue<frame::LidarPoint>>();
        parsers[type]      = worker::worker(type, *frameStreams->at(type), *lidarStream);
        distributors[type] = worker::worker(type, *lidarStream);
        dataStreams->try_emplace(type, std::move(lidarStream));

        type = frame::Type::IMU;
        frameStreams->try_emplace(type, std::make_unique<xqueue::Queue<frame::Frame>>());
        auto imuStream     = std::make_unique<xqueue::Queue<frame::Imu>>();
        parsers[type]      = worker::worker(type, *frameStreams->at(type), *imuStream);
        distributors[type] = worker::worker(type, *imuStream);
        dataStreams->try_emplace(type, std::move(imuStream));

        type = frame::Type::ENCODER;
        frameStreams->try_emplace(type, std::make_unique<xqueue::Queue<frame::Frame>>());
        auto encoderStream = std::make_unique<xqueue::Queue<frame::Encoder>>();
        parsers[type]      = worker::worker(type, *frameStreams->at(type), *encoderStream);
        distributors[type] = worker::worker(type, *encoderStream);
        dataStreams->try_emplace(type, std::move(encoderStream));

        return std::make_unique<manager::Manager>(
                std::move(port),
                std::move(frameStreams),
                std::move(dataStreams),
                std::move(transmitter),
                std::move(receiver),
                std::move(parsers),
                std::move(distributors));
}

} // namespace application
