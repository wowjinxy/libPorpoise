#include <revolution/cx.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <vector>

#ifndef SDK_CX_DEMO_CASE
#error SDK_CX_DEMO_CASE must identify the SDK demo inventory sequence
#endif

namespace {

using Bytes = std::vector<u8>;

constexpr std::array<u8, 8> Expected{
    'A', 'B', 'B', 'A', 'A', 'B', 'A', 'B'};

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

Bytes Header(u8 type) {
    return {
        type,
        static_cast<u8>(Expected.size()),
        0,
        0,
    };
}

Bytes MakeRL() {
    Bytes result = Header(CX_COMPRESSION_RL);
    result.push_back(static_cast<u8>(Expected.size() - 1));
    result.insert(result.end(), Expected.begin(), Expected.end());
    return result;
}

Bytes MakeLZ() {
    Bytes result = Header(CX_COMPRESSION_LZ);
    result.push_back(0);
    result.insert(result.end(), Expected.begin(), Expected.end());
    return result;
}

Bytes MakeHuffman() {
    Bytes result = Header(CX_COMPRESSION_HUFFMAN | 8);
    result.insert(
        result.end(),
        {
            1,
            0xc0,
            'A',
            'B',
            0x65,
        });
    return result;
}

bool Matches(const std::array<u8, Expected.size()>& output) {
    return std::equal(output.begin(), output.end(), Expected.begin());
}

bool TestDirectDecompression() {
    const Bytes runLength = MakeRL();
    const Bytes lz = MakeLZ();
    const Bytes huffman = MakeHuffman();
    std::array<u8, Expected.size()> rlOutput{};
    std::array<u8, Expected.size()> lzOutput{};
    std::array<u8, Expected.size()> huffmanOutput{};
    std::array<u8, Expected.size()> anyOutput{};

    CXUncompressRL(runLength.data(), rlOutput.data());
    CXUncompressLZ(lz.data(), lzOutput.data());
    CXUncompressHuffman(huffman.data(), huffmanOutput.data());
    CXUncompressAny(lz.data(), anyOutput.data());

    return
        Require(
            CXGetUncompressedSize(runLength.data()) == Expected.size() &&
                CXGetUncompressedSize(lz.data()) == Expected.size() &&
                CXGetUncompressedSize(huffman.data()) == Expected.size(),
            "CX header size decoding failed") &&
        Require(
            CXGetCompressionType(runLength.data()) == CX_COMPRESSION_RL &&
                CXGetCompressionType(lz.data()) == CX_COMPRESSION_LZ &&
                CXGetCompressionType(huffman.data()) ==
                    CX_COMPRESSION_HUFFMAN,
            "CX compression type decoding failed") &&
        Require(Matches(rlOutput), "run-length output differs from raw data") &&
        Require(Matches(lzOutput), "LZ output differs from raw data") &&
        Require(
            Matches(huffmanOutput),
            "Huffman output differs from raw data") &&
        Require(
            Matches(anyOutput),
            "CXUncompressAny did not dispatch to LZ");
}

template <typename Context, typename Init, typename Read, typename Finished>
bool StreamDecode(
    const Bytes& compressed,
    Init init,
    Read read,
    Finished finished,
    const char* label) {
    std::array<u8, Expected.size()> output{};
    Context context{};
    const std::array<std::size_t, 5> chunkPattern{1, 2, 1, 3, 2};
    std::size_t offset = 0;
    std::size_t chunkIndex = 0;

    init(&context, output.data());
    while (offset < compressed.size()) {
        const std::size_t amount = std::min(
            chunkPattern[chunkIndex++ % chunkPattern.size()],
            compressed.size() - offset);
        if (!Require(
                read(
                    &context,
                    compressed.data() + offset,
                    static_cast<u32>(amount)) ==
                    static_cast<s32>(amount),
                label)) {
            return false;
        }
        offset += amount;
        if (offset < compressed.size() &&
            !Require(!finished(&context), "CX stream finished too early")) {
            return false;
        }
    }
    return
        Require(finished(&context), "CX stream did not finish") &&
        Require(Matches(output), label);
}

bool TestStreamingDecompression() {
    return
        StreamDecode<CXUncompContextRL>(
            MakeRL(),
            CXInitUncompContextRL,
            CXReadUncompRL,
            CXIsFinishedUncompRL,
            "streaming RL output differs from direct output") &&
        StreamDecode<CXUncompContextLZ>(
            MakeLZ(),
            CXInitUncompContextLZ,
            CXReadUncompLZ,
            CXIsFinishedUncompLZ,
            "streaming LZ output differs from direct output") &&
        StreamDecode<CXUncompContextHuffman>(
            MakeHuffman(),
            CXInitUncompContextHuffman,
            CXReadUncompHuffman,
            CXIsFinishedUncompHuffman,
            "streaming Huffman output differs from direct output");
}

}  // namespace

int main() {
#if SDK_CX_DEMO_CASE == 26
    return TestDirectDecompression() ? 0 : 1;
#elif SDK_CX_DEMO_CASE == 27
    return TestStreamingDecompression() ? 0 : 1;
#else
#error Unsupported SDK_CX_DEMO_CASE
#endif
}
