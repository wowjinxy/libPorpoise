#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <format>
#ifdef LIBPORPOISE_BUILD_WIN
#include <libloaderapi.h>
#endif
#include <stdio.h>

#include <dolphin/types.h>
#include <dolphin/card.h>
#include <dolphin/dvd.h>

#include "simulator/sim_card.hpp"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;


static std::string sCardRootBase = "";
static std::array<std::unordered_map<std::string, s32>, 2> sCardFilenumMaps = {};
static std::array<std::vector<std::string>, 2> sCardFilenumIndex = {};

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


namespace SIM::CARD {
static constexpr auto RootPathNameBase = "CARDRoot";

void Init() {
    auto exeDir = GetExeDir();
    std::replace(exeDir.begin(), exeDir.end(), '\\', '/');
    sCardRootBase = exeDir + "/" + RootPathNameBase;
}

void CreateRootPath(s32 channel) {
    auto rootPath = GetRootPath(channel);

    if(channel >= 2) {
        // Invalid card channel
        return;
    }

    // Create CARDRoot{channel} if it does not exist
    if(!std::filesystem::exists(rootPath)) {
        std::filesystem::create_directory(rootPath);
    }

    // Build the filenum index
    if(sCardFilenumMaps[channel].empty()) {
        for (const auto& dirEntry : recursive_directory_iterator(rootPath)) {
            auto fileNum = sCardFilenumIndex[channel].size();
            auto pathStr = dirEntry.path().string();
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

            sCardFilenumMaps[channel].emplace(pathStr, static_cast<s32>(fileNum));
            sCardFilenumIndex[channel].push_back(pathStr);
        }
    }
}

std::string GetRootPath(s32 channel) {
    return sCardRootBase + std::format("{}", channel);
}

s32 Open(s32 chan, std::string fileName, CARDFileInfo* fileInfo) {
    if(fileName.length() == 0) {
        return CARD_RESULT_NOFILE;
    }

    auto path = GetRootPath(chan) + "/" + fileName;
    fileInfo->pcFilePtr = fopen(path.c_str(), "rwb");
    if(!fileInfo->pcFilePtr) {
        return CARD_RESULT_NOFILE;
    }

    fseek(fileInfo->pcFilePtr, 0, SEEK_END);
    fileInfo->offset = 0x1337;
    fileInfo->length = ftell(fileInfo->pcFilePtr);
    fseek(fileInfo->pcFilePtr, 0, SEEK_SET);
    fileInfo->chan = chan;
    return CARD_RESULT_READY;
}

s32 Create(s32 chan, std::string fileName, u32 size, CARDFileInfo* fileInfo) {
    auto rootPath = GetRootPath(chan);

    auto filePath = rootPath + "/" + fileName;

    if(chan >= 2) {
        // Invalid card channel
        return CARD_RESULT_FATAL_ERROR;
    }

    if(std::filesystem::exists(filePath)) {
        return CARD_RESULT_EXIST;
    }

    if(size > 64 * 1024 * 1024) {
        //  64 Mb: This would be larger than most entire memory cards
        return CARD_RESULT_INSSPACE;
    }

    fileInfo->pcFilePtr = fopen(filePath.c_str(), "w");
    fseek(fileInfo->pcFilePtr, size, SEEK_SET);
    fputc('\0', fileInfo->pcFilePtr);
    fclose(fileInfo->pcFilePtr);


    // Register in filenum map
    auto fileNum = sCardFilenumIndex[chan].size();
    sCardFilenumMaps[chan].emplace(filePath, static_cast<s32>(fileNum));
    sCardFilenumIndex[chan].push_back(filePath);

    return Open(chan, fileName, fileInfo);
}

}

// C APIs
void SIM_CARDMount(s32 channel) {
    SIM::CARD::CreateRootPath(channel);
}

s32 SIM_CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
    return SIM::CARD::Open(chan, fileName, fileInfo);
}


s32 SIM_CARDFastOpen(s32 chan, s32 fileNum, CARDFileInfo* fileInfo) {
    if(chan >= 2) {
        // Invalid card channel
        return CARD_RESULT_FATAL_ERROR;
    }

    if(fileNum >= 0 && fileNum < sCardFilenumIndex[chan].size()) {
        return SIM::CARD::Open(chan, sCardFilenumIndex[chan][fileNum], fileInfo);
    } else {
        return CARD_RESULT_NOFILE;
    }
}

s32 SIM_CARDCreate(s32 chan, const char * fileName, u32 size, CARDFileInfo* fileInfo) {
    return SIM::CARD::Create(chan, fileName, size, fileInfo);
}

s32 SIM_CARDGetStatus(s32 channel, s32 fileNo, CARDStat* state) {
    if(fileNo < 0 || fileNo >= sCardFilenumIndex[channel].size()) {
        return CARD_RESULT_NOFILE;
    }

    std::strncpy(state->fileName, sCardFilenumIndex[channel][fileNo].c_str(), CARD_FILENAME_MAX-1);
    state->fileName[CARD_FILENAME_MAX-1] = 0;

    FILE * tmpFile = fopen(sCardFilenumIndex[channel][fileNo].c_str(), "rb");

    if(tmpFile) {
        fseek(tmpFile, 0, SEEK_END);
        state->length = ftell(tmpFile);
        fclose(tmpFile);
    }

    auto* dvdId = DVDGetCurrentDiskID();
    std::strncpy((char*)state->gameName, dvdId->gameName, sizeof(state->gameName));
    std::strncpy((char*)state->company, dvdId->company, sizeof(state->company));

    return CARD_RESULT_READY;
}