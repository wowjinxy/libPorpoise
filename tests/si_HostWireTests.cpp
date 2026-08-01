#include <dolphin/si.h>

#include "../src/pad/PADWire.h"
#include "../src/si/SIWire.h"

#include <array>

int main()
{
    const std::array<u8, 7> guarded = {
        0xa5u, 0x12u, 0x34u, 0x56u, 0x78u, 0x9au, 0x5au};
    const u8* source = guarded.data() + 1;

    if (__SIPackWireWord(source, 1) != 0x12000000u ||
        __SIPackWireWord(source, 2) != 0x12340000u ||
        __SIPackWireWord(source, 3) != 0x12345600u ||
        __SIPackWireWord(source, 4) != 0x12345678u ||
        __SIPackWireWord(source + 4, 1) != 0x9a000000u) {
        return 1;
    }

    std::array<u8, 7> output = {};
    output.front() = 0xa5u;
    output.back() = 0x5au;
    __SIUnpackWireWord(output.data() + 1, 4, 0x12345678u);
    __SIUnpackWireWord(output.data() + 5, 1, 0x9abcdef0u);
    if (output != guarded) {
        return 2;
    }

    if (__SIBuildComcsr(0, 2, 3, 4, TRUE) != 0xc0030405u ||
        __SIBuildComcsr(0xffffffffu, 1, 128, 128, FALSE) !=
            0xbf8080fbu) {
        return 3;
    }

    SIComm flags = {};
    flags.flags.tcint = 1;
    flags.flags.tcintmsk = 1;
    flags.flags.outlngth = 3;
    flags.flags.inlngth = 4;
    flags.flags.channel = 2;
    flags.flags.tstart = 1;
    if (flags.val != 0xc0030405u) {
        return 4;
    }

    std::array<PADHostTransferStaging, 4> staging{};
    __PADStageScalarCommand(&staging[0], 0x4d812300u, 3);
    __PADStageScalarCommand(&staging[1], 0x41000000u, 1);
    if (staging[0].command[0] != 0x4du ||
        staging[0].command[1] != 0x81u ||
        staging[0].command[2] != 0x23u ||
        staging[1].command[0] != 0x41u) {
        return 5;
    }

    staging[2].response[0] = 0x20u;
    staging[2].response[1] = 0x80u;
    staging[2].response[2] = 0x7fu;
    staging[2].response[3] = 0x80u;
    staging[2].response[4] = 0x01u;
    staging[2].response[5] = 0xffu;
    staging[2].response[6] = 0x10u;
    staging[2].response[7] = 0x20u;
    staging[2].response[8] = 0x30u;
    staging[2].response[9] = 0x40u;
    PADStatus origin{};
    __PADCommitOriginResponse(&origin, &staging[2], 10);
    if (origin.button != 0x2080u || origin.stickX != 127 ||
        origin.stickY != -128 || origin.substickX != 1 ||
        origin.substickY != -1 || origin.triggerLeft != 0x10u ||
        origin.triggerRight != 0x20u || origin.analogA != 0x30u ||
        origin.analogB != 0x40u) {
        return 6;
    }

    staging[3].response[0] = 0x12u;
    staging[3].response[1] = 0x34u;
    staging[3].response[2] = 0x56u;
    if (__PADCommitScalarResponse(&staging[3], 3) != 0x12345600u) {
        return 7;
    }

    return 0;
}
