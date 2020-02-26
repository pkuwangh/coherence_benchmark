#ifndef __LIB_MEM_H__
#define __LIB_MEM_H__

#include <cstdint>
#include <memory>
#include <vector>

namespace utils {

class MemRegion {
  public:
    using Handle = std::shared_ptr<MemRegion>;

    MemRegion(uint64_t size, uint64_t page_size=4096, uint64_t line_size=64);
    virtual ~MemRegion() = default;

    // initialize to different patterns
    void stride_init();
    void page_random_init();
    void all_random_init();
    // helper
    void dump();
    uint64_t numLines() const { return num_pages_ * num_lines_in_page_; }
    // entry point
    char** getStartPoint() const { return (char**)base_; }
    char** getHalfPoint() const { return (char**)(base_ + size_/2); }
    char** getFirstQuarterPoint() const { return (char**)(base_ + size_/4); }
    char** getThirdQuarterPoint() const { return (char**)(base_ + size_/4*3); }

  private:
    void randomizeSequence_(std::vector<uint64_t>& sequence, uint64_t size, uint64_t unit);

    uint64_t size_;       // size of memory region in Bytes
    uint64_t page_size_;   // probably hard-coded to 4KB
    uint64_t line_size_;   // not necessarily the cacheline size; i.e. preferred spatial stride

    std::unique_ptr<char> addr_ = nullptr;  // raw pointer returned by new
    char*   base_ = NULL;   // page-aligned pointer

    uint64_t num_pages_;
    uint64_t num_lines_in_page_;
};

}

#endif
