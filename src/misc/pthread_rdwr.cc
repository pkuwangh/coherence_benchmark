#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"
#include "misc/pthread_rdwr.hh"

// global defined lock
pthread_mutex_t g_flow_mutex;
pthread_cond_t  g_flow_cond;
pthread_rwlock_t g_flow_rwlock;   // probably not necessary
uint32_t g_flow_step;

// thread function
void *writer_thread(void *ptr) {
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    std::string thread_sign = "thread " + std::to_string(pkt->getThreadId());
    // two timers
    std::string timer_full = thread_sign + " full";
    std::string timer_work = thread_sign + " work";
    // start full timer
    utils::start_timer(timer_full);
    // write lock
    pthread_mutex_lock(&g_flow_mutex);
    pthread_rwlock_wrlock(&g_flow_rwlock);
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
    // unlock
    ++ g_flow_step;
    pthread_cond_broadcast(&g_flow_cond);
    pthread_mutex_unlock(&g_flow_mutex);
    pthread_rwlock_unlock(&g_flow_rwlock);
    return NULL;
}

void *reader_thread(void *ptr) {
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    std::string thread_sign = "thread " + std::to_string(pkt->getThreadId());
    // two timers
    std::string timer_full = thread_sign + " full";
    std::string timer_work = thread_sign + " work";
    // start full timer
    utils::start_timer(timer_full);
    // read lock
    pthread_mutex_lock(&g_flow_mutex);
    while (g_flow_step == 0) {
        pthread_cond_wait(&g_flow_cond, &g_flow_mutex);
    }
    pthread_mutex_unlock(&g_flow_mutex);
    pthread_rwlock_rdlock(&g_flow_rwlock);
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
        utils::end_timer(timer_work, std::cout);
        utils::end_timer(timer_full, std::cout);
    }
    // unlock
    pthread_rwlock_unlock(&g_flow_rwlock);
    return NULL;
}

int main(int argc, char **argv)
{
    uint32_t thread_step = 1;
    if (argc > 1) {
        thread_step = atoi(argv[1]);
    }
    // init locks
    pthread_rwlock_init(&g_flow_rwlock, NULL);
    pthread_mutex_init(&g_flow_mutex, NULL);
    pthread_cond_init(&g_flow_cond, NULL);
    // thread attrs
    const uint32_t num_threads = get_nprocs();
    utils::ThreadHelper<ThreadPacket> threads(num_threads, num_threads, thread_step);
    // create threads
    threads.setRoutine(writer_thread, [](const uint32_t& idx) { return idx == 0; });
    threads.setRoutine(reader_thread, [](const uint32_t& idx) { return idx > 0; });
    threads.create();
    // wait threads
    threads.join();
    // destroy locks
    pthread_rwlock_destroy(&g_flow_rwlock);
    pthread_mutex_destroy(&g_flow_mutex);
    pthread_cond_destroy(&g_flow_cond);

    return 0;
}

