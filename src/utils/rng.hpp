#pragma once

#include <cstdint>
#include <cstddef>

namespace ChessEngine {

// Set the global PRNG seed for deterministic execution
void set_global_seed(uint64_t seed);

// Get the current global PRNG seed
uint64_t get_global_seed();

// Generate a 64-bit pseudo-random integer
uint64_t rand_u64();

// Generate a 32-bit pseudo-random integer
uint32_t rand_u32();

// Generate a random integer in inclusive range [min, max]
uint32_t rand_range(uint32_t min, uint32_t max);

// Generate a random index in range [0, size - 1]
size_t rand_index(size_t size);

// Generate a floating point number in [0.0, 1.0)
double rand_double();

} // namespace ChessEngine
