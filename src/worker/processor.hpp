#pragma once

#include <stop_token>

namespace processor
{

struct Processor
{
        virtual void run(std::stop_token) = 0;
        virtual ~Processor()              = default;
};

} // namespace processor
