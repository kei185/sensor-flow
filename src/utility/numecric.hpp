#pragma once

struct Cartesian
{
        double x;
        double y;
        double z;
};

constexpr double translation(double v_l, double v_r) { return (v_l + v_r) / 2.0; }

constexpr double rotation(double v_l, double v_r, double r) { return (v_r - v_l) / r; }
