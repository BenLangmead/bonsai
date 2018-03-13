#ifndef _FEATURE_MIN__
#define _FEATURE_MIN__

#include "encoder.h"
#include "spacer.h"
#include "khash64.h"
#include "util.h"
#include "klib/kthread.h"
#include <set>

// Decode 64-bit hash (contains both tax id and taxonomy depth for id)
#define TDtax(key) ((tax_t)key)
#define TDdepth(key) ((tax_t)~0 - (key >> 32))
#define TDencode(depth, taxid) (((u64)((tax_t)~0 - depth) << 32) | taxid)

// Decode 64-bit hash for feature counting.
// TODO: add building of FeatureMin hash
#define FMtax(key) ((tax_t)key)
#define FMcount(key) (key >> 32)

#define FMencode(count, taxid) (((u64)count << 32) | taxid)


namespace emp {


template<typename ScoreType>
khash_t(64) *feature_count_map(const std::vector<std::string> fns, khash_t(p) *tax_map, const char *seq2tax_path, const Spacer &sp, int num_threads, bool canon, size_t start_size);
khash_t(c) *make_depth_hash(khash_t(c) *lca_map, khash_t(p) *tax_map);
void lca2depth(khash_t(c) *lca_map, khash_t(p) *tax_map);

khash_t(64) *make_taxdepth_hash(khash_t(c) *kc, const khash_t(p) *tax);


void update_lca_map(khash_t(c) *kc, const khash_t(all) *set, const khash_t(p) *tax, tax_t taxid);
void update_td_map(khash_t(64) *kc, const khash_t(all) *set, const khash_t(p) *tax, tax_t taxid);
void update_feature_counter(khash_t(64) *kc, const khash_t(all) *set, const khash_t(p) *tax, tax_t taxid);
void update_minimized_map(const khash_t(all) *set, const khash_t(64) *full_map, khash_t(c) *ret);

// Wrap these in structs so that downstream code can be managed as a set, not updated one-by-one.
struct LcaMap {
    using ReturnType = khash_t(c) *;
    static constexpr size_t KeySize = sizeof(*(ReturnType{0})->keys);
    static void update(const khash_t(p) *tax, const khash_t(all) *set, const khash_t(64) *d64, khash_t(c) *r32, khash_t(64) *r64, tax_t taxid) {
        update_lca_map(r32, set, tax, taxid);
    }
};
struct TdMap {
    using ReturnType = khash_t(64) *;
    static constexpr size_t KeySize = sizeof(*(ReturnType{0})->keys);
    static void update(const khash_t(p) *tax, const khash_t(all) *set, const khash_t(64) *d64, khash_t(c) *r32, khash_t(64) *r64, tax_t taxid) {
        update_lca_map(r32, set, tax, taxid);
    }
};
struct FcMap {
    using ReturnType = khash_t(64) *;
    static constexpr size_t KeySize = sizeof(*(ReturnType{0})->keys);
    static void update(const khash_t(p) *tax, const khash_t(all) *set, const khash_t(64) *d64, khash_t(c) *r32, khash_t(64) *r64, tax_t taxid) {
        update_feature_counter(r64, set, tax, taxid);
    }
};
struct MinMap {
    using ReturnType = khash_t(c) *;
    static constexpr size_t KeySize = sizeof(*(ReturnType{0})->keys);
    static void update(const khash_t(p) *tax, const khash_t(all) *set, const khash_t(64) *d64, khash_t(c) *r32, khash_t(64) *r64, tax_t taxid) {
        update_feature_counter(r64, set, tax, taxid);
    }
};

template<typename ScoreType>
size_t fill_set_genome(const char *path, const Spacer &sp, khash_t(all) *ret, size_t index, void *data, bool canon, kseq_t *ks=nullptr) {
    LOG_ASSERT(ret);
    LOG_DEBUG("Filling from genome at path %s\n", path);

    Encoder<ScoreType> enc(0, 0, sp, data, canon);
    enc.add(ret, path, ks);
    LOG_DEBUG("Set of size %lu filled from genome at path %s\n", kh_size(ret), path);
    return index;
}

template<typename Container, typename ScoreType>
size_t fill_set_genome_container(Container &container, const Spacer &sp, khash_t(all) *ret, void *data, bool canon, kseq_t *ks=nullptr) {
    bool destroy;
    size_t sz(0);
    for(std::string &str: container)
        sz += fill_set_genome<ScoreType>(str.data(), sp, ret, 0, data, canon);
    return sz;
}

template<typename ScoreType>
khash_t(64) *ftct_map(const std::vector<std::string> &fns, khash_t(p) *tax_map,
                      const char *seq2tax_path,
                      const Spacer &sp, int num_threads, bool canon, size_t start_size) {
    return feature_count_map<ScoreType>(fns, tax_map, seq2tax_path, sp, num_threads, canon, start_size);
}

template<typename ScoreType>
khash_t(c) *minimized_map(std::vector<std::string> fns,
                          const khash_t(64) *full_map,
                          const Spacer &sp, int num_threads, size_t start_size, bool canon) {
    size_t submitted(0), completed(0), todo(fns.size());
    std::vector<khash_t(all) *> counters(todo, nullptr);
    khash_t(c) *ret(kh_init(c));
    kh_resize(c, ret, start_size);
    std::vector<std::future<size_t>> futures;
    //for(auto &i: fns) fprintf(stderr, "Filename: %s\n", i.data());

    if(num_threads < 0) num_threads = 16;

    LOG_DEBUG("Number of items to do: %zu\n", todo);

    for(size_t i(0); i < todo; ++i) counters[i] = kh_init(all), kh_resize(all, counters[i], start_size);

    // Submit the first set of jobs
    for(int i(0), e(std::min(num_threads, (int)todo)); i < e; ++i) {
#pragma message("TODO: Update this with the other approach.")
        futures.emplace_back(std::async(
          std::launch::async, fill_set_genome<ScoreType>, fns[i].data(), sp, counters[i], i, (void *)full_map, canon, nullptr));
        ++submitted;
    }

    // Daemon -- check the status of currently running jobs, submit new ones when available.
    while(submitted < todo) {
        //LOG_DEBUG("Submitted %zu, todo %zu\n", submitted, todo);
        if(auto f = std::find_if(futures.begin(), futures.end(), [](auto &f) {return is_ready(f);}); f != futures.end()) {
            const size_t index(f->get());
            futures.erase(f);
            futures.emplace_back(std::async(
                 std::launch::async, fill_set_genome<ScoreType>, fns[submitted].data(),
                 sp, counters[submitted], submitted, (void *)full_map, canon, nullptr));
            LOG_DEBUG("Submitted for %zu. Updating map for %zu. Total completed/all: %zu/%zu. Current size: %zu\n",
                     submitted, index, completed, todo, kh_size(ret));
            ++submitted, ++completed;
            update_minimized_map(counters[index], full_map, ret);
            kh_destroy(all, counters[index]); // Destroy set once we're done with it.
        }
    }

    // Join
    for(auto &f: futures) if(f.valid()) {
        const size_t index(f.get());
        update_minimized_map(counters[index], full_map, ret);
        kh_destroy(all, counters[index]);
        ++completed;
        LOG_DEBUG("Number left to do: %zu\n", todo - completed);
    }
    LOG_DEBUG("Finished minimized map building! Subbed %zu, completed %zu.\n", submitted, completed);

    // Clean up
    LOG_DEBUG("Cleaned up after LCA map building!\n");
    return ret;
}

template<typename ScoreType>
khash_t(64) *taxdepth_map(const std::vector<std::string> &fns, khash_t(p) *tax_map,
                          const char *seq2tax_path, const Spacer &sp,
                          int num_threads, bool canon, size_t start_size=1<<10) {
    size_t submitted(0), completed(0), todo(fns.size());
    std::vector<khash_t(all) *> counters(todo, nullptr);
    khash_t(64) *ret(kh_init(64));
    kh_resize(64, ret, start_size);
    khash_t(name) *name_hash(build_name_hash(seq2tax_path));
    std::vector<std::future<size_t>> futures;

    for(size_t i(0), end(fns.size()); i != end; ++i) counters[i] = kh_init(all);

    // Submit the first set of jobs
    std::set<size_t> subbed, used;
    for(int i(0), e(std::min(num_threads, (int)todo)); i < e; ++i) {
        LOG_DEBUG("Launching thread to read from file %s.\n", fns[i].data());
        futures.emplace_back(std::async(
          std::launch::async, fill_set_genome<ScoreType>, fns[i].data(), sp, counters[i], i, nullptr, canon, nullptr));
#pragma message("TODO: Update this with the other approach.")
        //LOG_DEBUG("Submitted for %zu.\n", submitted);
        subbed.insert(submitted);
        ++submitted;
    }

    // Daemon -- check the status of currently running jobs, submit new ones when available.
    while(submitted < todo) {
        LOG_DEBUG("Submitted %zu, todo %zu\n", submitted, todo);
        for(auto &f: futures) {
            if(is_ready(f)) {
                if(submitted == todo) break;
                const size_t index(f.get());
                if(used.find(index) != used.end()) continue;
                used.insert(index);
                if(subbed.find(submitted) != subbed.end()) throw std::runtime_error("Could not find what I was looking for....");
                LOG_DEBUG("Launching thread to read from file %s.\n", fns[submitted].data());
                f = std::async(
                  std::launch::async, fill_set_genome<ScoreType>, fns[submitted].data(),
                  sp, counters[submitted], submitted, nullptr, canon,  nullptr);
                subbed.insert(submitted);
                LOG_INFO("Submitted for %zu. Updating map for %zu. Total completed/all: %zu/%zu. Total size: %zu.\n",
                         submitted, index, completed, todo, kh_size(ret));
                ++submitted, ++completed;
                const tax_t taxid(get_taxid(fns[index].data(), name_hash));
                LOG_DEBUG("Just fetched taxid from file %s %u.\n", fns[index].data(), taxid);
                update_td_map(ret, counters[index], tax_map, taxid);
                kh_destroy(all, counters[index]); // Destroy set once we're done with it.
            }
        }
    }

    // Join
    for(auto &f: futures) if(f.valid()) {
        const size_t index(f.get());
        if(used.find(index) != used.end()) continue;
        used.insert(index);
        update_td_map(ret, counters[index], tax_map, get_taxid(fns[index].data(), name_hash));
        kh_destroy(all, counters[index]);
        ++completed;
        LOG_DEBUG("Number left to do: %zu\n", todo - completed);
    }
    LOG_DEBUG("Finished LCA map building! Subbed %zu, completed %zu, size of futures %zu.\n", submitted, completed, used.size());
#if !NDEBUG
    for(size_t i(0); i < todo; ++i) assert(used.find(i) != used.end());
#endif

    // Clean up
    khash_destroy(name_hash);
    LOG_DEBUG("Cleaned up after LCA map building!\n");
    return ret;
}

#if 0
template<typename ScoreType>
khash_t(c) *lca_map(const std::vector<std::string> &fns, khash_t(p) *tax_map,
                    const char *seq2tax_path,
                    const Spacer &sp, int num_threads, bool canon, size_t start_size=1<<10) {
    size_t submitted(0), completed(0), todo(fns.size());
    std::vector<khash_t(all) *> counters;
    khash_t(c) *ret(kh_init(c));
    kh_resize(c, ret, start_size);
    counters.reserve(todo);
    khash_t(name) *name_hash(build_name_hash(seq2tax_path));
    std::vector<std::future<size_t>> futures;

    for(size_t i(0), end(fns.size()); i != end; ++i) counters.emplace_back(kh_init(all));


    // Submit the first set of jobs
    std::set<size_t> subbed, used;
    for(int i(0), e(std::min(num_threads, (int)todo)); i < e; ++i) {
        LOG_DEBUG("Launching thread to read from file %s.\n", fns[i].data());
#pragma message("TODO: Update this with the other approach.")
        futures.emplace_back(std::async(
          std::launch::async, fill_set_genome<ScoreType>, fns[i].data(), sp, counters[i], i, nullptr, canon, nullptr));
        subbed.insert(submitted);
        ++submitted;
    }

    // Daemon -- check the status of currently running jobs, submit new ones when available.
    while(submitted < todo) {
        // Consider saving these temporary hashmaps to reduce allocations.
        // Also use templates to store the three methods in one generic application.
        LOG_DEBUG("Submitted %zu, todo %zu\n", submitted, todo);
        tax_t taxid;
        for(auto &f: futures) {
            if(is_ready(f)) {
                if(submitted == todo) break;
                const size_t index(f.get());
                if(used.find(index) != used.end()) continue;
                used.insert(index);
                if(subbed.find(submitted) != subbed.end()) throw std::runtime_error("Could not find what I was looking for....");
                LOG_DEBUG("Launching thread to read from file %s.\n", fns[submitted].data());
#pragma message("TODO: Update this with the other approach.")
                f = std::async(
                  std::launch::async, fill_set_genome<ScoreType>, fns[submitted].data(),
                  sp, counters[submitted], submitted, nullptr, canon, nullptr);
                subbed.insert(submitted);
                LOG_INFO("Submitted for %zu. Updating map for %zu. Total completed/all: %zu/%zu. Total size: %zu\n",
                         submitted, index, completed, todo, kh_size(ret));
                ++submitted, ++completed;
                if(unlikely((taxid = get_taxid(fns[index].data(), name_hash)) == UINT32_C(-1))) {
                    LOG_WARNING("Taxid for %s not listed in summary.txt. Not including.\n", fns[index].data());
                } else update_lca_map(ret, counters[index], tax_map, taxid);
                kh_destroy(all, counters[index]); // Destroy set once we're done with it.
            }
        }
    }

    // Join
    for(auto &f: futures) if(f.valid()) {
        const size_t index(f.get());
        if(used.find(index) != used.end()) continue;
        used.insert(index);
        tax_t taxid(get_taxid(fns[index].data(), name_hash));
        if(taxid == UINT32_C(-1)) {
            LOG_WARNING("Taxid for %s not listed in summary.txt. Not including.", fns[index].data());
        } else update_lca_map(ret, counters[index], tax_map, taxid);
        kh_destroy(all, counters[index]);
        ++completed;
        LOG_DEBUG("Number left to do: %zu\n", todo - completed);
    }
    LOG_DEBUG("Finished LCA map building! Subbed %zu, completed %zu, size of futures %zu.\n", submitted, completed, used.size());

    // Clean up
    khash_destroy(name_hash);
    LOG_DEBUG("Cleaned up after LCA map building!\n");
    return ret;
}
#endif

template<typename ScoreType, typename MapUpdater>
typename MapUpdater::ReturnType
make_map(const std::vector<std::string> fns, khash_t(p) *tax_map, const char *seq2tax_path, const Spacer &sp, int num_threads, bool canon, size_t start_size, const khash_t(64) *data) {
    MapUpdater mu;

    khash_t(c) *r32;
    khash_t(64) *r64;
    using ReturnType = typename MapUpdater::ReturnType;
    // Update this to include tax ids in the hash map.
    size_t submitted(0), completed(0), todo(fns.size());
    std::vector<khash_t(all)> counters(num_threads);
    if constexpr(ReturnType::KeySize == 8) {
        r64 = static_cast<typename MapUpdater::ReturnType>(std::calloc(sizeof(typename MapUpdater::ReturnType), 1));
        kh_resize(64, r64, start_size);
        r32 = nullptr;
    } else {
        r64 = nullptr;
        kh_resize(c, r32, start_size);
        r32 = static_cast<typename MapUpdater::ReturnType>(std::calloc(sizeof(typename MapUpdater::ReturnType), 1));
    }
    khash_t(name) *name_hash(build_name_hash(seq2tax_path));
    std::vector<std::future<size_t>> futures;
    // Mkae the future return the kseq pointer and then use it for resubmission.
    // TODO: Also use a fixed st of kh_all sets to reduce memory allocations.
    std::vector<kseq_t> kseqs;
    std::vector<uint32_t> counter_map;
    while(kseqs.size() < (unsigned)num_threads) kseqs.emplace_back(kseq_init_stack());

    // Submit the first set of jobs
    std::set<size_t> used;
    for(size_t i(0); i < (unsigned)num_threads && i < todo; ++i) {
        futures.emplace_back(std::async(
          std::launch::async, fill_set_genome<ScoreType>, fns[i].data(), sp, counters.data() + i, i, data, canon, &kseqs[submitted]));
        counter_map.emplace_back(submitted);
        LOG_DEBUG("Submitted for %zu.\n", submitted);
        ++submitted;
    }

    // Daemon -- check the status of currently running jobs, submit new ones when available.
    while(submitted < todo) {
        if(auto it = std::find_if(std::begin(futures), std::end(futures), [](auto &f) {return is_ready(f);}); it != futures.end()) {
            auto &f(*it);
            const size_t index(f.get());
            if(submitted == todo) break;
            if(used.find(index) != used.end()) continue;
            used.insert(index);
            const auto coffset = counter_map.at(index);
            khash_t(all) *counter = counters.data() + coffset; // Pointer to the counter to use
            kseq_t *ks_to_submit = kseqs.data() + coffset;
            f = std::async(
              std::launch::async, fill_set_genome<ScoreType>, fns[submitted].data(),
              sp, counter, submitted, data, canon, ks_to_submit);
            counter_map.emplace_back(coffset);
            ++submitted, ++completed;
            const tax_t taxid(get_taxid(fns[index].data(), name_hash));
            mu.update(tax_map, counter, data, r32, r64, taxid);
        }
    }

    // Join
    for(auto &f: futures) if(f.valid()) {
        const size_t index(f.get());
        const tax_t taxid(get_taxid(fns[index].data(), name_hash));
        mu.update(tax_map, counters.data() + index, data, r32, r64, taxid);
        ++completed;
    }

    // Clean up
    for(auto &ks: kseqs) kseq_destroy_stack(ks);
    for(auto &counter: counters) {
        std::free(counter.flags);
        std::free(counter.keys);
    }
    kh_destroy(name, name_hash);
    if constexpr(ReturnType::KeySize == 8)
        return r64;
    else
        return r32;
}
template<typename ScoreType>
auto feature_count_map(const std::vector<std::string> fns, khash_t(p) *tax_map, const char *seq2tax_path, const Spacer &sp, int num_threads, bool canon, size_t start_size) {
    return make_map<ScoreType, FcMap>(fns, tax_map, seq2tax_path, sp, num_threads, canon, start_size, nullptr);
}
template<typename ScoreType>
khash_t(c) *lca_map(const std::vector<std::string> &fns, khash_t(p) *tax_map,
                    const char *seq2tax_path,
                    const Spacer &sp, int num_threads, bool canon, size_t start_size) {
    return make_map<ScoreType, LcaMap>(fns, tax_map, seq2tax_path, sp, num_threads, canon, start_size, nullptr);
}



} // namespace emp
#endif // #ifdef _FEATURE_MIN__

