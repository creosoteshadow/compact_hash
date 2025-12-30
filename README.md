## compact_hash::CompactHash - Minimal, high-performance 64-bit non-cryptographic hash

## Features

- 100% header-only C++
- Extremely compact and auditable (~100 lines including comments)
- Fast (~8 GB/s on modern x86-64), highly readable, with wyhash-inspired quality and modern rrmxmx avalanche
- 128-bit internal state for excellent collision resistance
- Streaming interface with optional high-quality seeding (SplitMix64)
- Extendable output for multiple independent hashes from one input
- Fully portable across MSVC, GCC, and Clang on x86-64 and ARM64

## API
    compact_hash h(seed = 0);
    h.insert(data, size);
    uint64_t hash = h.finalize();

    // One-shot convenience function
    uint64_t compact_hash(const uint8_t* data, size_t size, uint64_t seed = 0);

    // Extended output: produce multiple 64 bit words from a single input.
    std::vector<uint64_t> compact_hash_extended(
        const uint8_t* data, size_t size, size_t nWords, uint64_t seed = 0)l

## Usage examples

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

## Credit

Compression function based on wyhash (public domain) by Wang Yi: https://github.com/wangyi-fudan/wyhash

Finalization mixers inspired by xxHash (public domain) by Yann Collet.
Reference: https://github.com/Cyan4973/xxHash/blob/dev/xxhash.h (search for rrmxmx or similar)


## License

This project is dedicated to the public domain under CC0 1.0 (see LICENSE).

As an optional fallback for users who prefer an explicit license, it is also available under the MIT License (see LICENSE-MIT).

You may choose to use the code under either CC0 1.0 **or** MIT — whichever better suits your needs. No attribution is required under either.
