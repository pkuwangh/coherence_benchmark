#ifndef __MULTIPLE_RDWR__
#define __MULTIPLE_RDWR__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "utils/lib_mem_region.hh"
#include "utils/lib_threading.hh"

class ThreadPacket: public utils::BaseThreadPacket {
  public:
    ThreadPacket(
            uint32_t size,
            uint32_t page_size,
            uint32_t stride,
            std::string pattern,
            uint32_t num_partitions,
            uint32_t num_iterations) :
        region_size_ (size),
        num_partitions_ (num_partitions),
        partition_size_ (size / num_partitions),
        mem_regions_ (num_partitions, nullptr),
        num_iterations_ (num_iterations)
    {
        for (uint32_t i = 0; i < mem_regions_.size(); ++i) {
            mem_regions_[i].reset(std::make_shared<utils::MemRegion>(1024*size, 1024*page_size, stride));
            if (pattern == "stride") {
                mem_regions_[i]->stride_init();
            } else if (pattern == "pageRand") {
                mem_region_[i]->page_random_init();
            } else if (pattern == "allRand") {
                mem_region_[i]->all_random_init();
            } else {
                std::cerr << "unknown pattern: " << pattern << std::endl;
                exit(1);
            }
        }
        signature_ = "thread-" + std::to_string(getThreadId()) + (getThreadId() < 10 ? " " : "");
    }
    ~ThreadPacket() = default;

    uint32_t getNumPartitions() const { return num_partitions_; }
    uint32_t getNumLines() const { return mem_regions_[0]->numLines(); }
    char** getStartPoint(const uint32_t& part_idx) const { return mem_regions_[part_idx]->getStartPoint(); }
    uint32_t getNumIterations() const { return num_iterations_; }
    const std::string& getSignature() const { return signature_; }

  private:
    const uint32_t region_size_;
    const uint32_t num_partitions_;
    const uint32_t partition_size_;
    std::vector<utils::MemRegion::Handle> mem_regions_;
    const uint32_t num_iterations_;
    std::string signature_;
};

#endif
