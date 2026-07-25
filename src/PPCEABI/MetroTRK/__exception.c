#include <dolphin/types.h>

u8 gTRKInterruptVectorTable[] = {
    0x00, 0x00, 0x00, 0x00
};
u8 gTRKInterruptVectorTableEnd[] = {
    0x00
};

//TODO: The actual interrupt vector table is an assembly file. it is the ONLY assembly file in this entire lib.
//Ideally, if we somehow get it in a C file, we can avoid bringing in 'nasm' as a required language for the project
// Currently, the interrupt vector table is missing completely.