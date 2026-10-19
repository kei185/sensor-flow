#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

struct Cartesian
{
        double x;
        double y;
        double z;
};

constexpr double translation(double v_l, double v_r) { return (v_l + v_r) / 2.0; }
constexpr double rotation(double v_l, double v_r, double r) { return (v_r - v_l) / r; }
constexpr std::pair<double, double>
toVelocityRatio(double v_t, double v_o, double r, double MAX_VELOCITY)
{
        const double left  = v_t - r * v_o / 2.0;
        const double right = v_t + r * v_o / 2.0;

        return {left / MAX_VELOCITY, right / MAX_VELOCITY};
}

constexpr auto minQ6 = std::numeric_limits<int16_t>::min();
constexpr auto maxQ6 = std::numeric_limits<int16_t>::max();
constexpr bool isValidSignedQ6(float f)
{

        return std::isfinite(f) && f >= minQ6 && f <= maxQ6; //
}

constexpr std::optional<uint16_t> toUnsignedQ6(float f)
{
        const float left = std::round(f * 64.0f);

        if (isValidSignedQ6(f))
                return static_cast<uint16_t>(static_cast<int16_t>(left));

        return std::nullopt;
}
