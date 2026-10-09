#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>

#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <functional>

#include "utility/logger.hpp"
#include "application.hpp"

// double I(std::function<double(double)> f, std::vector<double> x)
// {

//         double result = 0;

//         for (size_t i = 1; i < x.size(); i++)
//                 result += (f(x[i - 1]) + f(x[i])) * (x[i] - x[i - 1]) / 2;

//         return result;
// }

// std::vector<double> getX(double bottom, double upper, double precision)
// {
//         std::vector<double> x = {0};

//         for (double i = bottom; i <= upper; i += precision)
//                 x.push_back(i);

//         return x;
// }

// std::function<double(double)> f = static_cast<double (*)(double)>(sin);

int main(int argc, char* argv[])
{
        if (argc != 2 || argv[1][0] == '\0') {
                std::println(stderr, "usage: {} <serial-device>", argv[0]);
                return 1;
        }

        std::string path = argv[1];

        logger::log(std::format("RECEIVED ARGS [{}]", path));

        auto app = application::init(path);
        if (!app) {
                logger::log(app.error());
                return 1;
        }

        auto result = (*app)->run();
        if (!result) {
                logger::log(result.error());
                return 1;
        }
}

// #include <iostream>
// #include <boost/crc.hpp>

// int main(void)
// {
//         typedef boost::crc_optimal<16, 0x8005, 0xFFFF, 0, true, true> modbus_crc;

//         char pdu[] = "123456789";

//         modbus_crc crc;
//         crc.process_bytes(pdu, 9);

//         std::cout << std::hex << crc.checksum() << std::endl;
//         return 0;
// }
