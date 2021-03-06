#include <iostream>
#include <string>
#include <cassert>
#include <cstdint>
#include <functional>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

void print_usage() {
    std::cout << "[./bw_mem] [total size in KB] [action] [warmup iters] [main iters] [core freq] <region2 type> <region2 size> <active size in KB>" << std::endl;
    std::cout << "\tavailable action: prd, pwr, prmw, pcp, frd, fwr, frmw, fcp" << std::endl;
    std::cout << "\tregion2 type: native, remote, remote1, remote2, device" << std::endl;
    std::cout << "\tregion2 size: subset of total size, in KB" << std::endl;
    std::cout << "\tactive size: subset of total size, in KB" << std::endl;
    std::cout << "Example: ./bw_mem 4096 prd 10 100 2.3" << std::endl;
    std::cout << "Example: ./bw_mem 4096 prd 10 100 2.3 remote 2048" << std::endl;
    std::cout << "Example: ./bw_mem 4096 prd 10 100 2.3 device 2048" << std::endl;
    std::cout << "Example: ./bw_mem 4096 prd 10 100 2.3 native 0 2048" << std::endl;
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
    if (argc < 6) {
        print_usage();
        return 1;
    }
    // get command line arguments
    uint64_t size = 1024 * static_cast<uint64_t>(atoi(argv[1]));
    const std::string action = argv[2];
    const uint64_t warmup_iteration = atoi(argv[3]);
    const uint64_t main_iteration = atoi(argv[4]);
    const float core_freq_ghz = atof(argv[5]);
    bool use_hugepage = false;
    utils::MemType region2_type = utils::MemType::NATIVE;
    uint64_t region2_size = 0;
    if (argc >= 8) {
        const std::string r2_type_str = argv[6];
        if (r2_type_str == "remote" || r2_type_str == "Remote") {
            region2_type = utils::MemType::REMOTE1;
        } else if (r2_type_str == "remote1" || r2_type_str == "Remote1") {
            region2_type = utils::MemType::REMOTE1;
        } else if (r2_type_str == "remote2" || r2_type_str == "Remote2") {
            region2_type = utils::MemType::REMOTE2;
        } else if (r2_type_str == "device" || r2_type_str == "Device") {
            region2_type = utils::MemType::DEVICE;
        }
        region2_size = 1024 * static_cast<uint64_t>(atoi(argv[7]));
    }
    uint64_t active_size = size;
    if (argc >= 9) {
        active_size = 1024 * static_cast<uint64_t>(atoi(argv[8]));
    }
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
    uint64_t active_region_size = active_size;
    if (action == "pcp" or action == "fcp") {
        region_size *= 2;
        active_region_size *= 2;
    }
    const uint64_t page_size = 4096;
    const uint64_t line_size = 64;
    utils::MemRegion::Handle mem_region(
        new utils::MemRegion(
            region_size, active_region_size, page_size, line_size,
            use_hugepage, region2_type, region2_size));
    // input check
    const uint64_t num_lines = mem_region->numActiveLines();
    // input check
    static const uint64_t loop_size = 16 * 64;
    assert (active_size % loop_size == 0);
    const uint64_t unrolled_loop_count = active_size / loop_size;
    // run
    std::cout << "Memory region setup done; BW test begins ..." << std::endl;
    std::cout << "Total iterations: " << main_iteration << ", data size per iter: " << active_size << std::endl;
    utils::end_timer("startup", std::cout);
    int sum = 0;
    utils::start_timer("warmup");
    // warm-up some iterations
    sum |= func(mem_region, unrolled_loop_count, warmup_iteration);
    utils::end_timer("warmup", std::cout);
    // timer
    utils::start_timer(tag);
    sum |= func(mem_region, unrolled_loop_count, main_iteration);
    utils::end_timer(tag, std::cout, active_size, main_iteration, core_freq_ghz);
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
