#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>      // open
#include <sys/mman.h>   // mmap
#include <unistd.h>     // lseek, read

#include "utils/lib_timing.hh"

class PageInfo {
  public:
    uint64_t vaddr = 0;
    uint64_t paddr = 0;

    PageInfo() = default;
    ~PageInfo() = default;
};


void randomize_sequence(std::vector<uint64_t>& sequence, uint64_t size, uint64_t unit) {
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
    for (uint64_t i = 1; i < size; ++i) {
        if (sequence[i] == 0) {
            sequence[i] = sequence[0];
            sequence[0] = 0;
            break;
        }
    }
}


void all_random_init(char* addr, uint64_t size, uint64_t line_size) {
    const uint64_t num_lines = size / line_size;
    std::vector<uint64_t> lines(num_lines, 0);
    randomize_sequence(lines, num_lines, line_size);
    // run through the lines
    for (uint64_t i = 0; i < num_lines - 1; ++i) {
        *((char**)(&addr[lines.at(i)])) = (char*)(&addr[lines.at(i+1)]);
    }
    *((char**)(&addr[lines.at(num_lines-1)])) = (char*)(&addr[lines.at(0)]);
}


void dump(const std::vector<PageInfo>& page_pool, uint64_t size = 0) {
    fprintf(stdout, "--------------------------------\n");
    uint64_t dump_size = (size > 0) ? size : page_pool.size();
    for (uint64_t idx = 0; idx < dump_size; ++idx) {
        const uint64_t* ptr = (uint64_t*)(page_pool[idx].vaddr);
        fprintf(stdout, "%-lu : [%#llx] -> [%#llx] : %#llx\n",
                idx, page_pool[idx].vaddr, page_pool[idx].paddr,
                ptr ? *ptr : 0xdeadbeef);
        if (!ptr) {
            break;
        }
    }
}


#define LOOP1     p1 = (char **)*p1;
#define LOOP4     LOOP1 LOOP1 LOOP1 LOOP1
#define LOOP16    LOOP4 LOOP4 LOOP4 LOOP4
#define LOOP64    LOOP16 LOOP16 LOOP16 LOOP16
#define LOOP256   LOOP64 LOOP64 LOOP64 LOOP64

bool benchmark_loads(const char* region_addr, uint64_t loop_count, uint64_t num_iter)
{
    char **start = (char**)region_addr;
    char **p1 = start;
    uint64_t i = 0;
    while (num_iter > 0) {
        p1 = start;
        -- num_iter;
        for (i = 0; i < loop_count; ++i) {
            LOOP256;
        }
    }
    return (p1 == NULL);
}


int main() {
    utils::start_timer("startup");
    const uint64_t page_size = ((uint64_t)4 << 10);
    // allocate a pool of pages
    const uint64_t pool_size = ((uint64_t)4 << 10 << 10 << 10);
    const uint64_t pool_page_count = pool_size / page_size;
    fprintf(stdout, "page pool size: %lu / %lu Byte\n", pool_page_count, pool_size);
    assert(pool_size % page_size == 0);
    char* pool_addr = (char*)mmap(
            0x0, pool_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            0, 0);
    memset(pool_addr, 1, pool_size);
    // lookup phys addr
    std::vector<PageInfo> page_pool(pool_page_count);
    const uint64_t pagemap_entry_size = 8;
    const uint64_t pagemap_buf_size = pagemap_entry_size * pool_page_count;
    uint64_t* const pagemap_buf = (uint64_t*)malloc(pagemap_buf_size);
    if (pagemap_buf == nullptr) {
        fprintf(stderr, "Cannot allocate pagemap buf (%llu B)\n", pagemap_buf_size);
    }
    std::string pagemap_file = "/proc/self/pagemap";
    int fd = open(pagemap_file.c_str(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s\n", pagemap_file.c_str());
        return -1;
    }
    const uint64_t offset = pagemap_entry_size * (uint64_t)pool_addr / page_size;
    // seek to start addr
    if (lseek(fd, offset, SEEK_SET) < 0) {
        fprintf(stderr, "Cannot seek pagemap file with offset=%llu\n", offset);
        return -1;
    }
    // read whole range
    if (read(fd, pagemap_buf, pagemap_buf_size) < 0) {
        fprintf(stderr, "Failed to read pagemap\n");
        return -1;
    }
    for (uint64_t idx = 0; idx < pool_page_count; ++idx) {
        const uint64_t pfn = pagemap_buf[idx] & (~(0x1ffLLU << 55));
        const uint64_t paddr = (pfn << 12);
        const uint64_t vaddr = (uint64_t)pool_addr + idx * page_size;
        page_pool[idx].vaddr = vaddr;
        page_pool[idx].paddr = paddr;
    }
    //dump(page_pool);
    close(fd);
    free(pagemap_buf);
    // sort pages based on physical addr
    std::sort(page_pool.begin(), page_pool.end(),
              [](const PageInfo& e1, const PageInfo& e2) { return e1.paddr < e2.paddr; });
    //dump(page_pool);
    // pick pages contiguous in physical space
    const uint64_t group_size = 4;
    const uint64_t num_pages = 48 * 4;
    std::vector<PageInfo> page_picks(num_pages);
    uint64_t num_picks = 0;
    for (uint64_t idx = 0; (idx + group_size - 1) < pool_page_count; ++idx) {
        const uint64_t chunk_size = group_size * page_size;
        if (page_pool[idx].paddr % chunk_size == 0) {
            if (page_pool[idx + group_size - 1].paddr == page_pool[idx].paddr + chunk_size - page_size) {
                for (uint64_t j = 0; j < group_size; ++j) {
                    page_picks[num_picks] = page_pool[idx + j];
                    ++num_picks;
                }
                if (num_picks >= num_pages) {
                    break;
                }
            }
        }
    }
    fprintf(stdout, "picked pages: %lu\n", num_picks);
    //dump(page_picks, num_picks);
    // allocate new space and re-map
    const uint64_t region_size = num_pages * page_size;
    fprintf(stdout, "region size: %lu / %lu Byte\n", num_pages, region_size);
    char* region_addr = (char*)mmap(
            0x0, region_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            0, 0);
    memset(region_addr, 2, region_size);
    uint32_t num_remaps = num_picks;
    num_remaps = 0;
    for (uint64_t idx = 0; idx < num_remaps; ++idx) {
        uint64_t vaddr = (uint64_t)region_addr + idx * page_size;
        uint64_t data = *((uint64_t*)vaddr);
        void* ptr = mremap(
                (void*)(page_picks[idx].vaddr),
                page_size,
                page_size,
                MREMAP_MAYMOVE | MREMAP_FIXED,
                (void*)vaddr);
        fprintf(stdout, "%-4lu [%#llx] / [%#llx] : %#llx <- %#llx\n",
                idx, (uint64_t)ptr, (uint64_t)vaddr, *((uint64_t*)vaddr), data);
    }
    // init for pointer chasing pattern
    fprintf(stdout, "starting address %#llx\n", region_addr);
    //const uint64_t line_size = 4096 * 32;
    const uint64_t line_size = 64;
    all_random_init(region_addr, region_size, line_size);
    //for (uint64_t i = 0; i < region_size; i += line_size) {
    //    uint64_t curr = (uint64_t)(&region_addr[i]);
    //    uint64_t next = (uint64_t)(*((char**)curr));
    //    uint64_t curr_offset = (curr - (uint64_t)region_addr) / line_size;
    //    uint64_t next_offset = (next - (uint64_t)region_addr) / line_size;
    //    fprintf(stdout, "[%#llx] : %#llx\t%llu : %llu\n",
    //            curr, next, curr_offset, next_offset);
    //}
    // free the pool
    fflush(stdout);
    // benchmarking
    utils::end_timer("startup", std::cout);
    utils::start_timer("warmup");
    const uint64_t num_iters = 10000;
    const uint64_t loop_unroll = 256;
    const uint64_t num_chases = region_size / line_size;
    std::cout << "# of pointer chases per iter: " << num_chases << std::endl;
    const uint64_t loop_count = num_chases / loop_unroll;
    const float core_freq_ghz = 1.4;
    assert (num_chases % loop_unroll == 0);
    bool error = false;
    error |= benchmark_loads(region_addr, loop_count, num_iters/10);
    utils::end_timer("warmup", std::cout);
    utils::start_timer("benchmark");
    error |= benchmark_loads(region_addr, loop_count, num_iters);
    utils::end_timer("benchmark", std::cout, num_chases * num_iters, core_freq_ghz);
    munmap((void*)region_addr, region_size);
    munmap((void*)pool_addr, pool_size);
    return 0;
}
