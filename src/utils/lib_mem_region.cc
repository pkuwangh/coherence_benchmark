#include <cassert>
#include <iostream>
#include <iomanip>

#include "utils/lib_mem_region.hh"

namespace utils {

MemRegion::MemRegion(
        uint64_t size,
        uint64_t page_size,
        uint64_t line_size) :
    size_ (size),
    page_size_ (page_size),
    line_size_ (line_size),
    num_pages_ (size_ / page_size_),
    num_lines_in_page_ (page_size_ / line_size_)
{
    // allocate a little extra space for page alignment
    addr_.reset(new char[size_ + 2 * page_size_]);
    // point base_ to page-aligned addr
    base_ = addr_.get();
    if ((uint64_t)base_ % (uint64_t)page_size_) {
        base_ += page_size_ - (uint64_t)base_ % page_size_;
    }
    // use a fixed seed
    srand(0);
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
