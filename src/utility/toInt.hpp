#pragma once
#include <bit>
#include <cstdint>

constexpr uint32_t toInt32(const uint8_t* p)
{
        return (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[1]) << 8) | static_cast<uint32_t>(p[0]);
}

constexpr uint64_t toInt64(const uint8_t* p)
{
        return (static_cast<uint64_t>(toInt32(p + sizeof(uint32_t))) << 32) |
               static_cast<uint64_t>(toInt32(p));
}

constexpr uint16_t toInt16(const uint8_t* p)
{
        return (static_cast<uint16_t>(p[1]) << 8) | static_cast<uint16_t>(p[0]);
}

constexpr int16_t toSignedInt16(const uint8_t* p) { return std::bit_cast<int16_t>(toInt16(p)); }
