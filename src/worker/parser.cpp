#include "worker/parser.hpp"
#include "core/frame.hpp"
#include "utility/toInt.hpp"

#include <cstdint>
#include <vector>

namespace parser
{

template <>
std::vector<frame::systemMessage> Parser<frame::systemMessage>::parsePayload(frame::Frame& fr)
{

        return {{
                .timestamp = fr.receivedAt,
                .message   = std::string(fr.payload.begin(), fr.payload.end()),
                .type      = fr.type,
        }};
}

template <> std::vector<frame::LidarPoint> Parser<frame::LidarPoint>::parsePayload(frame::Frame& fr)
{
        std::vector<frame::LidarPoint> points = {};

        uint16_t angle_q6;
        uint16_t dist;

        for (size_t offset = 0; offset + frame::LIDAR_POINT_SIZE <= fr.payload.size();
             offset += frame::LIDAR_POINT_SIZE) {

                dist     = toInt16(fr.payload.data() + offset);
                angle_q6 = toInt16(fr.payload.data() + offset + sizeof(uint16_t));

                points.push_back(
                        {
                                .dist  = dist,
                                .angle = static_cast<float>(angle_q6) / 64.0f,
                        });
        }

        return points;
}

template <> std::vector<frame::Imu> Parser<frame::Imu>::parsePayload(frame::Frame& fr)
{
        uint8_t* head = fr.payload.data();

        return {(frame::Imu){
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

// TODO
template <> std::vector<frame::Encoder> Parser<frame::Encoder>::parsePayload(frame::Frame& fr)
{
        return {};
}

} // namespace parser
