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
    // pointer chasing
    register char** p = NULL;
    register uint32_t k = 0;
    // loop iterations, loop partitions
    for (uint32_t i = 0; i < pkt->getNumIterations(); ++i) {
        for (uint32_t part_idx = 0; part_idx < pkt->getNumPartitions(); ++part_idx) {
            // locking
            pthread_mutex_lock(pkt->getFlowMutex(part_idx));
            while (pkt->getFlowStep(part_idx) != pkt->getThreadId()) {
                pthread_cond_wait(pkt->getFlowCond(part_idx), pkt->getFlowMutex(part_idx));
            }
            // per-thread work timer
            if (i > 0) {
                pkt->startTimer();
            }
            // real work
            const uint32_t& num_chases = pkt->getNumLines();
            p = pkt->getStartPoint(part_idx);
            if (pkt->isReadOnly()) {
                for (k = 0; k < num_chases; ++k) {
                    p = (char**)(*p);
                }
            } else {
                for (k = 0; k < num_chases; ++k) {
                    *(p + 4) += 1;
                    p = (char**)(*p);
                }
            }
            // stop timer
            if (i > 0) {
                pkt->endTimer();
            }
            // unlocking
            pkt->incrFlowStep(part_idx);
            pthread_cond_broadcast(pkt->getFlowCond(part_idx));
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
    if (argc < 9) {
        std::cout << "Usage: ./multiple_rdwr"
            << " <region_size> <page_size> <stride> <pattern>"
            << " <partition_size> <num_iterations>"
            << " <num threads> <thread mapping step> <core-id start>" << std::endl;
        std::cout << "\tregion_size/page_size/partition_size in KB" << std::endl;
        std::cout << "\tstride (spatial) in B" << std::endl;
        std::cout << "\tpattern: stride, pageRand, allRand" << std::endl;
        std::cout << "\tthread mapping step: e.g. 2 leads to 0,1,2,3 -> 0,2,1,3" << std::endl;
        std::cout << "\t1st iteration will be warm-up" << std::endl;
        exit(1);
    }
    const uint32_t region_size = atoi(argv[1]);
    const uint32_t page_size = atoi(argv[2]);
    const uint32_t stride = atoi(argv[3]);
    std::string pattern = argv[4];
    const uint32_t partition_size = atoi(argv[5]);
    const uint32_t num_iterations = atoi(argv[6]);
    const uint32_t num_threads_user = atoi(argv[7]);
    const uint32_t thread_step = atoi(argv[8]);
    uint32_t core_id_start = 0;
    if (argc >= 10) core_id_start = atoi(argv[9]);
    // memory region setup
    MemSetup::Handle mem_setup = std::make_shared<MemSetup>(
            region_size, page_size, stride, pattern,
            partition_size, num_iterations);
    // thread attrs
    const uint32_t num_cores = get_nprocs();
    const uint32_t num_threads = (num_threads_user > 0) ? num_threads_user : num_cores;
    utils::ThreadHelper<ThreadPacket> threads(num_threads, num_cores, thread_step, core_id_start);
    for (uint32_t i = 0; i < num_threads; ++i) {
        threads.getPacket(i).setMemSetup(mem_setup);
        //if (i % 2 == 1) {
        //    threads.getPacket(i).setReadOnly(true);
        //}
    }
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
