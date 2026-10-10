#pragma once

#include "core/frame.hpp"
#include "core/xqueue.hpp"
#include "core/transmitter.hpp"

#include <expected>
#include <stop_token>

namespace protocol
{

void run(std::stop_token, transmitter::Transmitter&, xqueue::Queue<frame::systemMessage>&);

std::expected<void, error::Error>
timSync(std::stop_token, transmitter::Transmitter&, xqueue::Queue<frame::systemMessage>&);

} // namespace protocol
