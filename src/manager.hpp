#pragma once

#include "core/frame.hpp"
#include "core/io.hpp"
#include "core/xqueue.hpp"
#include "utility/error.hpp"
#include "worker/distributor.hpp"
#include "worker/parser.hpp"
#include "worker/receiver.hpp"
#include "worker/transmitter.hpp"

#include <cstddef>
#include <cstdlib>
#include <expected>
#include <map>
#include <memory>
#include <thread>

#include <unistd.h>

using namespace error;

namespace manager
{

template <typename T, Error InitError> struct Worker
{
        std::unique_ptr<T> instance;
        std::jthread       thread;

        std::expected<void, Error> dispatch()
        {
                if (!this->instance)
                        return std::unexpected<Error>(InitError);

                this->thread = std::jthread([component = this->instance.get()](std::stop_token st) {
                        component->run(st);
                });

                return {};
        }

        std::expected<void, Error> abort()
        {
                if (!this->instance)
                        return std::unexpected<Error>(InitError);

                this->thread.request_stop();

                return {};
        }
};

using ReceiverWorker    = Worker<receiver::Receiver, Error::RECEIVER_INIT_FAILED>;
using ParserWorker      = Worker<parser::ParserBase, Error::PARSER_INIT_FAILED>;
using DistributorWorker = Worker<distributor::Distributor, Error::DISTRIBUTOR_INIT_FAILED>;

using DataStreams = std::map<frame::Type, std::unique_ptr<xqueue::QueueBase>>;

struct Manager
{
        io::Port port;

        std::unique_ptr<std::map<frame::Type, xqueue::Queue<frame::Frame>>> frameStreams;
        DataStreams                                                         dataStreams;

        std::unique_ptr<transmitter::Transmitter> transmitter;
        ReceiverWorker                            receiverWorker;
        std::map<frame::Type, ParserWorker>       parsers;
        std::map<frame::Type, DistributorWorker>  distributors;

        Manager(const std::string);
        ~Manager();
        std::expected<void, Error> run();

        static DataStreams initDataStreams();

        static std::expected<void, Error> initParsers(
                std::map<frame::Type, ParserWorker>&,
                std::map<frame::Type, xqueue::Queue<frame::Frame>>&,
                DataStreams&);

        static std::expected<void, Error> initDistributors(
                std::map<frame::Type, DistributorWorker>&,
                DataStreams&,
                transmitter::Transmitter&);
};

} // namespace manager
