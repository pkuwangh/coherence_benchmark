#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <unistd.h>

#include "utils/lib_mem_region.hh"

int main() {

    auto test = [](
        std::vector<uint64_t> configs,
        bool use_hugepage,
        utils::MemType mem_type_region2,
        uint64_t size_region2,
        std::string pattern
        )->void {
        std::cout << std::endl << "Testing config:" << std::endl;
        std::cout << std::dec << "size=" << configs[0]/1024
            << " page=" << configs[1]/1024
            << ", hugepage=" << use_hugepage
            << ", region2 type=" << static_cast<char>(mem_type_region2)
            << " size=" << size_region2/1024
            << ", pattern=" << pattern << std::endl;
        assert(configs.size() == 3);
        utils::MemRegion::Handle mem_region(
            new utils::MemRegion(
              configs[0], configs[1], configs[2],
              use_hugepage, mem_type_region2, size_region2
            ));
        if (pattern == "stride") {
            mem_region->stride_init();
        } else if (pattern == "pageRand") {
            mem_region->page_random_init();
        } else if (pattern == "allRand") {
            mem_region->all_random_init();
        } else {
            assert(0);
        }
        mem_region->dump();
        if (use_hugepage) {
            sleep(4);
        }
    };

    //test({8192, 4096, 512}, false, utils::MemType::NATIVE, 0, "stride");
    //test({8192, 4096, 512}, false, utils::MemType::NATIVE, 4096, "stride");
    //test({8192, 4096, 512}, false, utils::MemType::NATIVE, 0, "pageRand");
    //test({8192, 4096, 512}, false, utils::MemType::NATIVE, 4096, "pageRand");
    //test({8192, 8192, 512}, false, utils::MemType::NATIVE, 4096, "pageRand");
    //test({8192, 8192, 512}, false, utils::MemType::NATIVE, 4096, "allRand");

    //test({4194304, 1048576, 524288}, true, utils::MemType::NATIVE, 0, "stride");

    test({2097152,  1048576, 262144}, false, utils::MemType::NATIVE, 2097152, "stride");
    test({536870912, 536870912, 67108864}, false, utils::MemType::NATIVE, 536870912, "stride");

    test({2097152, 1048576, 262144}, true, utils::MemType::DEVICE, 2097152, "stride");

    test({16777216, 16777216, 1048576}, false, utils::MemType::DEVICE, 8388608, "stride");

    return 0;
}
