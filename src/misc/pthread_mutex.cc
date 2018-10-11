#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"

class ThreadPacket {
  public:
    ThreadPacket(uint32_t tid) :
        thread_id (tid)
    { }
    ~ThreadPacket() = default;

    uint32_t thread_id;
};

// global defined locks
pthread_mutex_t g_flow_mutex;
pthread_cond_t  g_flow_cond;
uint32_t g_flow_step;

// thread function
void *thread_work(void *ptr) {
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    std::string thread_sign = "thread " + std::to_string(pkt->thread_id);
    // two timers
    std::string timer_full = thread_sign + " full";
    std::string timer_work = thread_sign + " work";
    // start full timer
    utils::start_timer(timer_full);
    // cond_wait w/ cond variable
    pthread_mutex_lock(&g_flow_mutex);
    while (g_flow_step != pkt->thread_id) {
        pthread_cond_wait(&g_flow_cond, &g_flow_mutex);
    }
    // start work timer
    utils::start_timer(timer_work);
    // real work
    const uint64_t loop_count = 20000000 * 20;
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
    uint32_t thread_step = 1;
    if (argc > 1) {
        thread_step = atoi(argv[1]);
    }
    // init locks
    pthread_mutex_init(&g_flow_mutex, NULL);
    pthread_cond_init(&g_flow_cond, NULL);
    g_flow_step = 0;
    // prepare thread attrs, packets
    const uint32_t num_threads = get_nprocs();
    if (num_threads % thread_step > 0) {
        std::cout << "expect num_threads=" << num_threads << " is a multiple of thread_step=" << thread_step << std::endl;
        exit(1);
    }
    const uint32_t group_size = num_threads / thread_step;
    std::vector<pthread_attr_t> attrs(num_threads);
    std::vector<ThreadPacket> packets(num_threads, 0);
    for (uint32_t i = 0; i < num_threads; ++i) {
        // get thread - core mapping
        const uint32_t group_id = i / group_size;
        const uint32_t group_offset = i % group_size;
        const uint32_t core_id = group_id + group_offset * thread_step;
        //std::cout << "thread " << i << " -> core " << core_id << std::endl;
        // set thread attribute
        pthread_attr_init(&attrs[i]);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_attr_setaffinity_np(&attrs[i], sizeof(cpu_set_t), &cpuset);
        // set packet passed to thread
        packets[i] = i;
    }
    // create threads
    std::vector<pthread_t> threads(num_threads);
    for (uint32_t i = 0; i < threads.size(); ++i) {
        pthread_create(&threads[i], &attrs[i], thread_work, (void*)(&packets[i]));
    }
    // wait threads
    for (uint32_t i = 0; i < threads.size(); ++i) {
        pthread_join(threads[i], NULL);
    }
    // destroy locks
    pthread_mutex_destroy(&g_flow_mutex);
    pthread_cond_destroy(&g_flow_cond);

    return 0;
}
