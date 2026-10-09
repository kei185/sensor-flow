#pragma once

#include "utility/error.hpp"

#include <print>
#include <source_location>
#include <string_view>
#include <thread>

namespace logger
{

inline void
log(std::string_view message, std::source_location location = std::source_location::current())
{
        std::println("[{}] {}: {}", std::this_thread::get_id(), location.function_name(), message);
}

inline void log(const error::Error& err)
{
        std::println("[{}] {}", std::this_thread::get_id(), error::toString(err));
}

} // namespace logger
