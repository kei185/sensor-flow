// @generated
#include "demo/gnuplot.hpp"
#include "utility/logger.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <numbers>
#include <string>
#include <utility>

namespace demo
{
namespace
{
constexpr size_t LIDAR_MAX_POINTS        = 720;
constexpr size_t IMU_MAX_POINTS          = 1000;
constexpr double IMU_TIME_WINDOW_SECONDS = 10.0;
constexpr auto   REDRAW_INTERVAL         = std::chrono::milliseconds(50);

struct RedrawTimer
{
        std::mutex                            mutex;
        std::condition_variable_any           condition;
        std::chrono::steady_clock::time_point nextRedraw = std::chrono::steady_clock::now();

        bool wait(std::stop_token st)
        {
                std::unique_lock lock(this->mutex);
                this->condition.wait_until(lock, st, this->nextRedraw, [] { return false; });
                if (st.stop_requested())
                        return false;
                this->nextRedraw = std::chrono::steady_clock::now() + REDRAW_INTERVAL;
                return true;
        }
};

template <typename T, typename Consume>
bool readPlotBatch(std::stop_token st, xqueue::Queue<T>& queue, RedrawTimer& timer, Consume consume)
{
        if (!queue.waitData(st) || !timer.wait(st))
                return false;

        do {
                if (st.stop_requested())
                        return false;
                consume(queue.pop());
        } while (!queue.empty() && std::chrono::steady_clock::now() < timer.nextRedraw);

        return !st.stop_requested();
}

void closeGnuplot(FILE* file) noexcept
{
        std::fputs("exit\n", file);
        pclose(file);
}

auto openGnuplot()
{
        std::unique_ptr<FILE, decltype(&closeGnuplot)> process(
                popen("gnuplot -persist", "w"),
                closeGnuplot);
        if (!process)
                logger::log("OPEN PROCESS gnuplot FAILED");
        return process;
}

constexpr double IMU_SAMPLE_RATE_HZ           = 15.0;
constexpr double GYRO_SENSITIVITY_DPS_PER_LSB = 4.375 / 1000.0;
// The sensor firmware leaves CTRL6 at its reset default: +/-125 dps.
constexpr double ACCELEROMETER_SENSITIVITY_G_PER_LSB    = 0.061 / 1000.0;
constexpr size_t GYRO_CALIBRATION_SAMPLES               = 30;
constexpr double GYRO_CALIBRATION_MAX_DPS               = 3.0;
constexpr double GYRO_CALIBRATION_MAX_RANGE_DPS         = 0.5;
constexpr double GYRO_CALIBRATION_ACCELERATION_CHANGE_G = 0.02;
constexpr double GYRO_NOISE_THRESHOLD_DPS               = 0.1;
constexpr double GRAVITY_TOLERANCE_G                    = 0.15;
constexpr double MAX_SAMPLE_PERIOD                      = 0.5;
constexpr double ACCELEROMETER_CORRECTION               = 0.02;
// Use accelerometer tilt as a small correction to limit integrated gyro drift.
constexpr double PLANE_HALF_LENGTH           = 1.2;
constexpr double PLANE_HALF_WIDTH            = 0.8;
constexpr double PLANE_FORWARD_MARKER_LENGTH = 1.5;
constexpr double DEGREES_PER_RADIAN          = 180.0 / std::numbers::pi;

struct Orientation
{
        double                roll            = 0.0;
        double                pitch           = 0.0;
        double                yaw             = 0.0;
        bool                  tiltInitialized = false;
        std::array<double, 3> gyroBias{};
        std::array<double, 3> gyroSum{};
        std::array<double, 3> gyroMin{};
        std::array<double, 3> gyroMax{};
        frame::Acceleration   calibrationAcceleration{};
        size_t                calibrationSamples  = 0;
        size_t                calibrationAttempts = 0;
        bool                  gyroCalibrated      = false;
        uint64_t              previousTimestamp   = 0;
};

struct Point3d
{
        double x;
        double y;
        double z;
};

struct ImuSample
{
        double     time;
        frame::Imu value;
};

std::string toString(const std::pair<double, double>& point)
{
        return std::format("{} {}", point.first, point.second);
}

std::string toString(frame::Imu sample)
{
        return std::format(
                "{} {} {} {} {} {}",
                sample.trans.x,
                sample.trans.y,
                sample.trans.z,
                sample.rot.x,
                sample.rot.y,
                sample.rot.z);
}

double wrapDegrees(double angle) { return std::remainder(angle, 360.0); }

double blendAngle(double predicted, double measured)
{
        const double difference = std::remainder(measured - predicted, 360.0);
        return wrapDegrees(predicted + ACCELEROMETER_CORRECTION * difference);
}

void calibrateGyroscope(Orientation& orientation, const frame::Imu& sample, double accelerationG)
{
        const std::array<double, 3> rates = {
                sample.rot.x * GYRO_SENSITIVITY_DPS_PER_LSB,
                sample.rot.y * GYRO_SENSITIVITY_DPS_PER_LSB,
                sample.rot.z * GYRO_SENSITIVITY_DPS_PER_LSB,
        };
        ++orientation.calibrationAttempts;
        bool stable = std::abs(accelerationG - 1.0) <= GRAVITY_TOLERANCE_G;
        for (double rate : rates)
                stable = stable && std::abs(rate) <= GYRO_CALIBRATION_MAX_DPS;

        if (orientation.calibrationSamples == 0) {
                orientation.gyroMin = orientation.gyroMax = rates;
                orientation.gyroSum                       = {};
                orientation.calibrationAcceleration       = sample.trans;
        }

        const auto&  reference          = orientation.calibrationAcceleration;
        const double accelerationChange = std::hypot(
                                                  double(sample.trans.x) - reference.x,
                                                  double(sample.trans.y) - reference.y,
                                                  double(sample.trans.z) - reference.z) *
                                          ACCELEROMETER_SENSITIVITY_G_PER_LSB;
        stable = stable && accelerationChange <= GYRO_CALIBRATION_ACCELERATION_CHANGE_G;
        for (size_t i = 0; i < rates.size(); ++i) {
                orientation.gyroMin[i] = std::min(orientation.gyroMin[i], rates[i]);
                orientation.gyroMax[i] = std::max(orientation.gyroMax[i], rates[i]);
                stable = stable && orientation.gyroMax[i] - orientation.gyroMin[i] <=
                                           GYRO_CALIBRATION_MAX_RANGE_DPS;
        }

        if (!stable) {
                orientation.calibrationSamples = 0;
        } else {
                for (size_t i = 0; i < rates.size(); ++i)
                        orientation.gyroSum[i] += rates[i];
                if (++orientation.calibrationSamples == GYRO_CALIBRATION_SAMPLES) {
                        for (size_t i = 0; i < rates.size(); ++i)
                                orientation.gyroBias[i] =
                                        orientation.gyroSum[i] / GYRO_CALIBRATION_SAMPLES;
                        orientation.gyroCalibrated = true;
                        logger::log(
                                std::format(
                                        "IMU gyro bias [deg/s]: X {:.4f}, Y {:.4f}, Z {:.4f}",
                                        orientation.gyroBias[0],
                                        orientation.gyroBias[1],
                                        orientation.gyroBias[2]));
                }
        }

        if (!orientation.gyroCalibrated &&
            orientation.calibrationAttempts % GYRO_CALIBRATION_SAMPLES == 0)
                logger::log(
                        std::format(
                                "IMU waiting for still samples: gyro [{:.3f}, {:.3f}, {:.3f}] "
                                "deg/s, acceleration {:.3f} g",
                                rates[0],
                                rates[1],
                                rates[2],
                                accelerationG));
}

void updateOrientation(Orientation& orientation, const frame::Imu& sample)
{
        const double ax             = sample.trans.x;
        const double ay             = sample.trans.y;
        const double az             = sample.trans.z;
        const double verticalLength = std::hypot(ay, az);
        const double totalLength    = std::hypot(ax, verticalLength);
        const double accelerationG  = totalLength * ACCELEROMETER_SENSITIVITY_G_PER_LSB;
        const bool   gravityValid   = std::abs(accelerationG - 1.0) <= GRAVITY_TOLERANCE_G;

        if (!orientation.gyroCalibrated) {
                if (gravityValid) {
                        orientation.roll  = std::atan2(ay, az) * DEGREES_PER_RADIAN;
                        orientation.pitch = std::atan2(-ax, verticalLength) * DEGREES_PER_RADIAN;
                        orientation.tiltInitialized = true;
                }
                calibrateGyroscope(orientation, sample, accelerationG);
                orientation.previousTimestamp = sample.timestamp;
                return;
        }
        // 起動直後は静止サンプルの平均を取り、ジャイロのゼロ点を求める。

        double samplePeriod = 1.0 / IMU_SAMPLE_RATE_HZ;
        if (orientation.previousTimestamp != 0 && sample.timestamp != 0) {
                const auto previousTimestamp  = orientation.previousTimestamp;
                orientation.previousTimestamp = sample.timestamp;
                if (sample.timestamp <= previousTimestamp)
                        return;
                samplePeriod = (sample.timestamp - previousTimestamp) / 1000.0;
                if (samplePeriod > MAX_SAMPLE_PERIOD)
                        return;
        }
        orientation.previousTimestamp = sample.timestamp;
        // 時刻の逆行や長い欠落を、そのまま大きな回転として積分しない。

        const auto correctedRate = [](int16_t raw, double bias) {
                const double rate = raw * GYRO_SENSITIVITY_DPS_PER_LSB - bias;
                return std::abs(rate) < GYRO_NOISE_THRESHOLD_DPS ? 0.0 : rate;
        };
        const double predictedRoll =
                orientation.roll +
                correctedRate(sample.rot.x, orientation.gyroBias[0]) * samplePeriod;
        const double predictedPitch =
                orientation.pitch +
                correctedRate(sample.rot.y, orientation.gyroBias[1]) * samplePeriod;
        orientation.yaw = wrapDegrees(
                orientation.yaw +
                correctedRate(sample.rot.z, orientation.gyroBias[2]) * samplePeriod);

        if (!gravityValid) {
                orientation.roll  = wrapDegrees(predictedRoll);
                orientation.pitch = wrapDegrees(predictedPitch);
                return;
        }

        const double measuredRoll  = std::atan2(ay, az) * DEGREES_PER_RADIAN;
        const double measuredPitch = std::atan2(-ax, verticalLength) * DEGREES_PER_RADIAN;

        if (!orientation.tiltInitialized) {
                orientation.roll            = measuredRoll;
                orientation.pitch           = measuredPitch;
                orientation.tiltInitialized = true;
                return;
        }

        orientation.roll  = blendAngle(predictedRoll, measuredRoll);
        orientation.pitch = blendAngle(predictedPitch, measuredPitch);
}

Point3d rotate(Point3d point, const Orientation& orientation)
{
        const double roll  = orientation.roll / DEGREES_PER_RADIAN;
        const double pitch = orientation.pitch / DEGREES_PER_RADIAN;
        const double yaw   = orientation.yaw / DEGREES_PER_RADIAN;

        const double cr = std::cos(roll);
        const double sr = std::sin(roll);
        const double cp = std::cos(pitch);
        const double sp = std::sin(pitch);
        const double cy = std::cos(yaw);
        const double sy = std::sin(yaw);

        return {
                .x = cy * cp * point.x + (cy * sp * sr - sy * cr) * point.y +
                     (cy * sp * cr + sy * sr) * point.z,
                .y = sy * cp * point.x + (sy * sp * sr + cy * cr) * point.y +
                     (sy * sp * cr - cy * sr) * point.z,
                .z = -sp * point.x + cp * sr * point.y + cp * cr * point.z,
        };
}

void configureOrientationPlot(FILE* file)
{
        std::fputs(
                "set title 'IMU orientation'\n"
                "set xlabel 'X'\n"
                "set ylabel 'Y'\n"
                "set zlabel 'Z'\n"
                "set xrange [-1.7:1.7]\n"
                "set yrange [-1.7:1.7]\n"
                "set zrange [-1.7:1.7]\n"
                "set view 60, 30\n"
                "set view equal xyz\n"
                "set xyplane at 0\n"
                "set grid\n"
                "unset key\n"
                "unset colorbox\n"
                "set pm3d depthorder border linecolor rgb '#00749A'\n",
                file);
        std::fflush(file);
}

void drawOrientation(FILE* file, const Orientation& orientation)
{
        const std::array<Point3d, 4> plane = {
                rotate({-PLANE_HALF_LENGTH, -PLANE_HALF_WIDTH, 0.0}, orientation),
                rotate({PLANE_HALF_LENGTH, -PLANE_HALF_WIDTH, 0.0}, orientation),
                rotate({-PLANE_HALF_LENGTH, PLANE_HALF_WIDTH, 0.0}, orientation),
                rotate({PLANE_HALF_LENGTH, PLANE_HALF_WIDTH, 0.0}, orientation),
        };
        const Point3d forward = rotate({PLANE_FORWARD_MARKER_LENGTH, 0.0, 0.0}, orientation);

        std::fputs("$plane << EOD\n", file);
        for (size_t row = 0; row < 2; ++row) {
                for (size_t column = 0; column < 2; ++column) {
                        const auto& point = plane[row * 2 + column];
                        std::fprintf(file, "%f %f %f\n", point.x, point.y, point.z);
                }
                std::fputc('\n', file);
        }
        std::fputs("EOD\n$forward << EOD\n0 0 0\n", file);
        std::fprintf(file, "%f %f %f\nEOD\n", forward.x, forward.y, forward.z);
        std::fprintf(
                file,
                "set title 'IMU orientation%s: roll %.1f, pitch %.1f, yaw %.1f deg'\n"
                "splot $plane using 1:2:3 with pm3d fillcolor rgb '#00AEEF', "
                "$forward using 1:2:3 with lines linewidth 4 linecolor rgb '#F28E2B'\n",
                orientation.gyroCalibrated ? "" : " (calibrating - keep still)",
                orientation.roll,
                orientation.pitch,
                orientation.yaw);
        std::fflush(file);
}

void configureImuTimeSeriesPlot(FILE* file)
{
        std::fputs(
                "set title 'IMU time series'\n"
                "set xlabel 'Elapsed time [s]'\n"
                "set format x '%.2f'\n"
                "set ylabel 'Translation raw value'\n"
                "set y2label 'Rotation raw value'\n"
                "set ytics nomirror\n"
                "set y2tics\n"
                "set grid\n"
                "set key outside top center horizontal maxrows 2\n"
                "set lmargin 10\n"
                "set rmargin 10\n"
                "set style data linespoints\n"
                "set style line 1 linecolor rgb '#00AEEF' linewidth 1.5 pointtype 7 pointsize 0.3\n"
                "set style line 2 linecolor rgb '#F28E2B' linewidth 1.5 pointtype 7 pointsize 0.3\n"
                "set style line 3 linecolor rgb '#59A14F' linewidth 1.5 pointtype 7 pointsize "
                "0.3\n"
                "set style line 4 linecolor rgb '#00AEEF' linewidth 1.5 dashtype 2\n"
                "set style line 5 linecolor rgb '#F28E2B' linewidth 1.5 dashtype 2\n"
                "set style line 6 linecolor rgb '#59A14F' linewidth 1.5 dashtype 2\n",
                file);
}
} // namespace

void run(std::stop_token st, xqueue::Queue<frame::LidarPoint>& inQueue)
{
        if (st.stop_requested())
                return;

        auto process = openGnuplot();
        if (!process)
                return;
        FILE* file = process.get();

        std::fputs(
                "set title 'LiDAR scan'\n"
                "set polar\n"
                "set angles degrees\n"
                "set theta top clockwise\n"
                "set size square\n"
                "set grid polar 30\n"
                "set rrange [0:*]\n"
                "unset key\n"
                "set style line 1 linecolor rgb '#00AEEF' pointtype 7 pointsize 0.5\n",
                file);

        std::deque<std::pair<double, double>> points;
        RedrawTimer                           timer;

        while (readPlotBatch(st, inQueue, timer, [&](const frame::LidarPoint& scan) {
                for (size_t i = 0; i < scan.point.size(); ++i) {
                        const double angle = std::fmod(scan.angleStart + scan.angleDiff * i, 360.0);
                        points.emplace_back(angle, scan.point[i]);
                        if (points.size() > LIDAR_MAX_POINTS)
                                points.pop_front();
                }
        })) {
                std::fputs("plot '-' using 1:2 with points linestyle 1\n", file);
                for (const auto& point : points)
                        std::fprintf(file, "%s\n", toString(point).c_str());
                std::fputs("e\n", file);
                std::fflush(file);
        }
}

void runImuTranslation(std::stop_token st, xqueue::Queue<frame::Imu>& inQueue)
{
        if (st.stop_requested())
                return;

        auto process = openGnuplot();
        if (!process)
                return;
        FILE* file = process.get();

        configureImuTimeSeriesPlot(file);
        std::fputs(
                "set title 'IMU translation (acceleration)'\n"
                "set ylabel 'Acceleration raw value'\n"
                "unset y2label\n"
                "unset y2tics\n",
                file);

        std::deque<ImuSample> points;
        const auto            start = std::chrono::steady_clock::now();
        RedrawTimer           timer;

        while (readPlotBatch(st, inQueue, timer, [&](frame::Imu value) {
                const double time =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                                .count();
                points.push_back({time, value});
                // 直近10秒を残し、古いサンプルを捨てる。
                while (points.size() > IMU_MAX_POINTS ||
                       points.front().time < time - IMU_TIME_WINDOW_SECONDS)
                        points.pop_front();
        })) {
                auto minimum = points.front().value.trans.x;
                auto maximum = minimum;

                std::fputs("$imu << EOD\n", file);
                for (const auto& point : points) {
                        const auto& trans = point.value.trans;
                        minimum           = std::min({minimum, trans.x, trans.y, trans.z});
                        maximum           = std::max({maximum, trans.x, trans.y, trans.z});
                        std::fprintf(
                                file,
                                "%.6f %d %d %d\n",
                                point.time,
                                trans.x,
                                trans.y,
                                trans.z);
                }
                std::fputs("EOD\n", file);

                // 一定値や全軸0でも表示できるよう、上下に余白を設ける。
                const double padding = std::max(1.0, (maximum - minimum) * 0.05);
                // 起動直後も時間軸を10秒幅に保ち、10秒を超えたら横へ流す。
                const double end = std::max(points.back().time, IMU_TIME_WINDOW_SECONDS);
                std::fprintf(
                        file,
                        "set xrange [%.6f:%.6f]\n"
                        "set yrange [%f:%f]\n"
                        "plot $imu using 1:2 title 'Translation X' linestyle 1, "
                        "$imu using 1:3 title 'Translation Y' linestyle 2, "
                        "$imu using 1:4 title 'Translation Z' linestyle 3\n",
                        end - IMU_TIME_WINDOW_SECONDS,
                        end,
                        minimum - padding,
                        maximum + padding);
                std::fflush(file);
        }
}

void runImuTimeSeries(std::stop_token st, xqueue::Queue<frame::Imu>& inQueue)
{
        if (st.stop_requested())
                return;

        auto process = openGnuplot();
        if (!process)
                return;
        FILE* file = process.get();

        configureImuTimeSeriesPlot(file);

        std::deque<ImuSample> points;
        const auto            start = std::chrono::steady_clock::now();
        RedrawTimer           timer;

        while (readPlotBatch(st, inQueue, timer, [&](frame::Imu value) {
                const double time =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                                .count();
                // Use elapsed host time for the plot's horizontal axis.
                points.push_back({time, value});
                if (points.size() > IMU_MAX_POINTS)
                        points.pop_front();
        })) {
                auto transMin = points.front().value.trans.x;
                auto transMax = transMin;
                auto rotMin   = points.front().value.rot.x;
                auto rotMax   = rotMin;

                std::fputs("$imu << EOD\n", file);
                for (const auto& point : points) {
                        const auto& t = point.value.trans;
                        const auto& r = point.value.rot;
                        transMin      = std::min({transMin, t.x, t.y, t.z});
                        transMax      = std::max({transMax, t.x, t.y, t.z});
                        rotMin        = std::min({rotMin, r.x, r.y, r.z});
                        rotMax        = std::max({rotMax, r.x, r.y, r.z});
                        std::fprintf(file, "%.6f %s\n", point.time, toString(point.value).c_str());
                }
                std::fputs("EOD\n", file);

                const double transPadding = std::max(1.0, (transMax - transMin) * 0.05);
                const double rotPadding   = std::max(1.0, (rotMax - rotMin) * 0.05);
                // Keep flat signals visible and avoid an empty range, including all-zero data.
                std::fprintf(
                        file,
                        "set xrange [%.6f:%.6f]\n",
                        points.front().time,
                        std::max(points.back().time, points.front().time + 0.05));
                std::fprintf(
                        file,
                        "set yrange [%f:%f]\n"
                        "set y2range [%f:%f]\n"
                        "plot $imu using 1:2 axes x1y1 title 'Translation X' linestyle 1, "
                        "$imu using 1:3 axes x1y1 title 'Translation Y' linestyle 2, "
                        "$imu using 1:4 axes x1y1 title 'Translation Z' linestyle 3, "
                        "$imu using 1:5 axes x1y2 title 'Rotation X' linestyle 4, "
                        "$imu using 1:6 axes x1y2 title 'Rotation Y' linestyle 5, "
                        "$imu using 1:7 axes x1y2 title 'Rotation Z' linestyle 6\n",
                        transMin - transPadding,
                        transMax + transPadding,
                        rotMin - rotPadding,
                        rotMax + rotPadding);
                // The Qt terminal draws multiplot panels one at a time, which exposes
                // a blank canvas during live redraws. Two Y axes keep both scales in
                // one plot so each update is presented as a single frame.
                std::fflush(file);
        }
}

void runImuOrientation(std::stop_token st, xqueue::Queue<frame::Imu>& inQueue)
{
        if (st.stop_requested())
                return;

        auto process = openGnuplot();
        if (!process)
                return;
        FILE* file = process.get();

        configureOrientationPlot(file);
        logger::log("IMU: keep the sensor still for about 2 seconds to calibrate the gyroscope");

        Orientation orientation;
        RedrawTimer timer;

        while (readPlotBatch(st, inQueue, timer, [&](frame::Imu value) {
                updateOrientation(orientation, value);
        })) {
                drawOrientation(file, orientation);
        }
}

} // namespace demo
