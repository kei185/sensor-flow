#include "manager.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <print>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace
{

static_assert(std::is_abstract_v<xqueue::QueueBase>);
static_assert(std::has_virtual_destructor_v<xqueue::QueueBase>);

void require(bool condition, const char* message)
{
        if (!condition)
                throw std::runtime_error(message);
}

template <typename T> xqueue::Queue<T>& stream(manager::DataStreams& streams, frame::Type type)
{
        auto* queue = dynamic_cast<xqueue::Queue<T>*>(streams.at(type).get());
        require(queue != nullptr, "stream has an unexpected payload type");
        return *queue;
}

template <typename T> void checkParser(manager::Manager& manager, frame::Type type)
{
        auto* parser = dynamic_cast<parser::Parser<T>*>(manager.parsers.at(type).instance.get());
        require(parser != nullptr, "parser has an unexpected payload type");
        require(&parser->inQueue == &manager.frameStreams->at(type), "wrong parser input queue");
        require(&parser->outQueue == &stream<T>(manager.dataStreams, type),
                "wrong parser output queue");
}

template <typename T> void checkPlotter(manager::Manager& manager, frame::Type type)
{
        auto* plotter = dynamic_cast<distributor::Plotter<T>*>(
                manager.distributors.at(type).instance.get());
        require(plotter != nullptr, "plotter has an unexpected payload type");
        require(&plotter->inQueue == &stream<T>(manager.dataStreams, type),
                "wrong plotter input queue");
}

void checkWiring(manager::Manager& manager)
{
        require(manager.dataStreams.size() == frame::TYPES.size(), "missing or extra stream");
        for (auto type : frame::TYPES) {
                require(manager.dataStreams.at(type) != nullptr, "null stream");
                require(manager.dataStreams.at(type)->empty(), "new stream is not empty");
        }

        checkParser<frame::systemMessage>(manager, frame::Type::SYSTEM);
        checkParser<frame::LidarPoint>(manager, frame::Type::LIDAR);
        checkParser<frame::Imu>(manager, frame::Type::IMU);
        checkPlotter<frame::LidarPoint>(manager, frame::Type::LIDAR);
        checkPlotter<frame::Imu>(manager, frame::Type::IMU);

        auto* controller = dynamic_cast<distributor::DeviceController*>(
                manager.distributors.at(frame::Type::SYSTEM).instance.get());
        require(controller != nullptr, "missing system distributor");
        require(&controller->inQueue ==
                        &stream<frame::systemMessage>(manager.dataStreams, frame::Type::SYSTEM),
                "wrong system distributor input queue");
}

void checkParserOutput(manager::Manager& manager)
{
        auto& output = stream<frame::LidarPoint>(manager.dataStreams, frame::Type::LIDAR);
        manager.frameStreams->at(frame::Type::LIDAR)
                .push({
                        .length  = 8,
                        .type    = frame::Type::LIDAR,
                        .payload = {0x34, 0x12, 0x40, 0x00, 0x78, 0x56, 0x80, 0x00},
                });

        std::jthread worker([&](std::stop_token stop) {
                manager.parsers.at(frame::Type::LIDAR).instance->run(stop);
        });
        auto         deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (output.empty() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        worker.request_stop();
        worker.join();

        require(!output.empty(), "parser did not publish to the mapped stream");
        auto first = output.pop();
        require(first.dist == 0x1234 && first.angle == 1.0f, "incorrect first LiDAR point");
        require(!output.empty(), "parser lost the second LiDAR point");
        auto second = output.pop();
        require(second.dist == 0x5678 && second.angle == 2.0f, "incorrect second LiDAR point");
        require(output.empty(), "unexpected extra LiDAR points");
        require(manager.dataStreams.at(frame::Type::SYSTEM)->empty(), "data leaked into SYSTEM");
        require(manager.dataStreams.at(frame::Type::IMU)->empty(), "data leaked into IMU");
}

void checkInvalidStreams(transmitter::Transmitter& transmitter)
{
        for (auto type : frame::TYPES) {
                for (int invalid = 0; invalid < 3; ++invalid) {
                        auto streams = manager::Manager::initDataStreams();
                        if (invalid == 0)
                                streams.erase(type);
                        else if (invalid == 1)
                                streams.at(type).reset();
                        else
                                streams.at(type) = std::make_unique<xqueue::Queue<int>>();

                        auto                                         streamCount = streams.size();
                        std::map<frame::Type, manager::ParserWorker> parsers;
                        std::map<frame::Type, xqueue::Queue<frame::Frame>> frames;
                        auto parsed = manager::Manager::initParsers(parsers, frames, streams);
                        require(!parsed && parsed.error() == error::Error::PARSER_INIT_FAILED,
                                "parser accepted a missing, null, or mistyped stream");
                        require(parsers.empty(),
                                "invalid stream left partially initialized parsers");

                        std::map<frame::Type, manager::DistributorWorker> distributors;
                        auto distributed = manager::Manager::initDistributors(
                                distributors,
                                streams,
                                transmitter);
                        require(!distributed && distributed.error() ==
                                                        error::Error::DISTRIBUTOR_INIT_FAILED,
                                "distributor accepted a missing, null, or mistyped stream");
                        require(distributors.empty(), "invalid stream left partial distributors");
                        require(streams.size() == streamCount, "lookup inserted a missing stream");
                }
        }
}

void checkPolymorphicDestruction()
{
        struct TrackedQueue : xqueue::Queue<int>
        {
                bool& destroyed;
                explicit TrackedQueue(bool& destroyed) : destroyed(destroyed) {}
                ~TrackedQueue() override { destroyed = true; }
        };

        bool destroyed = false;
        {
                manager::DataStreams streams;
                streams.emplace(frame::Type::SYSTEM, std::make_unique<TrackedQueue>(destroyed));
        }
        require(destroyed, "map did not destroy the derived queue");
}

} // namespace

int main()
{
        try {
                // Construct the workers without starting serial I/O or gnuplot.
                manager::Manager manager("/dev/null");
                checkWiring(manager);
                checkParserOutput(manager);
                checkInvalidStreams(*manager.transmitter);
                checkPolymorphicDestruction();
                std::println("DataStreams regression checks passed");
        } catch (const std::exception& error) {
                std::println(stderr, "DataStreams regression check failed: {}", error.what());
                return 1;
        }
}
