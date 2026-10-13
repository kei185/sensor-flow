#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace frame
{
/**
 * @see https://github.com/kei185/sensor-echo/blob/main/docs/frame.md#tx-frame
 */

extern const std::chrono::milliseconds OPERATION_TIMEOUT;
extern const uint8_t                   START_OF_FRAME[];

enum class OperationType
{
        START_SCAN,
        START_TIME_SYNC,
        TIME,
        HANDSHAKE,
        SOFT_RESET,
        MOTOR,
};
/**
 * Host -> MCU
 * command type
 */

extern const std::map<OperationType, std::array<uint8_t, 2>> OPERATION;
/**
 * OperationType -> command
 */

constexpr std::string_view toString(OperationType type)
{
        switch (type) {
                case OperationType::START_SCAN:
                        return "START_SCAN";
                case OperationType::START_TIME_SYNC:
                        return "START_TIME_SYNC";
                case OperationType::TIME:
                        return "TIME";
                case OperationType::HANDSHAKE:
                        return "HANDSHAKE";
                case OperationType::SOFT_RESET:
                        return "SOFT_RESET";
                case OperationType::MOTOR:
                        return "MOTOR";
                default:
                        return "UNKNOWN";
        }
}

enum class Type : uint8_t
{
        SYSTEM = 0x00,
        // Internal queue category; not a wire type.
        LIDAR               = 0x01,
        IMU                 = 0x02,
        ENCODER             = 0x03,
        INITIALIZING        = 0x04,
        DEVICE_INFO         = 0x05,
        HEALTH_STATUS       = 0x06,
        READY               = 0x07,
        STARTUP_FAILED      = 0x08,
        START_SCAN_ACK      = 0x09,
        START_TIME_SYNC_ACK = 0x0A,
        TIME_ACK            = 0x0B,
        TIME_REPORT         = 0x0C,
        HANDSHAKE_ACK       = 0x0D,
        MOTOR_ACK           = 0x0E,
        SOFT_RESET_ACK      = 0x0F,
        UNKNOWN             = 0x10,
};
/**
 * MCU -> Host
 * frame type
 */

extern const std::vector<Type> TYPES;

constexpr Type toAckType(OperationType type)
{
        switch (type) {
                case OperationType::START_SCAN:
                        return Type::START_SCAN_ACK;
                case OperationType::START_TIME_SYNC:
                        return Type::START_TIME_SYNC_ACK;
                case OperationType::TIME:
                        return Type::TIME_ACK;
                case OperationType::HANDSHAKE:
                        return Type::HANDSHAKE_ACK;
                case OperationType::SOFT_RESET:
                        return Type::SOFT_RESET_ACK;
                case OperationType::MOTOR:
                        return Type::MOTOR_ACK;
                default:
                        return Type::UNKNOWN;
        }
}

constexpr std::string_view toString(Type type)
{
        switch (type) {
                case Type::SYSTEM:
                        return "SYSTEM";
                case Type::LIDAR:
                        return "LIDAR";
                case Type::IMU:
                        return "IMU";
                case Type::ENCODER:
                        return "ENCODER";
                case Type::INITIALIZING:
                        return "INITIALIZING";
                case Type::DEVICE_INFO:
                        return "DEVICE_INFO";
                case Type::HEALTH_STATUS:
                        return "HEALTH_STATUS";
                case Type::READY:
                        return "READY";
                case Type::STARTUP_FAILED:
                        return "STARTUP_FAILED";
                case Type::START_SCAN_ACK:
                        return "START_SCAN_ACK";
                case Type::START_TIME_SYNC_ACK:
                        return "START_TIME_SYNC_ACK";
                case Type::TIME_ACK:
                        return "TIME_ACK";
                case Type::TIME_REPORT:
                        return "TIME_REPORT";
                case Type::HANDSHAKE_ACK:
                        return "HANDSHAKE_ACK";
                case Type::MOTOR_ACK:
                        return "MOTOR_ACK";
                case Type::SOFT_RESET_ACK:
                        return "SOFT_RESET_ACK";
                default:
                        return "UNKNOWN";
        }
}

constexpr Type frameQueueMUX(Type type)
{
        switch (type) {
                case Type::LIDAR:
                case Type::IMU:
                case Type::ENCODER:
                        return type;

                case Type::SYSTEM:
                case Type::INITIALIZING:
                case Type::DEVICE_INFO:
                case Type::HEALTH_STATUS:
                case Type::READY:
                case Type::STARTUP_FAILED:
                case Type::START_SCAN_ACK:
                case Type::START_TIME_SYNC_ACK:
                case Type::TIME_ACK:
                case Type::TIME_REPORT:
                case Type::HANDSHAKE_ACK:
                case Type::MOTOR_ACK:
                case Type::SOFT_RESET_ACK:
                        return Type::SYSTEM;

                default:
                        return Type::UNKNOWN;
        }
}

struct FrameHeader
{

        uint16_t length;
        Type     type;
        uint64_t timestamp;
};
extern const size_t FRAME_HEADER_SIZE;

struct Frame
{
        uint64_t             timestamp;
        uint16_t             length;
        Type                 type;
        std::vector<uint8_t> payload;
};
using Crc     = uint8_t;
using CrcData = uint16_t;

inline constexpr size_t CRC_OFFSET       = sizeof(CrcData);
inline constexpr size_t TYPE_OFFSET      = CRC_OFFSET + sizeof(Crc);
inline constexpr size_t TIMESTAMP_OFFSET = TYPE_OFFSET + sizeof(Type);
inline constexpr size_t RAW_HEADER_SIZE  = TIMESTAMP_OFFSET + sizeof(uint64_t);
// the base is at the payload length byte field

struct LidarPoint
{
        uint64_t            timestamp;
        double              angleStart;
        double              angleEnd;
        double              angleDiff;
        std::vector<double> point;
};
constexpr size_t LIDAR_POINT_SIZE = 2 + 2;

struct Acceleration
{
        int16_t x;
        int16_t y;
        int16_t z;
};
constexpr size_t IMU_ACCEL_VALUE_SIZE = 2;

struct Imu
{
        uint64_t     timestamp;
        Acceleration rot;
        Acceleration trans;
};

struct Encoder
{

        uint64_t timestamp;
        float    right;
        float    left;
};

struct systemMessage
{

        uint64_t    timestamp;
        std::string message;
        Type        type;
};

} // namespace frame
