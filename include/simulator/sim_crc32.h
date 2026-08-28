#ifndef CRC__H
#define CRC__H

#include <dolphin/types.h>
#include <stdlib.h>           /* For size_t                 */

#ifdef __cplusplus
extern "C" {
#endif

u32 SIM_updateCRC32(u8 ch, u32 crc);
u32 SIM_crc32buf(u8 *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CRC__H */