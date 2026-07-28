#ifndef REVOLUTION_CX_H
#define REVOLUTION_CX_H

#include <revolution/types.h>

BEGIN_SCOPE_EXTERN_C

#define CX_COMPRESSION_LZ 0x10
#define CX_COMPRESSION_HUFFMAN 0x20
#define CX_COMPRESSION_RL 0x30
#define CX_COMPRESSION_DIFF 0x80
#define CX_COMPRESSION_TYPE_MASK 0xf0

typedef struct CXUncompContextRL {
    void* destp;
    void* hostData;
    BOOL finished;
    s32 error;
} CXUncompContextRL;

typedef struct CXUncompContextLZ {
    void* destp;
    void* hostData;
    BOOL finished;
    s32 error;
} CXUncompContextLZ;

typedef struct CXUncompContextHuffman {
    void* destp;
    void* hostData;
    BOOL finished;
    s32 error;
} CXUncompContextHuffman;

u32 CXGetUncompressedSize(const void* source);
u32 CXGetCompressionType(const void* source);
void CXUncompressAny(const void* source, void* destination);
void CXUncompressRL(const void* source, void* destination);
void CXUncompressLZ(const void* source, void* destination);
void CXUncompressHuffman(const void* source, void* destination);

void CXInitUncompContextRL(
    CXUncompContextRL* context,
    void* destination);
void CXInitUncompContextLZ(
    CXUncompContextLZ* context,
    void* destination);
void CXInitUncompContextHuffman(
    CXUncompContextHuffman* context,
    void* destination);
s32 CXReadUncompRL(
    CXUncompContextRL* context,
    const void* data,
    u32 length);
s32 CXReadUncompLZ(
    CXUncompContextLZ* context,
    const void* data,
    u32 length);
s32 CXReadUncompHuffman(
    CXUncompContextHuffman* context,
    const void* data,
    u32 length);
BOOL CXIsFinishedUncompRL(const CXUncompContextRL* context);
BOOL CXIsFinishedUncompLZ(const CXUncompContextLZ* context);
BOOL CXIsFinishedUncompHuffman(
    const CXUncompContextHuffman* context);

END_SCOPE_EXTERN_C

#endif /* REVOLUTION_CX_H */
