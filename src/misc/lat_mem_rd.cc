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

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint64_t loop_count, uint64_t num_iter);

int main(int argc, char **argv)
{
    if (argc != 6) {
        print_usage();
        return 1;
    }
    // get command line arguments
    const uint64_t size = 1024 * static_cast<uint64_t>(atoi(argv[1]));
    const uint64_t page = 1024 * static_cast<uint64_t>(atoi(argv[2]));
    const uint64_t stride = static_cast<uint64_t>(atoi(argv[3]));
    const std::string pattern = argv[4];
    const uint64_t iteration = atoi(argv[5]);
    // setup memory region
    utils::MemRegion::Handle mem_region(new utils::MemRegion(size, page, stride));
    if (pattern == "stride") {
        mem_region->stride_init();
    } else if (pattern == "pageRand") {
        mem_region->page_random_init();
    } else if (pattern == "allRand") {
        mem_region->all_random_init();
    } else if (pattern == "allRandOffset") {
        mem_region->all_random_offset_init();
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
    std::cout << "Total iterations: " << iteration << ", # of pointer chases per iter: " << num_chases << std::endl;
    bool error = false;
    // warm-up one iteration
    error |= benchmark_loads(mem_region, unrolled_loop_count, 1);
    // timer
    utils::start_timer("lat_mem_rd");
    error |= benchmark_loads(mem_region, unrolled_loop_count, iteration);
    utils::end_timer("lat_mem_rd", std::cout);
    return error;
}

//#define LOOP1     p1 = (char **)*p1, p2 = (char **)*p2, p3 = (char **)*p3, p4 = (char **)*p4;
#define LOOP1     p1 = (char **)*p1, p2 = (char **)*p2;
//#define LOOP1     p1 = (char **)*p1;
#define LOOP4     LOOP1 LOOP1 LOOP1 LOOP1
#define LOOP16    LOOP4 LOOP4 LOOP4 LOOP4
#define LOOP64    LOOP16 LOOP16 LOOP16 LOOP16
#define LOOP256   LOOP64 LOOP64 LOOP64 LOOP64

bool benchmark_loads(const utils::MemRegion::Handle &mem_region, uint64_t loop_count, uint64_t num_iter)
{
    register char **start = mem_region->getStartPoint();
    register char **p1 = start;
    register char **half = mem_region->getHalfPoint();
    register char **p2 = half;
    //register char **q1 = mem_region->getFirstQuarterPoint();
    //register char **q3 = mem_region->getThirdQuarterPoint();
    //register char **p3 = q1;
    //register char **p4 = q3;
    register uint64_t i = 0;
    //num_iter /= 4;
    //num_iter /= 2;
    while (num_iter > 0) {
        p1 = start;
        p2 = half;
        //p3 = q1;
        //p4 = q3;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            LOOP256;
        }
    }
    //return (p1 == NULL);
    return (p1 == NULL && p2 == NULL);
}
