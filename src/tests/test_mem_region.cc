#include <iostream>
#include <vector>
#include <cassert>
#include <string>

#include "utils/lib_mem_region.hh"

int main() {

    auto test = [](std::vector<uint32_t> configs, std::string pattern)->void {
        assert(configs.size() == 3);
        utils::MemRegion::Handle mem_region(new utils::MemRegion(configs[0], configs[1], configs[2]));
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
    };

    test({8192, 4096, 512}, "stride");
    test({16384, 4096, 512}, "pageRand");
    test({16384, 4096, 512}, "allRand");

//    test({131072, 131072, 64}, "pageRand");
    return 0;
}
