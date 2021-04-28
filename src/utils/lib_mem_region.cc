#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "utils/lib_mem_region.hh"

namespace utils {

MemRegion::MemRegion(
        uint64_t size,
        uint64_t page_size,
        uint64_t line_size,
        bool use_hugepage,
        MemType mem_type_region2,
        uint64_t size_region2) :
    size_ (size),
    page_size_ (page_size),
    line_size_ (line_size),
    mem_type_region2_ (mem_type_region2),
    size_region2_ (size_region2),
    num_pages_ (size_ / page_size_),
    num_lines_in_page_ (page_size_ / line_size_),
    use_hugepage_ (use_hugepage)
{
    size_region1_ = size - size_region2_;
    // sanity checks
    if (size_region2_ > size_) {
        error_("region-2 size should be <= total size");
    }
    // allocate & init 1st region
    if (size_region1_ > 0) {
        addr1_ = allocNative_(size_region1_, raw_addr1_);
        std::cout << "Region-1 address: 0x" << std::hex
            << reinterpret_cast<uint64_t>(addr1_) << std::dec << std::endl;
        memset(addr1_, 0, size_region1_);
    }
    // allocate & init 2nd region
    if (size_region2_ > 0) {
        if (mem_type_region2_ == MemType::NATIVE) {
            addr2_ = allocNative_(size_region2_, raw_addr2_);
        } else {
            addr2_ = allocDevice_(size_region2_);
        }
        std::cout << "Region-2 address: 0x" << std::hex
            << reinterpret_cast<uint64_t>(addr2_) << std::dec << std::endl;
        memset(addr2_, 0, size_region2_);
    }
    // use a fixed seed
    srand(0);
}

MemRegion::~MemRegion() {
    if (addr1_) {
        if (use_hugepage_) {
            munmap(addr1_, size_region1_);
        } else {
          free(raw_addr1_);
        }
    }
    if (addr2_) {
        if (mem_type_region2_ == MemType::NATIVE) {
            if (use_hugepage_) {
                munmap(addr2_, size_region2_);
            } else {
                free(raw_addr2_);
            }
        } else {
            munmap(addr2_, size_region2_);
            if (fd_ != -1) {
                close(fd_);
            }
        }
    }
    addr1_ = NULL;
    addr2_ = NULL;
    raw_addr1_ = NULL;
    raw_addr2_ = NULL;
}

void MemRegion::error_(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

char* MemRegion::allocNative_(const uint64_t& size, char*& raw_addr) {
    char* addr = NULL;
    if (use_hugepage_) {
        addr = ((char*)mmap(
            0x0, size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
            0, 0
        ));
        if ((int64_t)addr == (int64_t)-1) {
            error_("mmap failed for hugepage");
        }
    } else {
        uint64_t alloc_size = size + 2 * page_size_;
        raw_addr = (char*)malloc(alloc_size);
        addr = raw_addr + page_size_ - (uint64_t)raw_addr % page_size_;
    }
    return addr;
}

char* MemRegion::allocDevice_(const uint64_t& size) {
    char* addr = NULL;
    const std::string dev_mem = "/dev/dax0.0";
    fd_ = open(dev_mem.c_str(), O_RDWR | O_SYNC);
    if (fd_ == -1) {
        error_("Failed to open " + dev_mem);
    }
    // mmap from fd
    if (use_hugepage_) {
        addr = ((char*)mmap(
            0x0, size,
            PROT_READ | PROT_WRITE,
            //MAP_SHARED | MAP_HUGETLB,
            MAP_SHARED,
            fd_, 0
        ));
        if ((int64_t)addr == (int64_t)-1) {
            error_("mmap failed for hugepage from device memory");
        }
    } else {
        addr = ((char*)mmap(
            0x0, size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd_, 0
        ));
        if ((int64_t)addr == (int64_t)-1) {
            error_("mmap failed from device memory");
        }
    }
    return addr;
}

void MemRegion::randomizeSequence_(
    std::vector<uint64_t>& sequence, uint64_t size, uint64_t unit, bool in_order)
{
    // initialize to sequential pattern
    for (uint64_t i = 0; i < size; ++i) {
        sequence[i] = i * unit;
    }
    if (in_order) {
      return;
    }
    // randomize by swapping
    uint64_t r = rand() ^ (rand() << 10);
    for (uint64_t i = size-1; i> 0; --i) {
        r = (r << 1) ^ rand();
        uint64_t tmp = sequence[r % (i+1)];
        sequence[r % (i+1)] = sequence[i];
        sequence[i] = tmp;
    }
    // start should still be start
    for (uint32_t i = 1; i < size; ++i) {
        if (sequence[i] == 0) {
            sequence[i] = sequence[0];
            sequence[0] = 0;
            break;
        }
    }
    // half should still be half
    for (uint32_t i = 1; i < size; ++i) {
        if (sequence[i] == size*unit/2) {
            sequence[i] = sequence[size/2];
            sequence[size/2] = size*unit/2;
        }
    }
}

char* MemRegion::getOffsetAddr_(uint64_t offset) const {
    if (size_region1_ > 0 && offset < size_region1_) {
        return &addr1_[offset];
    } else {
        assert(size_region2_ > 0);
        return &addr2_[offset - size_region1_];
    }
}

// create a circular list of pointers with sequential stride
void MemRegion::stride_init()
{
    uint64_t i = 0;
    for (i = line_size_; i < size_; i += line_size_) {
        *(char**)getOffsetAddr_(i - line_size_) = (char*)getOffsetAddr_(i);
    }
    *(char**)getOffsetAddr_(i - line_size_) = (char*)getOffsetAddr_(0);
}

// create a circular list of pointers with random-in-page
void MemRegion::page_random_init()
{
    std::vector<uint64_t> pages_(num_pages_, 0);
    std::vector<uint64_t> linesInPage_(num_lines_in_page_, 0);
    randomizeSequence_(pages_, num_pages_, page_size_, true);
    randomizeSequence_(linesInPage_, num_lines_in_page_, line_size_);
    // run through the pages
    for (uint64_t i = 0; i < num_pages_; ++i) {
        // run through the lines within a page
        for (uint64_t j = 0; j < num_lines_in_page_ - 1; ++j) {
            *(char**)getOffsetAddr_(pages_.at(i) + linesInPage_.at(j))
                = (char*)getOffsetAddr_(pages_.at(i) + linesInPage_.at(j + 1));
        }
        // jump the next page
        uint64_t next_page = (i == num_pages_ - 1) ? 0 : (i + 1);
        *(char**)getOffsetAddr_(pages_.at(i) + linesInPage_.at(num_lines_in_page_ - 1))
            = (char*)getOffsetAddr_(pages_.at(next_page) + linesInPage_.at(0));
    }
}

// create a circular list of pointers with all-random
void MemRegion::all_random_init()
{
    const uint64_t num_lines = numLines();
    std::vector<uint64_t> lines_(num_lines, 0);
    randomizeSequence_(lines_, num_lines, line_size_);
    // run through the lines
    for (uint64_t i = 0; i < num_lines - 1; ++i) {
        *(char**)getOffsetAddr_(lines_.at(i)) = (char*)getOffsetAddr_(lines_.at(i + 1));
    }
    *(char**)getOffsetAddr_(lines_.at(num_lines - 1)) = (char*)getOffsetAddr_(lines_.at(0));
}

void MemRegion::dump()
{
    std::cout << "================================" << std::endl;
    std::cout << "size=" << size_ << ", page=" << page_size_ << ", line=" << line_size_
        << ", numPage=" << num_pages_ << ", numLinesInPage=" << num_lines_in_page_ << std::endl;
    const uint64_t start_addr = addr1_ ? reinterpret_cast<uint64_t>(addr1_) : reinterpret_cast<uint64_t>(addr2_);
    for (uint64_t i = 0; i < size_; i += line_size_) {
        uint64_t curr = reinterpret_cast<uint64_t>(getOffsetAddr_(i));
        uint64_t next = reinterpret_cast<uint64_t>(*(char**)getOffsetAddr_(i));
        uint64_t curr_offset = (curr - start_addr) / line_size_;
        uint64_t next_offset = (next - start_addr) / line_size_;
        std::cout << std::hex << "[0x" << curr << "]: 0x" << next << std::dec << "  ";
        std::cout << std::setw(8) << std::right << curr_offset << ": " << std::setw(8) << std::left << next_offset;
        std::cout << std::endl;
    }
    //std::cout << "--------------------------------" << std::endl;
    //char** p = (char**)addr1_;
    //for (uint64_t i = 0; i < numLines(); ++i) {
    //    uint64_t curr = reinterpret_cast<uint64_t>(p);
    //    uint64_t next = reinterpret_cast<uint64_t>(*p);
    //    uint64_t curr_offset = (curr - start_addr)) / line_size_;
    //    uint64_t next_offset = (next - start_addr)) / line_size_;
    //    std::cout << std::hex << "[0x" << curr << "]: 0x" << next << std::dec << "  ";
    //    std::cout << std::setw(8) << std::right << curr_offset << ": " << std::setw(8) << std::left << next_offset;
    //    if (next > curr) {
    //        std::cout << "+" << next_offset - curr_offset;
    //    } else {
    //        std::cout << "-" << curr_offset - next_offset;
    //    }
    //    std::cout << std::endl;
    //    p = (char**)(*p);
    //}
    std::cout << "================================" << std::endl;
}

}
