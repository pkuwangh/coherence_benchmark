#include <cstdlib>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "utils/lib_timing.hh"
#include "coherence/multiple_rdwr.hh"

// read-modify-write thread
void *thread_rmw(void *ptr)
{
    uint32_t bad_status = 0;
    ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    // warm-up
    warm_up(ptr, std::to_string(pkt->getThreadId()));
    // pointer chasing
    register char** p = NULL;
    register uint32_t k = 0;
    // loop iterations, loop partitions
    for (uint32_t i = 0; i < pkt->getNumIterations(); ++i) {
        for (uint32_t part_idx = 0; part_idx < pkt->getNumPartitions(); ++part_idx) {
            // locking
            pthread_mutex_lock(pkt->getFlowMutex(part_idx));
            // per-thread work timer
            pkt->startTimer();
            // real work
            const uint32_t& num_chases = pkt->getNumLines();
            p = pkt->getStartPoint(part_idx);
            for (k = 0; k < num_chases; ++k) {
                *(p + 4) += 1;
                p = (char**)(*p);
            }
            // stop timer
            pkt->endTimer();
            // unlocking
            pthread_mutex_unlock(pkt->getFlowMutex(part_idx));
            // return something
            bad_status += (p == NULL);
        }
    }
    pkt->setBadStatus(bad_status);
}

int main(int argc, char** argv)
{
    // input parameters
    if (argc != 9) {
        std::cout << "Usage: ./smt_rdwr"
            << " <region_size> <page_size> <stride> <pattern0> <pattern1>"
            << " <num_iterations0> <num_iterations1> <thread_step>" << std::endl;
        std::cout << "\tregion_size/page_size in KB" << std::endl;
        std::cout << "\tstride (spatial) in B" << std::endl;
        std::cout << "\tpattern: stride, pageRand, allRand" << std::endl;
        std::cout << "\tthread mapping step: e.g. 2 leads to 0,1,2,3 -> 0,2,1,3" << std::endl;
        exit(1);
    }
    const uint32_t region_size = atoi(argv[1]);
    const uint32_t page_size = atoi(argv[2]);
    const uint32_t stride = atoi(argv[3]);
    std::string pattern0 = argv[4];
    std::string pattern1 = argv[5];
    const uint32_t num_iterations0 = atoi(argv[6]);
    const uint32_t num_iterations1 = atoi(argv[7]);
    const uint32_t thread_step = atoi(argv[8]);
    // memory region setup
    MemSetup::Handle mem_setup0 = std::make_shared<MemSetup>(
            region_size, page_size, stride, pattern0,
            region_size, num_iterations0);
    MemSetup::Handle mem_setup1 = std::make_shared<MemSetup>(
            region_size, page_size, stride, pattern1,
            region_size, num_iterations1);
    // thread attrs
    const uint32_t num_cores = get_nprocs();
    const uint32_t num_threads = 2;
    utils::ThreadHelper<ThreadPacket> threads(num_threads, num_cores, thread_step);
    threads.getPacket(0).setMemSetup(mem_setup0);
    threads.getPacket(1).setMemSetup(mem_setup1);
    utils::start_timer("all");
    // create threads
    threads.setRoutine(thread_rmw, [](const uint32_t& idx) { return true; });
    threads.create();
    // wait all threads
    threads.join();
    utils::end_timer("all", std::cout);
    // check output & dump timers
    uint32_t status = 0;
    for (uint32_t i = 0; i < num_threads; ++i) {
        status += threads.getPacket(i).getBadStatus();
        threads.getPacket(i).dumpTimer(std::cout);
    }
    return status;
}
