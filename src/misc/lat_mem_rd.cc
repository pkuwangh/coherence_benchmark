#include <iostream>
#include <string>
#include <cassert>
#include <cstdint>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

void print_usage() {
    std::cout << "[./lat_mem_rd] [size in KB] [page in KB] [stride in B] [pattern] [iteration]" << std::endl;
    std::cout << "\tavailable patterns: stride, pageRand, allRand" << std::endl;
    std::cout << "Example: ./lat_mem_rd 2048 4 64 pageRand 100" << std::endl;
}

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint32_t loop_count, uint32_t num_iter);

int main(int argc, char **argv)
{
    if (argc != 6) {
        print_usage();
        return 1;
    }
    // get command line arguments
    const uint32_t size = 1024 * atoi(argv[1]);
    const uint32_t page = 1024 * atoi(argv[2]);
    const uint32_t stride = atoi(argv[3]);
    const std::string pattern = argv[4];
    const uint32_t iteration = atoi(argv[5]);
    // setup memory region
    utils::MemRegion::Handle mem_region(new utils::MemRegion(size, page, stride));
    if (pattern == "stride") {
        mem_region->stride_init();
    } else if (pattern == "pageRand") {
        mem_region->page_random_init();
    } else if (pattern == "allRand") {
        mem_region->all_random_init();
    } else {
        print_usage();
        return 1;
    }
    // input check
    static const uint32_t loop_unroll = 256;
    const uint32_t num_chases = mem_region->numLines();
    assert (num_chases % loop_unroll == 0);
    const uint32_t unrolled_loop_count = num_chases / loop_unroll;
    // run
    std::cout << "Memory region setup done; Pointer-Chasing begins ..." << std::endl;
    std::cout << "Total iterations: " << iteration << ", # of pointer chases per iter: " << num_chases << std::endl;
    // warm-up one iteration
    bool error = benchmark_loads(mem_region, unrolled_loop_count, 1);
    // timer
    utils::start_timer("lat_mem_rd");
    error = benchmark_loads(mem_region, unrolled_loop_count, iteration);
    utils::end_timer("lat_mem_rd", std::cout);
    return error;
}

#define LOOP1     p = (char **)*p;
#define LOOP4     LOOP1 LOOP1 LOOP1 LOOP1
#define LOOP16    LOOP4 LOOP4 LOOP4 LOOP4
#define LOOP64    LOOP16 LOOP16 LOOP16 LOOP16
#define LOOP256   LOOP64 LOOP64 LOOP64 LOOP64

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint32_t loop_count, uint32_t num_iter)
{
    register char **start = mem_region->getStartPoint();
    register char **p = start;
    register uint32_t i = 0;
    while (num_iter > 0) {
        p = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            LOOP256;
        }
    }
    return (p == NULL);
}
