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
pthread_mutex_t g_flow_mutex;
pthread_cond_t  g_flow_cond;
uint32_t g_flow_step;

// thread function
void *thread_work(void *ptr) {
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    std::string thread_sign = "thread " + std::to_string(pkt->threadId);
    // two timers
    std::string timer_full = thread_sign + " full";
    std::string timer_work = thread_sign + " work";
    // start full timer
    pthread_mutex_lock(&g_timer_mutex);
    utils::start_timer(timer_full);
    pthread_mutex_unlock(&g_timer_mutex);
    // cond_wait w/ cond variable
    pthread_mutex_lock(&g_flow_mutex);
    while (g_flow_step != pkt->threadId) {
        pthread_cond_wait(&g_flow_cond, &g_flow_mutex);
    }
    // start work timer
    pthread_mutex_lock(&g_timer_mutex);
    utils::start_timer(timer_work);
    pthread_mutex_unlock(&g_timer_mutex);
    // real work
    const uint64_t loop_count = 20000000 * 1;
    double sum = 1;
    for (uint64_t i = 1; i <= loop_count; ++i) {
        if (i % 2 == 0) {
            sum *= static_cast<double>(i);
        } else {
            sum /= static_cast<double>(i);
        }
    }
    // end timer
    if (sum > 0) {
        utils::end_timer(timer_full, std::cout);
        utils::end_timer(timer_work, std::cout);
    }
    // cond_broadcast w/ cond variable
    ++ g_flow_step;
    pthread_cond_broadcast(&g_flow_cond);
    pthread_mutex_unlock(&g_flow_mutex);
}

int main(int argc, char **argv)
{
    // init locks
    pthread_mutex_init(&g_timer_mutex, NULL);
    pthread_mutex_init(&g_flow_mutex, NULL);
    pthread_cond_init(&g_flow_cond, NULL);
    g_flow_step = 0;
    // create threads
    const uint32_t num_threads = 16;
    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadPacket> packets(num_threads, 0);
    for (uint32_t i = 0; i < threads.size(); ++i) {
        packets[i] = i;
        pthread_create(&threads[i], NULL, thread_work, (void*)(&packets[i]));
    }
    // wait threads
    for (uint32_t i = 0; i < threads.size(); ++i) {
        pthread_join(threads[i], NULL);
    }
    // destroy locks
    pthread_mutex_destroy(&g_timer_mutex);
    pthread_mutex_destroy(&g_flow_mutex);
    pthread_cond_destroy(&g_flow_cond);

    return 0;
}