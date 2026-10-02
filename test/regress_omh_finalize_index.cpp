#include "test/catch.hpp"
#include "sketch/bmh.h"
#include <random>
#include <string>
#include <vector>

// An OrderMinHash sketch of a char sequence must depend only on the
// sequence: the selected positions index the n input characters, so the
// bytes before the sequence in memory must not change the result.

TEST_CASE("OMHasher sketch of a char sequence ignores the bytes before it") {
    std::mt19937_64 rng(7);
    const size_t n = 1000;
    std::string seq(n, 'A');
    for(auto &c: seq) c = "ACGT"[rng() % 4];
    const size_t pad = 256;
    std::string a(pad, 'A'), b(pad, 'T');
    for(auto &c: b) c = char(rng());
    a += seq;
    b += seq;
    sketch::omh::OMHasher<double> h(64, 5);
    const std::vector<uint64_t> sa = h.hash(a.data() + pad, n);
    const std::vector<uint64_t> sb = h.hash(b.data() + pad, n);
    size_t neq = 0;
    for(size_t i = 0; i < sa.size(); ++i) neq += sa[i] == sb[i];
    REQUIRE(neq == sa.size());
}
