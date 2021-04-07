#ifndef __MULTIPLE_RDWR__
#define __MULTIPLE_RDWR__

#include <cstdint>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <ostream>
#include <vector>
#include <pthread.h>

#include "utils/lib_mem_region.hh"
#include "utils/lib_timing.hh"
#include "utils/lib_threading.hh"

class MemRegionExt : public utils::MemRegion {
  public:
    using Handle = std::shared_ptr<MemRegionExt>;

    MemRegionExt(
        uint32_t region_size,
        uint32_t page_size,
        uint32_t line_size,
        bool use_hugepage,
        uint32_t num_partitions) :
        utils::MemRegion(region_size, page_size, line_size, use_hugepage)
    {
        flow_mutex.reset(new pthread_mutex_t);
        flow_cond.reset(new pthread_cond_t);
        flow_step.reset(new uint32_t);
        pthread_mutex_init(flow_mutex.get(), NULL);
        pthread_cond_init(flow_cond.get(), NULL);
        *flow_step = 0;
    }
    ~MemRegionExt() {
        pthread_mutex_destroy(flow_mutex.get());
        pthread_cond_destroy(flow_cond.get());
    }

    std::unique_ptr<pthread_mutex_t> flow_mutex;
    std::unique_ptr<pthread_cond_t>  flow_cond;
    std::unique_ptr<uint32_t>        flow_step;
};

class MemSetup {
  public:
    using Handle = std::shared_ptr<MemSetup>;

    MemSetup(
            uint32_t region_size,
            uint32_t page_size,
            uint32_t stride,
            std::string pattern,
            uint32_t partition_size,
            uint32_t num_iterations) :
        region_size_ (region_size),
        partition_size_ (partition_size),
        num_partitions_ (region_size / partition_size),
        mem_regions_ (num_partitions_, nullptr),
        num_iterations_ (num_iterations)
    {
        for (uint32_t i = 0; i < mem_regions_.size(); ++i) {
            const bool use_hugepage = false;
            mem_regions_[i] = std::make_shared<MemRegionExt>(
                1024*partition_size, 1024*page_size, stride, use_hugepage,
                num_partitions_);
            if (pattern == "stride") {
                mem_regions_[i]->stride_init();
            } else if (pattern == "pageRand") {
                mem_regions_[i]->page_random_init();
            } else if (pattern == "allRand") {
                mem_regions_[i]->all_random_init();
            } else {
                std::cerr << "unknown pattern: " << pattern << std::endl;
                exit(1);
            }
        }
    }
    ~MemSetup() = default;

  private:
    const uint32_t region_size_;
    const uint32_t partition_size_;
    const uint32_t num_partitions_;
    std::vector<MemRegionExt::Handle> mem_regions_;
    const uint32_t num_iterations_;

    friend class ThreadPacket;
};

class ThreadPacket: public utils::BaseThreadPacket {
  public:
    ThreadPacket() :
        BaseThreadPacket(),
        mem_setup_ (nullptr),
        bad_status_ (false),
        read_only_ (false),
        timer_enabled_ (false)
    { }
    ~ThreadPacket() = default;

    void setMemSetup(const MemSetup::Handle& ptr) { mem_setup_ = ptr; }

    uint32_t getNumIterations() const { return mem_setup_->num_iterations_; }
    uint32_t getNumPartitions() const { return mem_setup_->num_partitions_; }
    uint32_t getNumLines() const { return mem_setup_->mem_regions_[0]->numLines(); }
    char** getStartPoint(const uint32_t& part_idx) const {
        return mem_setup_->mem_regions_[part_idx]->getStartPoint();
    }

    pthread_mutex_t* getFlowMutex (const uint32_t& part_idx) { return mem_setup_->mem_regions_[part_idx]->flow_mutex.get(); }
    pthread_cond_t*  getFlowCond  (const uint32_t& part_idx) { return mem_setup_->mem_regions_[part_idx]->flow_cond.get(); }
    const uint32_t&  getFlowStep  (const uint32_t& part_idx) const { return *(mem_setup_->mem_regions_[part_idx]->flow_step); }
    void incrFlowStep(const uint32_t& part_idx) {
        *(mem_setup_->mem_regions_[part_idx]->flow_step) = (*(mem_setup_->mem_regions_[part_idx]->flow_step) + 1) % getNumThreads();
    }

    void setBadStatus(uint32_t v) { bad_status_ = v; }
    uint32_t getBadStatus() const { return bad_status_; }

    void startTimer() { timer_.startTimer(); }
    void endTimer() { timer_.endTimer(); }
    void dumpTimer(std::ostream& os) {
        std::string out_str = "timer <" + getSignature() + "> elapsed:" +
            " total(s)=" + std::to_string(timer_.getElapsedTime()) +
            " per-ref(ns)=" + std::to_string(timer_.getElapsedTime()*1e9/getNumLines()/(getNumIterations()-1)) + "\n";
        os << out_str;
    }

    // pattern
    void setReadOnly(bool v) { read_only_ = v; }
    const bool& isReadOnly() const { return read_only_; }
    // timer control
    void setTimerEnabled(bool v = true) { timer_enabled_ = v; }
    const bool& isTimerEnabled() const { return timer_enabled_; }

  private:
    MemSetup::Handle mem_setup_;
    uint32_t bad_status_;
    utils::Timer timer_;
    bool read_only_ = false;
    bool timer_enabled_ = false;
};

bool warm_up(void *ptr, std::string post_fix = "")
{
    const ThreadPacket* pkt = static_cast<ThreadPacket*>(ptr);
    register char** p = NULL;
    register uint32_t k = 0;
    // start timer
    utils::start_timer("warmup"+post_fix);
    // read-in memory regions
    for (uint32_t part_idx = 0; part_idx < pkt->getNumPartitions(); ++part_idx) {
        const uint32_t& num_chases = pkt->getNumLines();
        p = pkt->getStartPoint(part_idx);
        for (k = 0; k < num_chases; ++k) {
            p = (char**)(*p);
        }
    }
    // stop timer
    utils::end_timer("warmup"+post_fix, std::cout);
    return (p != NULL);
}

#endif
