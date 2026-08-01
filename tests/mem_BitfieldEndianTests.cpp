#include <revolution/mem.h>

int main() {
    MEMiHeapHead heap = {};
    heap.attribute.fields.optFlag = 0xA5U;
    if (heap.attribute.val != 0x000000A5U) {
        return 1;
    }
    heap.attribute.val = 0x1234565AU;
    if (heap.attribute.fields.optFlag != 0x5AU) {
        return 2;
    }

    MEMiExpHeapMBlockHead block = {};
    block.attribute.fields.allocDir = 1;
    block.attribute.fields.alignment = 0x35U;
    block.attribute.fields.groupID = 0xA7U;
    if (block.attribute.val != 0xB5A7U) {
        return 3;
    }
    block.attribute.val = 0xD2ABU;
    if (block.attribute.fields.allocDir != 1U ||
        block.attribute.fields.alignment != 0x52U ||
        block.attribute.fields.groupID != 0xABU) {
        return 4;
    }

    MEMiExpHeapHead expHeap = {};
    expHeap.feature.fields.useMarginOfAlign = 1;
    expHeap.feature.fields.allocMode = 1;
    if (expHeap.feature.val != 0x0003U) {
        return 5;
    }
    expHeap.feature.val = 0xFFFEU;
    if (expHeap.feature.fields.useMarginOfAlign != 1U ||
        expHeap.feature.fields.allocMode != 0U) {
        return 6;
    }

    return 0;
}
