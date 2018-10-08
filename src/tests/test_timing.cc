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

    lib_timing::start_timer("stdout");
    run();
    lib_timing::start_timer("fileout");
    run();
    run();
    lib_timing::end_timer("stdout", std::cout);
    lib_timing::end_timer("fileout", ofs);

    ofs.close();

    return static_cast<int>(sum);
}
