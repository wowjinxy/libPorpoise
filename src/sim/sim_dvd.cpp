#include <algorithm>
#include <cstring>
#include <string>
#include <unistd.h>
#ifdef LIBPORPOISE_BUILD_WIN64
#include <libloaderapi.h>
#endif
#include <cstdio>

#include <dolphin/types.h>

#include "simulator/sim_dvd.h"

static std::string s_dvdRootPath = "";


static std::string GetExeDir() {
    char exeNameBuf[256] = {0};
    int bytes = 0;
    std::string ret = "";
#ifdef LIBPORPOISE_BUILD_LINUX
    bytes = std::min<int>(readlink("/proc/self/exe", exeNameBuf, 255), 255 - 1);
    if(bytes >= 0)
        exeNameBuf[bytes] = '\0';
#endif
#ifdef LIBPORPOISE_BUILD_WIN64
    bytes = GetModuleFileName(NULL, exeNameBuf, 255);
#endif
    char * lastSlash = strrchr(exeNameBuf, '/');
    *lastSlash = '\0';

    return std::string(exeNameBuf);
}


namespace SIM::DVD {
static constexpr auto RootPathName = "DVDRoot";

void Init() {
    auto exeDir = GetExeDir();
    s_dvdRootPath = exeDir + "/" + RootPathName;
}

const char * GetRootPath() {
    return s_dvdRootPath.c_str();
}
}

// C APIs

void SIM_DVDInit() {
    SIM::DVD::Init();
}

const char * SIM_DVDGetRootPath() {
    return SIM::DVD::GetRootPath();
}