#include "demo/debug.hpp"
#include "utility/logger.hpp"

#include <format>
#include <fstream>
#include <system_error>

namespace demo
{

void runImuDebug(
        std::stop_token              st,
        xqueue::Queue<frame::Imu>&   inQueue,
        const std::filesystem::path& path)
{
        if (st.stop_requested())
                return;

        if (!path.parent_path().empty()) {
                std::error_code error;
                std::filesystem::create_directories(path.parent_path(), error);
                if (error) {
                        logger::log(
                                std::format(
                                        "CREATE IMU debug directory FAILED: {}",
                                        error.message()));
                        return;
                }
        }

        std::ofstream file(path, std::ios::app);
        if (!file) {
                logger::log(std::format("OPEN IMU debug file FAILED: {}", path.string()));
                return;
        }

        if (file.tellp() == std::streampos(0))
                file << "timestamp_ms,gyro_x_raw,gyro_y_raw,gyro_z_raw,"
                        "accel_x_raw,accel_y_raw,accel_z_raw\n";
        file.flush();
        if (!file) {
                logger::log(std::format("WRITE IMU debug file FAILED: {}", path.string()));
                return;
        }
        logger::log(std::format("IMU debug CSV (append): {}", path.string()));

        while (inQueue.waitData(st) && !st.stop_requested()) {
                const auto sample = inQueue.pop();
                file << sample.timestamp << ',' << sample.rot.x << ',' << sample.rot.y << ','
                     << sample.rot.z << ',' << sample.trans.x << ',' << sample.trans.y << ','
                     << sample.trans.z << '\n';
                file.flush();
                // 1サンプルごとに書き出して、実行中もtailなどで確認できるようにする。
                if (!file) {
                        logger::log(std::format("WRITE IMU debug file FAILED: {}", path.string()));
                        return;
                }
        }
}

} // namespace demo
