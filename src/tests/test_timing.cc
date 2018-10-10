#include <cstdint>
#include <iostream>
#include <fstream>

#include "utils/lib_timing.hh"

int main()
{
    const uint64_t loop_count = 50000000;
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

    utils::start_timer("stdout1");
    run();
    utils::start_timer("stdout2");
    utils::start_timer("fileout");
    run();
    run();
    utils::end_timer("stdout2", std::cout);
    run();
    run();
    utils::end_timer("stdout1", std::cout);
    utils::end_timer("fileout", ofs);

    ofs.close();

    return static_cast<int>(sum);
}
