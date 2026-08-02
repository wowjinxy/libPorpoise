#ifndef SIM_DVD_H
#define SIM_DVD_H

#ifdef __cplusplus
namespace SIM::DVD {
void Init();
const char * GetRootDir();
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C APIs
void SIM_DVDInit();
const char * SIM_DVDGetRootPath();

#ifdef __cplusplus
}
#endif

#endif
