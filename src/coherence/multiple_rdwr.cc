#include <iostream>
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
        utils::start_timer(pkt->getSignature() + " p" + std::to_string(part_idx));

        // real work
        const uint32_t& num_chases = pkt->getNumPartitions();
        p = pkt->getStartPoint(part_idx);
        for (k = 0; k < num_chases; ++k) {
            p = (char**)(*p);
        }
    }
}
