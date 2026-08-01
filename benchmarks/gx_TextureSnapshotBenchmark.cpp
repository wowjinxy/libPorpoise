#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

static size_t AllocationCalls = 0u;

void* operator new(size_t size) {
    ++AllocationCalls;
    if (void* allocation = std::malloc(size != 0u ? size : 1u)) {
        return allocation;
    }
    throw std::bad_alloc();
}

void* operator new[](size_t size) {
    ++AllocationCalls;
    if (void* allocation = std::malloc(size != 0u ? size : 1u)) {
        return allocation;
    }
    throw std::bad_alloc();
}

void operator delete(void* allocation) noexcept {
    std::free(allocation);
}

void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}

void operator delete(void* allocation, size_t) noexcept {
    std::free(allocation);
}

void operator delete[](void* allocation, size_t) noexcept {
    std::free(allocation);
}

namespace {

size_t ParseIterations(const char* text) {
    if (text == nullptr || *text == '\0') {
        return 64u;
    }
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    return end != text && *end == '\0' && value != 0u
        ? static_cast<size_t>(value)
        : 64u;
}

}

int main(int argc, char** argv) {
    const size_t iterations =
        ParseIterations(argc > 1 ? argv[1] : nullptr);

    SIM::GX::TextureState texture;
    texture.width = 1024u;
    texture.height = 1024u;
    texture.format = GX_TF_RGBA8;
    const size_t byteSize = SIM::GX::GetTextureSourceByteSize(texture);
    std::vector<u8> source(byteSize);
    for (size_t index = 0u; index < source.size(); ++index) {
        source[index] = static_cast<u8>(index * 131u + 17u);
    }
    texture.data = source.data();

    SIM::GX::TextureContentSnapshot snapshot;
    snapshot.Capture(texture);
    if (!snapshot.Matches(texture)) {
        std::fprintf(stderr, "initial texture snapshot did not match\n");
        return 1;
    }

    bool allMatched = true;
    const size_t canonicalAllocationsBefore = AllocationCalls;
    const auto start = std::chrono::steady_clock::now();
    for (size_t iteration = 0u; iteration < iterations; ++iteration) {
        allMatched = snapshot.Matches(texture) && allMatched;
    }
    const auto end = std::chrono::steady_clock::now();
    const size_t canonicalAllocations =
        AllocationCalls - canonicalAllocationsBefore;
    if (!allMatched || canonicalAllocations != 0u) {
        std::fprintf(
            stderr,
            "canonical snapshot match failed or allocated (%zu calls)\n",
            canonicalAllocations);
        return 2;
    }

    source.back() ^= 1u;
    if (snapshot.Matches(texture)) {
        std::fprintf(stderr, "texture snapshot missed a source change\n");
        return 3;
    }

    // Restore the canonical source before constructing its host-native form.
    source.back() ^= 1u;
    std::vector<u16> nativeSource(byteSize / sizeof(u16));
    for (size_t word = 0u; word < nativeSource.size(); ++word) {
        nativeSource[word] = static_cast<u16>(
            (static_cast<u16>(source[word * 2u]) << 8u) |
            static_cast<u16>(source[word * 2u + 1u]));
    }
    SIM::GX::TextureState nativeTexture = texture;
    nativeTexture.data = nativeSource.data();
    nativeTexture.sourceEncoding =
        SIM::GX::TextureState::SourceEncoding::NativeU16;
    SIM::GX::TextureContentSnapshot nativeSnapshot;
    nativeSnapshot.Capture(nativeTexture);
    if (!nativeSnapshot.Matches(texture)) {
        std::fprintf(stderr, "native snapshot did not match canonical data\n");
        return 4;
    }

    const size_t nativeAllocationsBefore = AllocationCalls;
    bool allNativeMatched = true;
    const auto nativeStart = std::chrono::steady_clock::now();
    for (size_t iteration = 0u; iteration < iterations; ++iteration) {
        allNativeMatched =
            nativeSnapshot.Matches(nativeTexture) && allNativeMatched;
    }
    const auto nativeEnd = std::chrono::steady_clock::now();
    const size_t nativeAllocations =
        AllocationCalls - nativeAllocationsBefore;
    if (!allNativeMatched || nativeAllocations != 0u) {
        std::fprintf(
            stderr,
            "native snapshot match failed or allocated (%zu calls)\n",
            nativeAllocations);
        return 5;
    }
    nativeSource.back() ^= 1u;
    if (nativeSnapshot.Matches(nativeTexture)) {
        std::fprintf(stderr, "native snapshot missed a source change\n");
        return 6;
    }

    const double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    const double nativeElapsedMs =
        std::chrono::duration<double, std::milli>(
            nativeEnd - nativeStart).count();
    const double bytesCompared =
        static_cast<double>(byteSize) * static_cast<double>(iterations);
    const double gibPerSecond =
        bytesCompared / (1024.0 * 1024.0 * 1024.0) /
        (elapsedMs / 1000.0);
    std::printf(
        "bytes=%zu iterations=%zu canonical_us=%.3f canonical_gib_s=%.3f "
        "canonical_allocations=%zu native_us=%.3f native_gib_s=%.3f "
        "native_allocations=%zu\n",
        byteSize,
        iterations,
        elapsedMs * 1000.0 / static_cast<double>(iterations),
        gibPerSecond,
        canonicalAllocations,
        nativeElapsedMs * 1000.0 / static_cast<double>(iterations),
        bytesCompared / (1024.0 * 1024.0 * 1024.0) /
            (nativeElapsedMs / 1000.0),
        nativeAllocations);
    return 0;
}
