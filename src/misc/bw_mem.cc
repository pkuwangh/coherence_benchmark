#include <iostream>
#include <string>
#include <cassert>
#include <cstdint>
#include <functional>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

void print_usage() {
    std::cout << "[./bw_mem] [size in KB] [action] [iteration] <core freq in GHz>" << std::endl;
    std::cout << "\tavailable action: prd, pwr, prmw, pcp, frd, fwr, frmw, fcp" << std::endl;
    std::cout << "Example: ./bw_mem 2048 prd 100 2.3" << std::endl;
}

int benchmark_prd(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
int benchmark_pwr(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
int benchmark_prmw(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
int benchmark_pcp(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
//int benchmark_frd(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
//int benchmark_fwr(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
//int benchmark_frmw(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);
//int benchmark_fcp(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter);

int main(int argc, char **argv)
{
    utils::start_timer("startup");
    if (argc < 4) {
        print_usage();
        return 1;
    }
    // get command line arguments
    uint64_t size = 1024 * static_cast<uint64_t>(atoi(argv[1]));
    const std::string action = argv[2];
    const uint64_t iteration = atoi(argv[3]);
    const float core_freq_ghz = argc > 4 ? atof(argv[4]) : 1.6;
    // action
    std::function<int(const utils::MemRegion::Handle&, uint64_t, uint64_t)> func;
    if (action == "prd") func = benchmark_prd;
    else if (action == "pwr") func = benchmark_pwr;
    else if (action == "prmw") func = benchmark_prmw;
    else if (action == "pcp") func = benchmark_pcp;
//    else if (action == "frd") func = benchmark_frd;
//    else if (action == "fwr") func = benchmark_fwr;
//    else if (action == "frmw") func = benchmark_frmw;
//    else if (action == "fcp") func = benchmark_fcp;
    else {
        print_usage();
        return 1;
    }
    std::string tag = "bw_mem_" + action;
    // setup memory region
    uint64_t region_size = size;
    if (action == "pcp" or action == "fcp") {
        region_size *= 2;
    }
    utils::MemRegion::Handle mem_region(new utils::MemRegion(region_size));
    // input check
    const uint64_t num_lines = mem_region->numLines();
    // input check
    static const uint64_t loop_size = 16 * 64;
    assert (size % loop_size == 0);
    const uint64_t unrolled_loop_count = size / loop_size;
    // run
    std::cout << "Memory region setup done; BW test begins ..." << std::endl;
    std::cout << "Total iterations: " << iteration << ", data size (KB) per iter: " << size << std::endl;
    utils::end_timer("startup", std::cout);
    int sum = 0;
    // warm-up one iteration
    sum |= func(mem_region, unrolled_loop_count, 1);
    // timer
    utils::start_timer(tag);
    sum |= func(mem_region, unrolled_loop_count, iteration);
    utils::end_timer(tag, std::cout, size, iteration, core_freq_ghz);
    return sum;
}

int benchmark_prd(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter) {
    register uint64_t* start = (uint64_t*)(mem_region->getStartPoint());
    register uint64_t* p = start;
    register uint64_t i = 0;
    register uint64_t sum = 0;
#define DOIT(i) p[i]+
    while (num_iter > 0) {
        p = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            sum +=
            DOIT(0)  DOIT(4)  DOIT(8)  DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28)
            DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) DOIT(60)
            DOIT(64) DOIT(68) DOIT(72) DOIT(76) DOIT(80) DOIT(84) DOIT(88) DOIT(92)
            DOIT(96) DOIT(100) DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) p[124];
            p += 128;
        }
    }
#undef DOIT
    return sum;
}

int benchmark_pwr(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter) {
    register uint64_t* start = (uint64_t*)(mem_region->getStartPoint());
    register uint64_t* p = start;
    register uint64_t i = 0;
#define DOIT(i) p[i] = 1;
    while (num_iter > 0) {
        p = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            DOIT(0)  DOIT(4)  DOIT(8)  DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28)
            DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) DOIT(60)
            DOIT(64) DOIT(68) DOIT(72) DOIT(76) DOIT(80) DOIT(84) DOIT(88) DOIT(92)
            DOIT(96) DOIT(100) DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) DOIT(124);
            p += 128;
        }
    }
#undef DOIT
    return 0;
}

int benchmark_prmw(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter) {
    register uint64_t* start = (uint64_t*)(mem_region->getStartPoint());
    register uint64_t* p = start;
    register uint64_t i = 0;
#define DOIT(i) p[i] += 1;
    while (num_iter > 0) {
        p = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            DOIT(0)  DOIT(4)  DOIT(8)  DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28)
            DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) DOIT(60)
            DOIT(64) DOIT(68) DOIT(72) DOIT(76) DOIT(80) DOIT(84) DOIT(88) DOIT(92)
            DOIT(96) DOIT(100) DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) DOIT(124);
            p += 128;
        }
    }
#undef DOIT
    return 0;
}

int benchmark_pcp(const utils::MemRegion::Handle& mem_region, uint64_t loop_count, uint64_t num_iter) {
    register uint64_t* src = (uint64_t*)(mem_region->getStartPoint());
    register uint64_t* dst = (uint64_t*)(mem_region->getHalfPoint());
    register uint64_t* p = src;
    register uint64_t* q = dst;
    register uint64_t i = 0;
#define DOIT(i) q[i] = p[i];
    while (num_iter > 0) {
        p = src;
        q = dst;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            DOIT(0)  DOIT(4)  DOIT(8)  DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28)
            DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) DOIT(60)
            DOIT(64) DOIT(68) DOIT(72) DOIT(76) DOIT(80) DOIT(84) DOIT(88) DOIT(92)
            DOIT(96) DOIT(100) DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) DOIT(124);
            p += 128;
            q += 128;
        }
    }
#undef DOIT
    return 0;
}

