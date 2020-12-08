#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sys/mman.h>

#include "utils/lib_mem_region.hh"

namespace utils {

MemRegion::MemRegion(
        uint64_t size,
        uint64_t page_size,
        uint64_t line_size,
        bool use_hugepage) :
    size_ (size),
    page_size_ (page_size),
    line_size_ (line_size),
    use_hugepage_ (use_hugepage),
    num_pages_ (size_ / page_size_),
    num_lines_in_page_ (page_size_ / line_size_)
{
    // allocate a little extra space for page alignment
    const uint64_t alloc_size = size_ + 2 * page_size_;
    if (use_hugepage) {
        addr_ = ((char*)mmap(
            0x0, alloc_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
            0, 0
        ));
    } else {
        addr_ = ((char*)malloc(alloc_size));
    }
    // point base_ to page-aligned addr
    base_ = addr_;
    if ((uint64_t)base_ % (uint64_t)page_size_) {
        base_ += page_size_ - (uint64_t)base_ % page_size_;
    }
    // init to 0
    memset(base_, 0, size_);
    // use a fixed seed
    srand(0);
}

MemRegion::~MemRegion() {
    if (use_hugepage_) {
        munmap(addr_, size_ + 2 * page_size_);
    } else {
        free(addr_);
    }
}

void MemRegion::randomizeSequence_(std::vector<uint64_t>& sequence, uint64_t size, uint64_t unit)
{
    // initialize to sequential pattern
    for (uint64_t i = 0; i < size; ++i) {
        sequence[i] = i * unit;
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

// create a circular list of pointers with sequential stride
void MemRegion::stride_init()
{
    char* addr = base_;
    uint64_t i;
    for (i = line_size_; i < size_; i += line_size_) {
        *(char **)(&addr[i - line_size_]) = (char*)&addr[i];
    }
    *(char **)&addr[i - line_size_] = (char*)&addr[0];
}

// create a circular list of pointers with random-in-page
void MemRegion::page_random_init()
{
    std::vector<uint64_t> pages_(num_pages_, 0);
    std::vector<uint64_t> linesInPage_(num_lines_in_page_, 0);
    randomizeSequence_(pages_, num_pages_, page_size_);
    randomizeSequence_(linesInPage_, num_lines_in_page_, line_size_);
    // run through the pages
    for (uint64_t i = 0; i < num_pages_; ++i) {
        // run through the lines within a page
        for (uint64_t j = 0; j < num_lines_in_page_ - 1; ++j) {
            *(char**)(base_ + pages_.at(i) + linesInPage_.at(j))
                = base_ + pages_.at(i) + linesInPage_.at(j+1);
        }
        // jump the next page
        uint64_t next_page = (i == num_pages_ - 1) ? 0 : (i + 1);
        *(char**)(base_ + pages_.at(i) + linesInPage_.at(num_lines_in_page_ - 1))
            = base_ + pages_.at(next_page) + linesInPage_.at(0);
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
        *(char**)(base_ + lines_.at(i)) = base_ + lines_.at(i+1);
    }
    *(char**)(base_ + lines_.at(num_lines - 1)) = base_ + lines_.at(0);
}

void MemRegion::dump()
{
    std::cout << "================================" << std::endl;
    std::cout << "size=" << size_ << ", page=" << page_size_ << ", line=" << line_size_
        << ", numPage=" << num_pages_ << ", numLinesInPage=" << num_lines_in_page_ << std::endl;
    for (uint64_t i = 0; i < size_; i += line_size_) {
        uint64_t curr = reinterpret_cast<uint64_t>(base_ + i);
        uint64_t next = reinterpret_cast<uint64_t>(*(char**)(base_ + i));
        uint64_t curr_offset = (curr - reinterpret_cast<uint64_t>((char**)base_)) / line_size_;
        uint64_t next_offset = (next - reinterpret_cast<uint64_t>((char**)base_)) / line_size_;
        std::cout << std::hex << "[0x" << curr << "]: 0x" << next << std::dec << "  ";
        std::cout << std::setw(8) << std::right << curr_offset << ": " << std::setw(8) << std::left << next_offset;
        std::cout << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;
    char** p = (char**)base_;
    for (uint64_t i = 0; i < numLines(); ++i) {
        uint64_t curr = reinterpret_cast<uint64_t>(p);
        uint64_t next = reinterpret_cast<uint64_t>(*p);
        uint64_t curr_offset = (curr - reinterpret_cast<uint64_t>((char**)base_)) / line_size_;
        uint64_t next_offset = (next - reinterpret_cast<uint64_t>((char**)base_)) / line_size_;
        std::cout << std::hex << "[0x" << curr << "]: 0x" << next << std::dec << "  ";
        std::cout << std::setw(8) << std::right << curr_offset << ": " << std::setw(8) << std::left << next_offset;
        if (next > curr) {
            std::cout << "+" << next_offset - curr_offset;
        } else {
            std::cout << "-" << curr_offset - next_offset;
        }
        std::cout << std::endl;
        p = (char**)(*p);
    }
    std::cout << "================================" << std::endl;
}

}
