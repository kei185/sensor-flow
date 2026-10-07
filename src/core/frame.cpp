#include "core/frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <stdint.h>
#include <sys/types.h>

namespace frame
{
const std::chrono::milliseconds OPERATION_TIMEOUT(5000);

const size_t            FRAME_HEADER_SIZE = RAW_HEADER_SIZE;
const uint8_t           START_OF_FRAME[]  = {0xAA, 0x55};
const std::vector<Type> TYPES             = {Type::SYSTEM, Type::LIDAR, Type::IMU, Type::ENCODER};

// static const std::array<uint8_t, 2> COMMAND_GET_STATUS = {0xAA, 0xA1};
static const std::array<uint8_t, 2> COMMAND_START_SCAN = {0xAA, 0xA2};
// static const std::array<uint8_t, 2> COMMAND_END_SCAN   = {0xAA, 0xA3};
static const std::array<uint8_t, 2> COMMAND_START_TIME_SYNC = {0xAA, 0xA4};
static const std::array<uint8_t, 2> COMMAND_TIME            = {0xAA, 0xA5};

const std::map<OperationType, std::array<uint8_t, 2>> OPERATION = {
        {OperationType::START_SCAN, COMMAND_START_SCAN},
        {OperationType::START_TIME_SYNC, COMMAND_START_TIME_SYNC},
        {OperationType::TIME, COMMAND_TIME},
};

} // namespace frame
