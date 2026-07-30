#include <revolution/cx.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum CXDecodeResult {
    CX_DECODE_DONE,
    CX_DECODE_NEED_MORE,
    CX_DECODE_ERROR
} CXDecodeResult;

typedef struct CXStreamState {
    u8* source;
    size_t size;
    size_t capacity;
    u32 type;
} CXStreamState;

static u32 CXReadLittleEndian32(const u8* bytes) {
    return (u32)bytes[0] |
           ((u32)bytes[1] << 8) |
           ((u32)bytes[2] << 16) |
           ((u32)bytes[3] << 24);
}

static CXDecodeResult CXReadHeader(
    const u8* source,
    size_t sourceLength,
    u32 expectedType,
    u32* outputSize,
    size_t* headerSize) {
    u32 size;

    if (sourceLength < 4) {
        return CX_DECODE_NEED_MORE;
    }
    if ((source[0] & CX_COMPRESSION_TYPE_MASK) != expectedType) {
        return CX_DECODE_ERROR;
    }
    size = (u32)source[1] |
           ((u32)source[2] << 8) |
           ((u32)source[3] << 16);
    *headerSize = 4;
    if (size == 0) {
        if (sourceLength < 8) {
            return CX_DECODE_NEED_MORE;
        }
        size = CXReadLittleEndian32(source + 4);
        *headerSize = 8;
    }
    if (size == 0) {
        return CX_DECODE_ERROR;
    }
    *outputSize = size;
    return CX_DECODE_DONE;
}

static CXDecodeResult CXDecodeRL(
    const u8* source,
    size_t sourceLength,
    u8* destination) {
    u32 outputSize;
    size_t input;
    u32 output = 0;
    CXDecodeResult header =
        CXReadHeader(
            source,
            sourceLength,
            CX_COMPRESSION_RL,
            &outputSize,
            &input);
    if (header != CX_DECODE_DONE) {
        return header;
    }

    while (output < outputSize) {
        u8 control;
        u32 length;

        if (input >= sourceLength) {
            return CX_DECODE_NEED_MORE;
        }
        control = source[input++];
        length = (control & 0x7f) +
                 ((control & 0x80) != 0 ? 3u : 1u);
        if (length > outputSize - output) {
            length = outputSize - output;
        }

        if ((control & 0x80) != 0) {
            if (input >= sourceLength) {
                return CX_DECODE_NEED_MORE;
            }
            memset(destination + output, source[input++], length);
            output += length;
        } else {
            if (length > sourceLength - input) {
                return CX_DECODE_NEED_MORE;
            }
            memcpy(destination + output, source + input, length);
            input += length;
            output += length;
        }
    }
    return CX_DECODE_DONE;
}

static CXDecodeResult CXDecodeLZ(
    const u8* source,
    size_t sourceLength,
    u8* destination) {
    u32 outputSize;
    size_t input;
    u32 output = 0;
    const BOOL extended =
        sourceLength != 0 && (source[0] & 0x0f) != 0;
    CXDecodeResult header =
        CXReadHeader(
            source,
            sourceLength,
            CX_COMPRESSION_LZ,
            &outputSize,
            &input);
    if (header != CX_DECODE_DONE) {
        return header;
    }

    while (output < outputSize) {
        u8 flags;
        u32 token;

        if (input >= sourceLength) {
            return CX_DECODE_NEED_MORE;
        }
        flags = source[input++];
        for (token = 0; token < 8 && output < outputSize; ++token) {
            if ((flags & (0x80u >> token)) == 0) {
                if (input >= sourceLength) {
                    return CX_DECODE_NEED_MORE;
                }
                destination[output++] = source[input++];
            } else {
                u32 length;
                u32 offset;
                u8 first;

                if (input >= sourceLength) {
                    return CX_DECODE_NEED_MORE;
                }
                first = source[input];
                if (!extended) {
                    if (sourceLength - input < 2) {
                        return CX_DECODE_NEED_MORE;
                    }
                    length = (first >> 4) + 3;
                    offset =
                        (((u32)first & 0x0f) << 8) |
                        source[input + 1];
                    input += 2;
                } else if ((first >> 4) == 0) {
                    if (sourceLength - input < 3) {
                        return CX_DECODE_NEED_MORE;
                    }
                    length =
                        (((u32)first & 0x0f) << 4) |
                        (source[input + 1] >> 4);
                    length += 0x11;
                    offset =
                        (((u32)source[input + 1] & 0x0f) << 8) |
                        source[input + 2];
                    input += 3;
                } else if ((first >> 4) == 1) {
                    if (sourceLength - input < 4) {
                        return CX_DECODE_NEED_MORE;
                    }
                    length =
                        (((u32)first & 0x0f) << 12) |
                        ((u32)source[input + 1] << 4) |
                        (source[input + 2] >> 4);
                    length += 0x111;
                    offset =
                        (((u32)source[input + 2] & 0x0f) << 8) |
                        source[input + 3];
                    input += 4;
                } else {
                    if (sourceLength - input < 2) {
                        return CX_DECODE_NEED_MORE;
                    }
                    length = (first >> 4) + 1;
                    offset =
                        (((u32)first & 0x0f) << 8) |
                        source[input + 1];
                    input += 2;
                }

                ++offset;
                if (offset > output) {
                    return CX_DECODE_ERROR;
                }
                if (length > outputSize - output) {
                    length = outputSize - output;
                }
                while (length-- != 0) {
                    destination[output] =
                        destination[output - offset];
                    ++output;
                }
            }
        }
    }
    return CX_DECODE_DONE;
}

static CXDecodeResult CXDecodeHuffman(
    const u8* source,
    size_t sourceLength,
    u8* destination) {
    u32 outputSize;
    size_t headerSize;
    size_t treeSizePosition;
    size_t treeStart;
    size_t treeEnd;
    size_t bitPosition;
    size_t node;
    u32 output = 0;
    u32 symbolCount = 0;
    const u32 bitSize =
        sourceLength != 0 ? source[0] & 0x0f : 0;
    CXDecodeResult header =
        CXReadHeader(
            source,
            sourceLength,
            CX_COMPRESSION_HUFFMAN,
            &outputSize,
            &headerSize);
    if (header != CX_DECODE_DONE) {
        return header;
    }
    if (bitSize != 4 && bitSize != 8) {
        return CX_DECODE_ERROR;
    }
    if (headerSize >= sourceLength) {
        return CX_DECODE_NEED_MORE;
    }

    treeSizePosition = headerSize;
    treeStart = treeSizePosition + 1;
    treeEnd =
        treeSizePosition +
        ((size_t)source[treeSizePosition] + 1) * 2;
    if (treeEnd > sourceLength || treeStart >= treeEnd) {
        return CX_DECODE_NEED_MORE;
    }

    bitPosition = treeEnd * 8;
    node = treeStart;
    if (bitSize == 4) {
        memset(destination, 0, outputSize);
    }

    while (output < outputSize) {
        size_t child;
        u8 nodeValue;
        u32 branch;
        BOOL leaf;

        if (bitPosition / 8 >= sourceLength) {
            return CX_DECODE_NEED_MORE;
        }
        branch =
            (source[bitPosition / 8] >>
             (7 - (bitPosition & 7))) &
            1;
        ++bitPosition;

        if (node >= treeEnd) {
            return CX_DECODE_ERROR;
        }
        nodeValue = source[node];
        child =
            (node & ~(size_t)1) +
            (((size_t)(nodeValue & 0x3f) + 1) * 2) +
            branch;
        leaf = (nodeValue &
                (branch == 0 ? 0x80 : 0x40)) != 0;
        if (child >= treeEnd) {
            return CX_DECODE_ERROR;
        }

        if (leaf) {
            const u8 symbol = source[child];
            if (bitSize == 8) {
                destination[output++] = symbol;
            } else {
                if ((symbolCount & 1) == 0) {
                    destination[output] = symbol & 0x0f;
                } else {
                    destination[output] |=
                        (u8)((symbol & 0x0f) << 4);
                    ++output;
                }
                ++symbolCount;
            }
            node = treeStart;
        } else {
            node = child;
        }
    }
    return CX_DECODE_DONE;
}

static CXDecodeResult CXDecode(
    u32 type,
    const u8* source,
    size_t sourceLength,
    u8* destination) {
    switch (type) {
    case CX_COMPRESSION_RL:
        return CXDecodeRL(source, sourceLength, destination);
    case CX_COMPRESSION_LZ:
        return CXDecodeLZ(source, sourceLength, destination);
    case CX_COMPRESSION_HUFFMAN:
        return CXDecodeHuffman(source, sourceLength, destination);
    default:
        return CX_DECODE_ERROR;
    }
}

u32 CXGetUncompressedSize(const void* source) {
    const u8* bytes = (const u8*)source;
    u32 size;
    if (bytes == NULL) {
        return 0;
    }
    size = (u32)bytes[1] |
           ((u32)bytes[2] << 8) |
           ((u32)bytes[3] << 16);
    return size != 0 ? size : CXReadLittleEndian32(bytes + 4);
}

u32 CXGetCompressionType(const void* source) {
    return source != NULL
        ? (*(const u8*)source & CX_COMPRESSION_TYPE_MASK)
        : 0;
}

void CXUncompressAny(const void* source, void* destination) {
    if (source != NULL && destination != NULL) {
        CXDecode(
            CXGetCompressionType(source),
            (const u8*)source,
            SIZE_MAX,
            (u8*)destination);
    }
}

void CXUncompressRL(const void* source, void* destination) {
    if (source != NULL && destination != NULL) {
        CXDecodeRL(
            (const u8*)source,
            SIZE_MAX,
            (u8*)destination);
    }
}

void CXUncompressLZ(const void* source, void* destination) {
    if (source != NULL && destination != NULL) {
        CXDecodeLZ(
            (const u8*)source,
            SIZE_MAX,
            (u8*)destination);
    }
}

void CXUncompressHuffman(const void* source, void* destination) {
    if (source != NULL && destination != NULL) {
        CXDecodeHuffman(
            (const u8*)source,
            SIZE_MAX,
            (u8*)destination);
    }
}

static void CXInitStream(
    void* contextMemory,
    size_t contextSize,
    void* destination,
    u32 type) {
    CXUncompContextRL* context = (CXUncompContextRL*)contextMemory;
    CXStreamState* state;

    if (context == NULL) {
        return;
    }
    memset(context, 0, contextSize);
    context->destp = destination;
    state = (CXStreamState*)calloc(1, sizeof(*state));
    if (state == NULL) {
        context->error = -1;
        return;
    }
    state->type = type;
    context->hostData = state;
}

void CXInitUncompContextRL(
    CXUncompContextRL* context,
    void* destination) {
    CXInitStream(
        context,
        sizeof(*context),
        destination,
        CX_COMPRESSION_RL);
}

void CXInitUncompContextLZ(
    CXUncompContextLZ* context,
    void* destination) {
    CXInitStream(
        context,
        sizeof(*context),
        destination,
        CX_COMPRESSION_LZ);
}

void CXInitUncompContextHuffman(
    CXUncompContextHuffman* context,
    void* destination) {
    CXInitStream(
        context,
        sizeof(*context),
        destination,
        CX_COMPRESSION_HUFFMAN);
}

static s32 CXReadStream(
    CXUncompContextRL* context,
    const void* data,
    u32 length) {
    CXStreamState* state;
    size_t required;
    CXDecodeResult result;

    if (context == NULL ||
        context->error != 0 ||
        context->finished ||
        context->hostData == NULL ||
        data == NULL ||
        length == 0) {
        return -1;
    }
    state = (CXStreamState*)context->hostData;
    required = state->size + length;
    if (required > state->capacity) {
        size_t capacity = state->capacity != 0 ? state->capacity : 32;
        u8* resized;
        while (capacity < required) {
            if (capacity > SIZE_MAX / 2) {
                context->error = -1;
                return -1;
            }
            capacity *= 2;
        }
        resized = (u8*)realloc(state->source, capacity);
        if (resized == NULL) {
            context->error = -1;
            return -1;
        }
        state->source = resized;
        state->capacity = capacity;
    }
    memcpy(state->source + state->size, data, length);
    state->size += length;

    result = CXDecode(
        state->type,
        state->source,
        state->size,
        (u8*)context->destp);
    if (result == CX_DECODE_ERROR) {
        context->error = -1;
        free(state->source);
        free(state);
        context->hostData = NULL;
        return -1;
    }
    if (result == CX_DECODE_DONE) {
        context->finished = TRUE;
        free(state->source);
        free(state);
        context->hostData = NULL;
    }
    return (s32)length;
}

s32 CXReadUncompRL(
    CXUncompContextRL* context,
    const void* data,
    u32 length) {
    return CXReadStream(context, data, length);
}

s32 CXReadUncompLZ(
    CXUncompContextLZ* context,
    const void* data,
    u32 length) {
    return CXReadStream((CXUncompContextRL*)context, data, length);
}

s32 CXReadUncompHuffman(
    CXUncompContextHuffman* context,
    const void* data,
    u32 length) {
    return CXReadStream((CXUncompContextRL*)context, data, length);
}

BOOL CXIsFinishedUncompRL(const CXUncompContextRL* context) {
    return context != NULL && context->finished;
}

BOOL CXIsFinishedUncompLZ(const CXUncompContextLZ* context) {
    return context != NULL && context->finished;
}

BOOL CXIsFinishedUncompHuffman(
    const CXUncompContextHuffman* context) {
    return context != NULL && context->finished;
}
