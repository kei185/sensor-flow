#pragma once

#include "utility/error.hpp"
#include "utility/logger.hpp"

#include <cstdlib>
#include <expected>
#include <utility>

[[noreturn]] inline void abortByError(const error::Error& err)
{
        logger::log(err);
        std::exit(1);
}

template <typename T> T unwrap(std::expected<T, error::Error> result)
{
        if (!result)
                abortByError(result.error());

        return std::move(result).value();
}
