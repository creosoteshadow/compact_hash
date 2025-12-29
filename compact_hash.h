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

Based on wyhash (public domain) by Wang Yi: https://github.com/wangyi-fudan/wyhash

This implementation is dedicated to the public domain (CC0 1.0) with an
optional MIT license fallback. Use freely, no attribution required.
 */

namespace compact_hash {
    //
    // helper for older c++ versions
    //
    uint64_t rotr(uint64_t x, unsigned r) { return (x >> r) | (x >> (64 - r)); }

    //
    // Provide _umul128 compatibility for non-MSVC platforms, matching the Windows intrinsic from <intrin.h>.
    //
#if !defined(_MSC_VER) && defined(__SIZEOF_INT128__)
    inline std::uint64_t _umul128(std::uint64_t a, std::uint64_t b, std::uint64_t* hi) {
        __uint128_t product = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
        std::uint64_t lo = static_cast<std::uint64_t>(product);
        *hi = static_cast<std::uint64_t>(product >> 64);
        return lo;
    }
#elif defined(_MSC_VER)
    // For MSVC, just use the intrinsic directly.
#else
#error "compact_hash requires a 64-bit platform with 128-bit integer support. \
Supported compilers: MSVC (x64/arm64), GCC/Clang with __uint128_t."
#endif


    /////////////////////////////////////////////////////////////////////////////////////////////

    struct CompactHash {
        uint64_t state[2];      // 128 bit state. Dual lanes allow the CPU to process two blocks at once (ILP)
        uint64_t total_len = 0; // Tracks total bytes for length-dependent hashing

        // Initialize with large, asymmetrical constants to break symmetry early
        CompactHash(uint64_t seed = 0)noexcept
            : state{ seed ^ 0x9e3779b97f4a7c15ULL, rotr(seed, 9) * 0xC2B2AE3D27D4EB4FULL } {
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

        // finalize is based on xxh64 finalization
        inline uint64_t finalize() noexcept {
            // 1. Merge the two 64-bit lanes into one
            uint64_t h = compress(state[0], state[1]);
            // 2. Mix in total length to ensure hash("A") != hash("A\0")
            h ^= total_len * 0x9e3779b97f4a7c15ULL;
            // 3. Final Avalanche: Ensure every input bit affects every output bit.
            // Constants from XXH64 finalization.
            h = (h ^ (h >> 33)) * 0xC2B2AE3D27D4EB4FULL;
            h = (h ^ (h >> 29)) * 0x165667B19E3779F9ULL;
            return h ^ (h >> 32);
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
    std::vector<uint64_t> compact_hash_extended(
        const uint8_t* data, size_t size, size_t nWords, uint64_t seed = 0) noexcept {
        if (nWords == 0) return {};

        std::vector<uint64_t> result(nWords);

        CompactHash h(seed);
        h.insert(data, size);
        result[0] = h.finalize();

        for (size_t i = 1; i < nWords; ++i) {
            // Mix in counter and previous output to derive next
            h.insert(reinterpret_cast<const uint8_t*>(&i), sizeof(i));
            h.insert(reinterpret_cast<const uint8_t*>(&result[i - 1]), sizeof(result[i - 1]));
            result[i] = h.finalize();
        }

        return result;
    }//compact_hash_extended

}//namespace compact_hash 
