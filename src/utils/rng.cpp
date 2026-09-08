#include "rng.hpp"
#include <random>
#include <mutex>

namespace ChessEngine {

namespace {
uint64_t s_current_seed = 42ULL;
std::mt19937_64 s_rng(42ULL);
std::mutex s_rng_mutex;
}

void set_global_seed(uint64_t seed) {
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    s_current_seed = seed;
    s_rng.seed(seed);
}

uint64_t get_global_seed() {
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    return s_current_seed;
}

uint64_t rand_u64() {
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    return s_rng();
}

uint32_t rand_u32() {
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    return static_cast<uint32_t>(s_rng());
}

uint32_t rand_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(s_rng);
}

size_t rand_index(size_t size) {
    if (size <= 1) return 0;
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    std::uniform_int_distribution<size_t> dist(0, size - 1);
    return dist(s_rng);
}

double rand_double() {
    std::lock_guard<std::mutex> lock(s_rng_mutex);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(s_rng);
}

} // namespace ChessEngine
