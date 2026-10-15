#include "worker/parser.hpp"
#include "core/frame.hpp"
#include "utility/toInt.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace parser
{

template <>
std::vector<frame::systemMessage> Parser<frame::systemMessage>::parsePayload(frame::Frame& fr)
{
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
        // Time synchronization uses the host's ACK receive time.

        return {{
                .timestamp = timestamp,
                .message   = std::string(fr.payload.begin(), fr.payload.end()),
                .type      = fr.type,
        }};
}

template <> std::vector<frame::LidarPoint> Parser<frame::LidarPoint>::parsePayload(frame::Frame& fr)
{
        const size_t count = fr.payload.size() / frame::LIDAR_POINT_SIZE;
        if (count == 0)
                return {};

        std::vector<frame::LidarPoint> points(1);
        auto&                          point = points.front();
        point.timestamp                      = fr.timestamp;
        point.angleStart                     = toInt16(fr.payload.data() + sizeof(uint16_t)) / 64.0;
        point.angleEnd = toInt16(fr.payload.data() + (count - 1) * frame::LIDAR_POINT_SIZE +
                                 sizeof(uint16_t)) /
                         64.0;

        double span = point.angleEnd - point.angleStart;
        if (span < 0.0)
                span += 360.0;
        point.angleDiff = count > 1 ? span / (count - 1) : 0.0;

        point.point.reserve(count);
        for (size_t offset = 0; offset + frame::LIDAR_POINT_SIZE <= fr.payload.size();
             offset += frame::LIDAR_POINT_SIZE)
                point.point.push_back(toInt16(fr.payload.data() + offset));

        return points;
}

template <> std::vector<frame::Imu> Parser<frame::Imu>::parsePayload(frame::Frame& fr)
{
        if (fr.payload.size() < 6 * frame::IMU_ACCEL_VALUE_SIZE)
                return {};

        uint8_t* head = fr.payload.data();

        return {(frame::Imu){
                .timestamp = fr.timestamp,
                .rot =
                        {
                                .x = toSignedInt16(head),
                                .y = toSignedInt16(head += frame::IMU_ACCEL_VALUE_SIZE),
                                .z = toSignedInt16(head += frame::IMU_ACCEL_VALUE_SIZE),
                        },
                .trans = {
                        .x = toSignedInt16(head += frame::IMU_ACCEL_VALUE_SIZE),
                        .y = toSignedInt16(head += frame::IMU_ACCEL_VALUE_SIZE),
                        .z = toSignedInt16(head += frame::IMU_ACCEL_VALUE_SIZE),
                }}};
}

template <> std::vector<frame::Encoder> Parser<frame::Encoder>::parsePayload(frame::Frame& fr)
{
        if (fr.payload.size() < 2 * sizeof(int16_t))
                return {};

        return {{
                .timestamp = fr.timestamp,
                .right     = toSignedInt16(fr.payload.data() + sizeof(int16_t)) / 64.0f,
                .left      = toSignedInt16(fr.payload.data()) / 64.0f,
        }};
}

} // namespace parser
