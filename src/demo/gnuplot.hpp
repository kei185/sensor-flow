// @generated
#pragma once

#include "core/frame.hpp"
#include "core/xqueue.hpp"

#include <stop_token>

namespace demo
{

void run(std::stop_token, xqueue::Queue<frame::LidarPoint>&);
// IMUデモは同じキューを消費するので、1つだけ起動する。
void runImuTranslation(std::stop_token, xqueue::Queue<frame::Imu>&);
void runImuTimeSeries(std::stop_token, xqueue::Queue<frame::Imu>&);
void runImuOrientation(std::stop_token, xqueue::Queue<frame::Imu>&);

} // namespace demo
