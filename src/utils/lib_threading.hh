#ifndef __LIB_THREADING_HH__
#define __LIB_THREADING_HH__

#include <iostream>
#include <vector>
#include <pthread.h>

namespace utils {

class BaseThreadPacket {
  public:
    BaseThreadPacket() { }
    virtual ~BaseThreadPacket() = default;

    void setThreadId(uint32_t tid) { thread_id_ = tid; }
    uint32_t getThreadId() const { return thread_id_; }

  private:
    uint32_t thread_id_;
};

template <class Packet>
class ThreadHelper {
  public:
    ThreadHelper(uint32_t num_threads, uint32_t thread_step) :
        num_threads_ (num_threads),
        thread_step_ (thread_step),
        attrs_ (num_threads),
        threads_ (num_threads),
        packets_ (num_threads)
    {
        if (num_threads % thread_step > 0) {
            std::cout << "expect num_threads=" << num_threads << " to be a multiple of thread_step=" << thread_step << std::endl;
            exit(1);
        }
        const uint32_t group_size = num_threads / thread_step;
        // prepare thread attrs
        for (uint32_t i = 0; i < num_threads; ++i) {
            // get thread-core mapping
            const uint32_t group_id = i / group_size;
            const uint32_t group_offset = i % group_size;
            const uint32_t core_id = group_id + group_offset * thread_step;
            std::cout << "thread " << i << " -> core" << core_id << std::endl;
            pthread_attr_init(&attrs_[i]);
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            // set thread attribute
            pthread_attr_setaffinity_np(&attrs_[i], sizeof(cpu_set_t), &cpuset);
            // thread packets
            packets_[i].setThreadId(i);
        }
    }
    ~ThreadHelper() = default;

    Packet& getPacket(const uint32_t& i) { return packets_[i]; }

    void create(void *(*start_routine)(void *)) {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            pthread_create(&threads_[i], &attrs_[i], start_routine, (void*)(&packets_[i]));
        }
    }
    void join() {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            pthread_join(threads_[i], NULL);
        }
    }

  private:
    const uint32_t num_threads_;
    const uint32_t thread_step_;

    std::vector<pthread_t> threads_;
    std::vector<pthread_attr_t> attrs_;
    std::vector<Packet> packets_;
};

}

#endif
