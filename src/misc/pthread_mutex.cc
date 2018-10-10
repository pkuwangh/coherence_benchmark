#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

class ThreadPacket {
  public:
    ThreadPacket(uint32_t tid) :
        threadId (tid)
    { }
    ~ThreadPacket() = default;

    uint32_t threadId;
};

// global defined locks
pthread_mutex_t g_timer_mutex;

void *thread_work(void *ptr) {
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    std::string thread_id = "thread " + std::to_string(pkt->threadId);
    // we have a timer pool; adding to the pool needs thread safety
    pthread_mutex_lock(&g_timer_mutex);
    const uint64_t loop_count = 20000000 * (1 + pkt->threadId);
    pthread_mutex_unlock(&g_timer_mutex);

    double sum = 1;
    for (uint64_t i = 1; i <= loop_count; ++i) {
        if (i % 2 == 0) {
            sum *= static_cast<double>(i);
        } else {
            sum /= static_cast<double>(i);
        }
    }
    if (sum > 0) {
        utils::end_timer(thread_id, std::cout);
    }
}

int main(int argc, char **argv)
{
    // init locks
    pthread_mutex_init(&g_timer_mutex, NULL);
    // create threads
    const uint32_t num_threads = 16;
    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadPacket> packets(num_threads, 0);
    for (uint32_t i = 0; i < threads.size(); ++i) {
        packets[i] = i;
        utils::start_timer("thread " + std::to_string(packets[i].threadId));
        pthread_create(&threads[i], NULL, thread_work, (void*)(&packets[i]));
    }

    for (uint32_t i = 0; i < threads.size(); ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
