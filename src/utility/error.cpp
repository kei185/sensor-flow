
#include "utility/error.hpp"

#include <format>

namespace error
{

Error makeError(Code code, std::source_location location) noexcept { return {code, location}; }

std::string toString(const Error& error)
{
        return std::format(
                "{} at {}:{}:{} in {}",
                toString(error.code),
                error.location.file_name(),
                error.location.line(),
                error.location.column(),
                error.location.function_name());
}

} // namespace error
