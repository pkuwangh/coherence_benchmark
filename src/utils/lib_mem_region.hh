#ifndef __LIB_MEM_H__
#define __LIB_MEM_H__

#include <cstdint>
#include <memory>
#include <vector>

namespace utils {

class MemRegion {
  public:
    using Handle = std::shared_ptr<MemRegion>;

    MemRegion(uint32_t size, uint32_t pageSize, uint32_t lineSize);
    ~MemRegion() = default;

    // initialize to different patterns
    void stride_init();
    void page_random_init();
    void all_random_init();
    // helper
    void dump();
    uint32_t numLines() const { return numPages_ * numLinesInPage_; }
    // entry point
    char** getStartPoint() const { return (char**)base_; }
    char** getHalfPoint() const { return (char**)(base_ + size_/2); }

  private:
    void randomizeSequence_(std::vector<uint32_t>& sequence, uint32_t size, uint32_t unit);

    uint32_t size_;       // size of memory region in Bytes
    uint32_t pageSize_;   // probably hard-coded to 4KB
    uint32_t lineSize_;   // not necessarily the cacheline size; i.e. preferred spatial stride

    std::unique_ptr<char> addr_ = nullptr;  // raw pointer returned by new
    char*   base_ = NULL;   // page-aligned pointer

    uint32_t numPages_;
    uint32_t numLinesInPage_;
};

}

#endif
