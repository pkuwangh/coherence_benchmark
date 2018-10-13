#ifndef __MULTIPLE_RDWR__
#define __MULTIPLE_RDWR__

#include <cstdint>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>

#include "utils/lib_mem_region.hh"
#include "utils/lib_threading.hh"

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
            mem_regions_[i] = std::make_shared<utils::MemRegion>(
                    1024*region_size, 1024*page_size, stride);
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
    std::vector<utils::MemRegion::Handle> mem_regions_;
    const uint32_t num_iterations_;

    friend class ThreadPacket;
};

class ThreadPacket: public utils::BaseThreadPacket {
  public:
    ThreadPacket() :
        BaseThreadPacket(),
        mem_setup_ (nullptr)
    { }
    ~ThreadPacket() = default;

    void setMemSetup(const MemSetup::Handle& ptr) { mem_setup_ = ptr; }

    uint32_t getNumIterations() const { return mem_setup_->num_iterations_; }
    uint32_t getNumPartitions() const { return mem_setup_->num_partitions_; }
    uint32_t getNumLines() const { return mem_setup_->mem_regions_[0]->numLines(); }
    char** getStartPoint(const uint32_t& part_idx) const {
        return mem_setup_->mem_regions_[part_idx]->getStartPoint();
    }

  private:
    MemSetup::Handle mem_setup_;
};

#endif
