#ifndef __LIB_THREADING_HH__
#define __LIB_THREADING_HH__

#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <pthread.h>

namespace utils {

class BaseThreadPacket {
  public:
    BaseThreadPacket() { }
    virtual ~BaseThreadPacket() = default;

    void setThreadId(uint32_t tid, uint32_t core_id, uint32_t num) {
        thread_id_ = tid;
        core_id_ = core_id;
        num_threads_ = num;
        signature_ = "T" + std::to_string(tid) + "/C" + std::to_string(core_id);
    }
    const uint32_t& getThreadId() const { return thread_id_; }
    const uint32_t& getNumThreads() const { return num_threads_; }
    const std::string& getSignature() const { return signature_; }

  private:
    uint32_t thread_id_;
    uint32_t core_id_;
    uint32_t num_threads_;
    std::string signature_;
};

template <class Packet>
class ThreadHelper {
  public:
    ThreadHelper(uint32_t num_threads, uint32_t num_cores, uint32_t thread_step, uint32_t core_id_start=0) :
        num_threads_ (num_threads),
        thread_step_ (thread_step),
        attrs_ (num_threads),
        threads_ (num_threads),
        packets_ (num_threads),
        start_routines_ (num_threads, nullptr)
    {
        if (num_cores % thread_step > 0) {
            std::cerr << "expect num_cores=" << num_threads << " to be a multiple of thread_step=" << thread_step << std::endl;
            exit(1);
        }
        const uint32_t group_size = num_cores / thread_step;
        // prepare thread attrs
        std::cout << "thread ID: [";
        for (uint32_t i = 0; i < num_threads; ++i) {
            std::cout << i;
            if (num_threads > 100 && i < 100) std::cout << " ";
            if (i < 10) std::cout << " ";
            if (i < num_threads-1) std::cout << " ";
        }
        std::cout << "]\n core  ID: [";
        for (uint32_t i = 0; i < num_threads; ++i) {
            // get thread-core mapping
            const uint32_t group_id = i / group_size;
            const uint32_t group_offset = i % group_size;
            const uint32_t core_id = (core_id_start + group_id + group_offset * thread_step) % num_cores;
            std::cout << core_id;
            if (num_threads > 100 && core_id < 100) std::cout << " ";
            if (core_id < 10) std::cout << " ";
            if (i < num_threads-1) std::cout << " ";
            pthread_attr_init(&attrs_[i]);
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            // set thread attribute
            pthread_attr_setaffinity_np(&attrs_[i], sizeof(cpu_set_t), &cpuset);
            // thread packets
            packets_[i].setThreadId(i, core_id, num_threads);
        }
        std::cout << "]" << std::endl;
    }
    ~ThreadHelper() = default;

    Packet& getPacket(const uint32_t& idx) { return packets_[idx]; }
    void* getPacketPtr(const uint32_t& idx) { return (void*)(&packets_[idx]); }

    template <class UnaryPredicate>
    void setRoutine(void *(*start_routine)(void *), UnaryPredicate pred) {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            if (pred(i)) {
                start_routines_[i] = start_routine;
            }
        }
    }

    void create() {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            pthread_create(&threads_[i], &attrs_[i], start_routines_[i], (void*)(&packets_[i]));
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
    std::vector<void *(*)(void *)> start_routines_;
};

}

#endif
