#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#ifdef LIBPORPOISE_BUILD_WIN
#include <libloaderapi.h>
#endif

#include <dolphin/types.h>

#include "simulator/sim_dvd.h"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;


static std::string s_dvdRootPath = "";
static std::unordered_map<std::string, s32> s_dvdEntrynumMap = {};
static std::vector<std::string> s_dvdEntrynumIndex = {};

static std::string GetExeDir() {
    char exeNameBuf[512] = {0};
    int bytes = 0;
    std::string ret = "";
#ifdef LIBPORPOISE_BUILD_LINUX
    bytes = std::min<int>(readlink("/proc/self/exe", exeNameBuf, 511), 511 - 1);
    if(bytes >= 0)
        exeNameBuf[bytes] = '\0';
    char * lastSlash = strrchr(exeNameBuf, '/');
#endif
#ifdef LIBPORPOISE_BUILD_WIN
    bytes = GetModuleFileName(NULL, exeNameBuf, 511);
    char * lastSlash = strrchr(exeNameBuf, '\\');
#endif
    
    *lastSlash = '\0';

    return std::string(exeNameBuf);
}


namespace SIM::DVD {
static constexpr auto RootPathName = "DVDRoot";

void Init() {
    auto exeDir = GetExeDir();
    s_dvdRootPath = exeDir + "/" + RootPathName;

    // Create DVDRoot if it does not exist
    if(!std::filesystem::exists(s_dvdRootPath)) {
        std::filesystem::create_directory(s_dvdRootPath);
    }

    // Build the entrynum index
    for (const auto& dirEntry : recursive_directory_iterator(s_dvdRootPath)) {
        auto entryNum = s_dvdEntrynumIndex.size();
        auto pathStr = dirEntry.path().string();
        
        s_dvdEntrynumMap.emplace(pathStr, static_cast<s32>(entryNum));
        s_dvdEntrynumIndex.push_back(pathStr);
    }

    // Change dir into current DVD Dir
    std::filesystem::current_path(s_dvdRootPath);
}

const char * GetRootPath() {
    return s_dvdRootPath.c_str();
}

s32 ConvertPathToEntrynum(const char * path) {
    if(s_dvdEntrynumMap.count(path)) {
        return s_dvdEntrynumMap[path];
    } else {
        return -1;
    }
}

std::string ConvertEntrynumToPath(size_t entrynum) {
    if(entrynum < s_dvdEntrynumIndex.size()) {
        return s_dvdEntrynumIndex[entrynum].c_str();
    } else {
        // File does not exist
        return "";
    }
}
}

// C APIs

void SIM_DVDInit() {
    SIM::DVD::Init();
}

const char * SIM_DVDGetRootPath() {
    return SIM::DVD::GetRootPath();
}

s32 SIM_DVDConvertPathToEntrynum(const char * path) {
    return SIM::DVD::ConvertPathToEntrynum(path);
}

void SIM_DVDConvertEntrynumToPath(s32 entrynum, char * buf, u32 bufLen) {
    size_t unsignedEntryNum = static_cast<size_t>(entrynum);
    strncpy(buf, SIM::DVD::ConvertEntrynumToPath(unsignedEntryNum).c_str(), bufLen-1);
}

u32 SIM_DVDGetMaxEntrynum() {
    return(static_cast<u32>(s_dvdEntrynumIndex.size()));
}