#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>
#include <numaif.h>     // move_pages
#include <unistd.h>     // getpagesize

void print_usage() {
    std::cout << "[./move_pages] [pid] [start address] [# of pages] [target node]" << std::endl;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        print_usage();
        return 1;
    }
    // get command line arguments
    const int pid = std::stoi(argv[1]);
    const uint64_t addr = std::stoull(argv[2], 0, 16);
    const uint32_t num_pages = std::stoul(argv[3]);
    const int target_node = std::stoi(argv[4]);
    // sanity check
    const uint64_t os_page_size = getpagesize();
    if (addr % os_page_size) {
        std::cout << "start address not aligned to page boundary" << std::endl;
        return 1;
    }
    const bool verbose = true;
    void** pages = (void**)malloc(sizeof(char*) * num_pages);
    int* nodes = (int*)malloc(sizeof(int*) * num_pages);
    int* status = (int*)malloc(sizeof(int*) * num_pages);
    for (uint32_t i = 0; i < num_pages; ++i) {
        pages[i] = (char*)addr + i * os_page_size;
        nodes[i] = target_node;
        status[i] = 0;
    }
    int ret = move_pages(pid, num_pages, pages, nodes, status, 0);
    if (ret != 0) {
        std::cout << "Page migration failed with retcode=" << ret << std::endl;
    }
    if (verbose || ret != 0) {
        for (uint32_t i = 0; i < num_pages; ++i) {
            bool to_print = (status[i] < 0 || status[i] != target_node ||
                             i <= 2 || i >= num_pages - 2);
            if (to_print) {
                std::cout << std::hex << "0x" << reinterpret_cast<uint64_t>(pages[i])
                    << std::dec << "\t" << i << "\t";
            }
            if (status[i] >= 0) {
                if (to_print) {
                    std::cout << "to node " << status[i];
                }
            } else if (status[i] == -EACCES) {
                std::cout << "mapped by multiple procs";
            } else if (status[i] == -EBUSY) {
                std::cout << "page busy";
            } else if (status[i] == -EFAULT) {
                std::cout << "fault b/c zero page or not mapped";
            } else if (status[i] == -EIO) {
                std::cout << "unable to write-back page";
            } else if (status[i] == -EINVAL) {
                std::cout << "the dirty page cannot be moved";
            } else if (status[i] == -ENOENT) {
                std::cout << "page is not present";
            } else if (status[i] == -ENOMEM) {
                std::cout << "unable to allocate on target node";
            } else {
                std::cout << "unknown errno";
            }
            if (to_print) {
                std::cout << std::endl;
            }
        }
    }
    return ret;
}

