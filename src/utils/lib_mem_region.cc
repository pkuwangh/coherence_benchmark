#include <cassert>
#include <iostream>
#include <iomanip>

#include "utils/lib_mem_region.hh"

namespace utils {

MemRegion::MemRegion(
        uint32_t size,
        uint32_t pageSize,
        uint32_t lineSize) :
    size_ (size),
    pageSize_ (pageSize),
    lineSize_ (lineSize),
    numPages_ (size_ / pageSize_),
    numLinesInPage_ (pageSize_ / lineSize_)
{
    // allocate a little extra space for page alignment
    addr_.reset(new char[size_ + 2 * pageSize_]);
    // point base_ to page-aligned addr
    base_ = addr_.get();
    if ((uint64_t)base_ % (uint64_t)pageSize_) {
        base_ += pageSize_ - (uint64_t)base_ % pageSize_;
    }
    // use a fixed seed
    srand(0);
}

void MemRegion::randomizeSequence_(std::vector<uint32_t>& sequence, uint32_t size, uint32_t unit)
{
    // initialize to sequential pattern
    for (uint32_t i = 0; i < size; ++i) {
        sequence[i] = i * unit;
    }
    // randomize by swapping
    uint32_t r = rand() ^ (rand() << 10);
    for (uint32_t i = size-1; i> 0; --i) {
        r = (r << 1) ^ rand();
        uint32_t tmp = sequence[r % (i+1)];
        sequence[r % (i+1)] = sequence[i];
        sequence[i] = tmp;
    }
}

// create a circular list of pointers with sequential stride
void MemRegion::stride_init()
{
    char* addr = base_;
    uint32_t i;
    for (i = lineSize_; i < size_; i += lineSize_) {
        *(char **)(&addr[i - lineSize_]) = (char*)&addr[i];
    }
    *(char **)&addr[i - lineSize_] = (char*)&addr[0];
}

// create a circular list of pointers with random-in-page
void MemRegion::page_random_init()
{
    std::vector<uint32_t> pages_(numPages_, 0);
    std::vector<uint32_t> linesInPage_(numLinesInPage_, 0);
    randomizeSequence_(pages_, numPages_, pageSize_);
    randomizeSequence_(linesInPage_, numLinesInPage_, lineSize_);
    // run through the pages
    for (uint32_t i = 0; i < numPages_; ++i) {
        // run through the lines within a page
        for (uint32_t j = 0; j < numLinesInPage_ - 1; ++j) {
            *(char**)(base_ + pages_.at(i) + linesInPage_.at(j))
                = base_ + pages_.at(i) + linesInPage_.at(j+1);
        }
        // jump the next page
        uint32_t next_page = (i == numPages_ - 1) ? 0 : (i + 1);
        *(char**)(base_ + pages_.at(i) + linesInPage_.at(numLinesInPage_ - 1))
            = base_ + pages_.at(next_page) + linesInPage_.at(0);
    }
}

// create a circular list of pointers with all-random
void MemRegion::all_random_init()
{
    const uint32_t num_lines = numLines();
    std::vector<uint32_t> lines_(num_lines, 0);
    randomizeSequence_(lines_, num_lines, lineSize_);
    // run through the lines
    for (uint32_t i = 0; i < num_lines - 1; ++i) {
        *(char**)(base_ + lines_.at(i)) = base_ + lines_.at(i+1);
    }
    *(char**)(base_ + lines_.at(num_lines - 1)) = base_ + lines_.at(0);
}

void MemRegion::dump()
{
    std::cout << "================================" << std::endl;
    std::cout << "size=" << size_ << ", page=" << pageSize_ << ", line=" << lineSize_
        << ", numPage=" << numPages_ << ", numLinesInPage=" << numLinesInPage_ << std::endl;
    for (uint32_t i = 0; i < size_; i += lineSize_) {
        uint64_t curr = reinterpret_cast<uint64_t>(base_ + i);
        uint64_t next = reinterpret_cast<uint64_t>(*(char**)(base_ + i));
        uint64_t curr_offset = (curr - reinterpret_cast<uint64_t>((char**)base_)) / lineSize_;
        uint64_t next_offset = (next - reinterpret_cast<uint64_t>((char**)base_)) / lineSize_;
        std::cout << std::hex << "[0x" << curr << "]: 0x" << next << std::dec << "  ";
        std::cout << std::setw(8) << std::right << curr_offset << ": " << std::setw(8) << std::left << next_offset;
        std::cout << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;
    char** p = (char**)base_;
    for (uint32_t i = 0; i < numLines(); ++i) {
        uint64_t curr = reinterpret_cast<uint64_t>(p);
        uint64_t next = reinterpret_cast<uint64_t>(*p);
        uint64_t curr_offset = (curr - reinterpret_cast<uint64_t>((char**)base_)) / lineSize_;
        uint64_t next_offset = (next - reinterpret_cast<uint64_t>((char**)base_)) / lineSize_;
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
