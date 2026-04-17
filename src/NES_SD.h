#ifndef SD_TYPE
#define SD_SD 1
#define SD_LFS 2

#define SD_TYPE SD_LFS

#if SD_TYPE == SD_LFS
#include <LittleFS.h>
#define SD LittleFS
#else
#include <SD.h>
#endif
#endif