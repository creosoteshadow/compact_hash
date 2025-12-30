#pragma once
// File: compact_hash.h
// Description: Minimal, high-performace non-cryptographic hash function
// Author: Jim Staley
// License: Public Domain (CC0 1.0) with option MIT license fallback

#include <cstdint>      // uint64_t, uint8_t, size_t
#include <string.h>     // memcpy

#if defined(_MSC_VER)
#include <intrin.h>     // _umul128 on MSVC
#endif

#include <vector>       // std::vector for compact_hash_extended
#include "SplitMix64.h" // SplitMix64 RNG for extended output seeding

/*
compact_hash::CompactHash - Minimal, high-performance 64-bit non-cryptographic hash

Features:
    - Extremely compact and auditable (~100 lines, including comments)
    - wyhash-level speed and quality
    - 128 bit internal state for low collision rates
    - Streaming interface with optional seeding
    - Fully portable (MSVC, GCC, Clang on x86-64 and arm64)

API:
    compact_hash h(seed = 0);
    h.insert(data, size);
    uint64_t hash = h.finalize();

    // One-shot convenience function
    uint64_t compact_hash(const uint8_t* data, size_t size, uint64_t seed = 0);

    // Extended output: produce multiple 64 bit words from a single input.
    std::vector<uint64_t> compact_hash_extended(
        const uint8_t* data, size_t size, size_t nWords, uint64_t seed = 0)l

Usage examples:

    // Example 1 (streaming):
    compact_hash::CompactHash hasher(12345ULL);           // optional seed
    hasher.insert(reinterpret_cast<const uint8_t*>(data1), size1);
    uint64_t hash1 = hasher.finalize();

    // Example 2 (one-shot):
    uint64_t hashval = compact_hash::compact_hash(
    reinterpret_cast<const uint8_t*>(data), size, 12345ULL);

    // Example 3 (extended output):
    auto hashes = compact_hash::compact_hash_extended(
    reinterpret_cast<const uint8_t*>(data), size, 4, 12345ULL);

Compression function based on wyhash (public domain) by Wang Yi: https://github.com/wangyi-fudan/wyhash

Finalization mixers inspired by xxHash (public domain) by Yann Collet.
Reference: https://github.com/Cyan4973/xxHash/blob/dev/xxhash.h (search for rrmxmx or similar)

This implementation is dedicated to the public domain (CC0 1.0) with an
optional MIT license fallback. Use freely, no attribution required.
 */

namespace compact_hash {
    //
    // helper for older c++ versions
    //
    static inline uint64_t rotr(uint64_t x, unsigned r) noexcept { r &= 63; return (x >> r) | (x << (64 - r)); }
    static inline uint64_t rotl(uint64_t x, unsigned r) noexcept { r &= 63; return (x << r) | (x >> (64 - r)); }

    //
    // Provide _umul128 compatibility for non-MSVC platforms, matching the Windows intrinsic from <intrin.h>.
    //
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    // MSVC provides fast intrinsic on 64-bit targets
    // Declaration not needed — used directly
#elif defined(__SIZEOF_INT128__)
    inline uint64_t _umul128(uint64_t a, uint64_t b, uint64_t* hi) noexcept {
        __uint128_t product = static_cast<__uint128_t>(a) * b;
        *hi = product >> 64;
        return static_cast<uint64_t>(product);
    }
#else
#error "compact_hash requires 128-bit multiplication support: \
_umul128 on MSVC x64/ARM64 or __uint128_t on GCC/Clang"
#endif

    /////////////////////////////////////////////////////////////////////////////////////////////

    struct CompactHash {
        uint64_t state[2];      // 128 bit state. Dual lanes allow the CPU to process two blocks at once (ILP)
        uint64_t total_len = 0; // Tracks total bytes for length-dependent hashing

        // Initialize with SplitMix64 randomized state
        CompactHash(uint64_t seed = 0) noexcept {
            RNG::SplitMix64 gen(seed);
            state[0] = gen();
            state[1] = gen();
        }

        // raw byte insertion
        inline void insert(const uint8_t* p, size_t size) noexcept {
            total_len += size;
            // Main Loop: Process 16-byte chunks. ILP friendly.
            for (; size >= 16; size -= 16, p += 16) {
                uint64_t m[2];
                memcpy(m, p, 8);
                memcpy(m + 1, p + 8, 8);
                state[0] = compress(state[0], m[0]);
                state[1] = compress(state[1], m[1]);
            }
            // Tail: Handle remaining 1-15 bytes.
            if (size > 0) {
                uint64_t m[2] = { 0, 0 };
                memcpy(m, p, size);
                state[0] = compress(state[0], m[0]);
                state[1] = compress(state[1], m[1]);
            }
        }

        // finalize(), based on xxh3 finalization
        inline uint64_t finalize() noexcept {
            uint64_t h = compress(state[0], state[1]);

            // Strong avalanche inspired by Pelle Evensen's rrmxmx (used in XXH3)
            // Reference: https://github.com/Cyan4973/xxHash/blob/dev/xxhash.h (search for rrmxmx or similar)
            h ^= rotl(h, 49) ^ rotl(h, 24);
            h *= 0x9fb21c651e98df25ULL;  // PRIME_MX2 from current xxHash dev (odd prime close to 2^64)
            h ^= (h >> 35) ^ total_len;
            h *= 0x9fb21c651e98df25ULL;
            return h ^ (h >> 28);  // equivalent to xorshift64(h, 28)
        }

    private:
        // Strong 128→64 bit compression in "MUM" (Multiply-Unfold-Mix) style.
        // Constants and overall structure adapted from wyhash/wyrand by Wang Yi
        // (public domain: https://github.com/wangyi-fudan/wyhash).
        static inline uint64_t compress(uint64_t x, uint64_t y) noexcept {
            x = (x + y) * 0x2d358dccaa6c78a5ULL;
            uint64_t hi, lo;
            lo = _umul128(x, x ^ 0x8bb84b93962eacc9ULL, &hi);
            return x ^ 0x8bb84b93962eacc9ULL ^ lo ^ hi;
        }
    };//struct compact_hash 

    /////////////////////////////////////////////////////////////////////////////////////////////

    // One-shot convenience function
    uint64_t compact_hash(const uint8_t* data, size_t size, uint64_t seed = 0) noexcept {
        CompactHash h(seed);
        h.insert(data, size);
        return h.finalize();
    }//compact_hash

    // Extended output: produce multiple 64 bit words from a single input.
    // Uses SplitMix64 to generate high-quality independent seeds for each word.
    std::vector<uint64_t> compact_hash_extended(
        const uint8_t* data, size_t size, size_t nWords, uint64_t seed = 0) noexcept
    {
        // trivial exit
        if (nWords == 0) return {};

        // Create vector to hold results
        std::vector<uint64_t> result;
        result.reserve(nWords);

        RNG::SplitMix64 seed_generator(seed);  // fully deterministic, used for seeding the hash functions

        for (size_t i = 0; i < nWords; ++i) {
            CompactHash h(seed_generator()); // fresh, high-entropy seed
            h.insert(data, size);
            // Domain separation: ensure different indices produce unrelated outputs
            // even if the input data is identical across calls
            h.insert(reinterpret_cast<const uint8_t*>(&i), sizeof(i));  // domain separation
            result.push_back(h.finalize());
        }

        return result;
    }

}//namespace compact_hash 
