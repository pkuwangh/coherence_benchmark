#include <cstdint>
#include <iostream>
#include <fstream>

#include "utils/lib_timing.hh"

int main()
{
    const uint64_t loop_count = 10000000;
    double sum = 1;

    auto run = [&sum, &loop_count]() -> void {
        for (uint64_t i = 1; i <= loop_count; ++i) {
            if (i % 2 == 0) {
                sum *= static_cast<double>(i);
            } else {
                sum /= static_cast<double>(i);
            }
        }
    };

    std::ofstream ofs;
    ofs.open("output_lib_timing");

    utils::start_timer("stdout");
    run();
    utils::start_timer("fileout");
    run();
    run();
    utils::end_timer("stdout", std::cout);
    utils::end_timer("fileout", ofs);

    ofs.close();

    return static_cast<int>(sum);
}
