#ifndef __LIB_MEM_H__
#define __LIB_MEM_H__

#include <cstdint>
#include <memory>
#include <vector>

namespace utils {

enum class MemType : char {
  NATIVE='N',
  REMOTE='R',
  DEVICE='D',
};


class MemRegion {
  public:
    using Handle = std::shared_ptr<MemRegion>;

    MemRegion(
      uint64_t size,
      uint64_t active_size,
      uint64_t page_size,
      uint64_t line_size,
      bool use_hugepage=false,
      MemType mem_type_region2=MemType::NATIVE,
      uint64_t size_region2=0);
    virtual ~MemRegion();

    // initialize to different patterns
    void stride_init();
    void page_random_init();
    void all_random_init();
    // helper
    void dump();
    uint64_t numAllLines() const { return num_all_pages_ * num_lines_in_page_; }
    uint64_t numActiveLines() const { return num_active_pages_ * num_lines_in_page_; }
    // entry point
    char** getStartPoint() const { return (char**)getOffsetAddr_(0); }
    char** getHalfPoint() const { return (char**)getOffsetAddr_(active_size_ / 2); }

  private:
    void error_(std::string message);
    char* allocNative_(const uint64_t& size, char*& raw_addr);
    char* allocRemote_(const uint64_t& size, char*& raw_addr, uint64_t& raw_size);
    char* allocDevice_(const uint64_t& size);
    void randomizeSequence_(
        std::vector<uint64_t>& sequence,
        uint64_t size,
        uint64_t unit,
        bool in_order=false);
    char* getOffsetAddr_(uint64_t offset) const;

    uint64_t size_;         // size of memory region in Bytes
    uint64_t active_size_;  // active size of memory region in Bytes
    uint64_t page_size_;    // not meant to be OS page size; better to be multiple of OS page
    uint64_t line_size_;    // not necessarily the cacheline size; i.e. preferred spatial stride
    bool use_hugepage_ = false;
    MemType mem_type_region2_ = MemType::NATIVE;
    uint64_t size_region1_;
    uint64_t size_region2_;

    int      fd_ = -1;
    char*    addr1_ = NULL;
    char*    addr2_ = NULL;
    char*    raw_addr1_ = NULL;
    char*    raw_addr2_ = NULL;
    uint64_t raw_size2_ = 0;

    uint64_t num_all_pages_;
    uint64_t num_active_pages_;
    uint64_t num_lines_in_page_;
};

}

#endif
