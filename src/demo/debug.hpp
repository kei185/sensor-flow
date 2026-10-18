#pragma once

#include "core/frame.hpp"
#include "core/xqueue.hpp"

#include <filesystem>
#include <stop_token>

namespace demo
{

void runImuDebug(
        std::stop_token,
        xqueue::Queue<frame::Imu>&,
        const std::filesystem::path& path = "debug/imu.csv");
// timestampはミリ秒、ジャイロと加速度は補正前の生値としてCSVへ追記する。

} // namespace demo
