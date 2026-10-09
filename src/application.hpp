#pragma once

#include "manager.hpp"

#include <expected>
#include <memory>
#include <string>

namespace application
{

std::expected<std::unique_ptr<manager::Manager>, error::Error> init(const std::string& path);

} // namespace application
