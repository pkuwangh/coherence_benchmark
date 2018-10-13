#include <cstdlib>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "utils/lib_timing.hh"
#include "coherence/multiple_rdwr.hh"

// global dfined lock
std::vector<pthread_mutex_t>  g_flow_mutex;
std::vector<pthread_cond_t>   g_flow_cond;
std::vector<uint32_t>         g_flow_step;

// read-modify-write thread
void *thread_work(void *ptr)
{
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);

    register char** p = NULL;
    register uint32_t k = 0;

    for (uint32_t part_idx = 0; part_idx < pkt->getNumPartitions(); ++part_idx) {
        // locking
        pthread_mutex_lock(&g_flow_mutex[part_idx]);
        while (g_flow_step[part_idx] != pkt->getThreadId()) {
            pthread_cond_wait(&g_flow_cond[part_idx], &g_flow_mutex[part_idx]);
        }
        // per-thread work timer
        const std::string timer_key = "T" + std::to_string(pkt->getThreadId()) +
            " p" + std::to_string(part_idx);
        utils::start_timer(timer_key);
        // real work
        const uint32_t& num_chases = pkt->getNumLines();
        p = pkt->getStartPoint(part_idx);
        for (k = 0; k < num_chases; ++k) {
            if (k < 4) {
                std::stringstream ss;
                ss << "T" << pkt->getThreadId() << " p" << part_idx
                    << " addr=[" << std::hex << "0x" << reinterpret_cast<uint64_t>(p)
                    << "]" << std::dec << "\n";
                std::cout << ss.str();
            }
            p = (char**)(*p);
        }
        // stop timer
        if (p != NULL) {
            utils::end_timer(timer_key, std::cout);
        }
        // unlocking
        ++ g_flow_step[part_idx];
        pthread_cond_broadcast(&g_flow_cond[part_idx]);
        pthread_mutex_unlock(&g_flow_mutex[part_idx]);
    }
}

int main(int argc, char** argv)
{
    // input parameters
    if (argc != 8) {
        std::cout << "Usage: ./multiple_rdwr.cc"
            << " <region_size> <page_size> <stride> <pattern>"
            << " <partition_size> <num_iterations> <thread mapping step>" << std::endl;
        std::cout << "region_size/page_size/partition_size in KB" << std::endl;
        std::cout << "stride (spatial) in B" << std::endl;
        std::cout << "pattern: stride, pageRand, allRand" << std::endl;
        std::cout << "thread mapping step: e.g. 2 leads to 0,1,2,3 -> 0,2,1,3" << std::endl;
        exit(1);
    }
    const uint32_t region_size = atoi(argv[1]);
    const uint32_t page_size = atoi(argv[2]);
    const uint32_t stride = atoi(argv[3]);
    std::string pattern = argv[4];
    const uint32_t partition_size = atoi(argv[5]);
    const uint32_t num_iterations = atoi(argv[6]);
    const uint32_t thread_step = atoi(argv[7]);
    // memory region setup
    MemSetup::Handle mem_setup = std::make_shared<MemSetup>(
            region_size, page_size, stride, pattern,
            partition_size, num_iterations);
    // init locks
    const uint32_t num_partitions = region_size / partition_size;
    g_flow_mutex.resize(num_partitions);
    g_flow_cond.resize(num_partitions);
    g_flow_step.resize(num_partitions, 0);
    for (uint32_t i = 0; i < num_partitions; ++i) {
        pthread_mutex_init(&g_flow_mutex[i], NULL);
        pthread_cond_init(&g_flow_cond[i], NULL);
    }
    // thread attrs
    const uint32_t num_threads = get_nprocs();
    utils::ThreadHelper<ThreadPacket> threads(num_threads, thread_step);
    for (uint32_t i = 0; i < num_threads; ++i) {
        threads.getPacket(i).setMemSetup(mem_setup);
    }
    // create threads
    threads.create(thread_work, 0, num_threads);
    // wait all threads
    threads.join();
    // destroy locks
    for (uint32_t i = 0; i < num_partitions; ++i) {
        pthread_mutex_destroy(&g_flow_mutex[i]);
        pthread_cond_destroy(&g_flow_cond[i]);
    }
    return 0;
}
