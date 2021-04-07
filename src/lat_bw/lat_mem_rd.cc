#include <iostream>
#include <string>
#include <cassert>
#include <cstdint>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

void print_usage() {
    std::cout << "[./lat_mem_rd] [size in KB] [page in KB] [stride in B] [pattern] [warmup iteration] [main iteration] <core freq in GHz>"
              << std::endl;
    std::cout << "\tavailable patterns: stride, pageRand, allRand" << std::endl;
    std::cout << "Example: ./lat_mem_rd 2048 4 64 pageRand 10 100 2.3" << std::endl;
}

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint64_t loop_count, uint64_t num_iter);

int main(int argc, char **argv)
{
    utils::start_timer("startup");
    if (argc < 7) {
        print_usage();
        return 1;
    }
    // get command line arguments
    const uint64_t size = 1024 * static_cast<uint64_t>(atoi(argv[1]));
    const uint64_t page = 1024 * static_cast<uint64_t>(atoi(argv[2]));
    const uint64_t stride = static_cast<uint64_t>(atoi(argv[3]));
    const std::string pattern = argv[4];
    const uint64_t warmup_iteration = atoi(argv[5]);
    const uint64_t main_iteration = atoi(argv[6]);
    const float core_freq_ghz = argc > 7 ? atof(argv[7]) : 1.6;
    std::string tag = "lat_mem_rd_" + pattern;
    // setup memory region
    const bool use_hugepage = false;
    utils::MemRegion::Handle mem_region(
        new utils::MemRegion(size, page, stride, use_hugepage));
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
    static const uint64_t loop_unroll = 256;
    const uint64_t num_chases = mem_region->numLines();
    assert (num_chases % loop_unroll == 0);
    const uint64_t unrolled_loop_count = num_chases / loop_unroll;
    // run
    std::cout << "Memory region setup done; Pointer-Chasing begins ..." << std::endl;
    std::cout << "Total iterations: " << main_iteration << ", # of pointer chases per iter: " << num_chases << std::endl;
    utils::end_timer("startup", std::cout);
    bool error = false;
    utils::start_timer("warmup");
    // warm-up some iterations
    error |= benchmark_loads(mem_region, unrolled_loop_count, warmup_iteration);
    utils::end_timer("warmup", std::cout);
    // timer
    utils::start_timer(tag);
    error |= benchmark_loads(mem_region, unrolled_loop_count, main_iteration);
    utils::end_timer(tag, std::cout, num_chases * main_iteration, core_freq_ghz);
    return error;
}

#define LOOP1     p1 = (char **)*p1;
#define LOOP4     LOOP1 LOOP1 LOOP1 LOOP1
#define LOOP16    LOOP4 LOOP4 LOOP4 LOOP4
#define LOOP64    LOOP16 LOOP16 LOOP16 LOOP16
#define LOOP256   LOOP64 LOOP64 LOOP64 LOOP64

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint64_t loop_count, uint64_t num_iter)
{
    register char **start = mem_region->getStartPoint();
    register char **p1 = start;
    register uint64_t i = 0;
    while (num_iter > 0) {
        p1 = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            LOOP256;
        }
    }
    return (p1 == NULL);
}
