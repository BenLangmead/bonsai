#include "test/catch.hpp"
#include "sketch/setsketch.h"
#include <cstdint>
#include <vector>

// With a minimum count, CountFilteredCSetSketch admits an id once it has
// occurred mincount times, and its saved count for a register must be the
// number of times that id occurred, as for the unfiltered CSetSketch.

TEST_CASE("CountFilteredCSetSketch saves the full count of each sampled id") {
    const uint32_t mincount = 2;
    const size_t n = 3000, m = 64;
    auto count = [](uint64_t id) {return uint32_t(1 + id % 5);};
    sketch::setsketch::CountFilteredCSetSketch<double> s(mincount, m, true, true);
    // Interleave the occurrences: pass p adds every id that occurs more than p times.
    for(uint32_t p = 0; p < 5; ++p)
        for(uint64_t id = 1; id <= n; ++id)
            if(count(id) > p) s.update(id);
    size_t nchecked = 0, nwrong = 0;
    for(size_t i = 0; i < m; ++i) {
        const uint64_t id = s.ids()[i];
        if(id == 0) continue;
        ++nchecked;
        REQUIRE(count(id) >= mincount);
        nwrong += s.idcounts()[i] != count(id);
    }
    REQUIRE(nchecked == m);
    REQUIRE(nwrong == 0);
}
