#pragma once

#if defined(_WIN32)
  #include <windows.h>
  #define DYNLIB_HANDLE HMODULE
  #define DYNLIB_LOAD(name) LoadLibraryA(name)
  #define DYNLIB_SYM(lib, name) GetProcAddress(lib, name)
  #define STEAM_LIB_NAME "steam_api64.dll"
#elif defined(__APPLE__)
  #include <dlfcn.h>
  #define DYNLIB_HANDLE void*
  #define DYNLIB_LOAD(name) dlopen(name, RTLD_LAZY)
  #define DYNLIB_SYM(lib, name) dlsym(lib, name)
  #define STEAM_LIB_NAME "libsteam_api.dylib"
#endif

#ifndef S_CALLTYPE
  #define S_CALLTYPE __cdecl
#endif

bool SteamAPI_InitSafe();
void SteamAPI_Shutdown();
void SteamAPI_RunCallbacks();
void* SteamUserStats();
bool GetAchievement(const char* name);
bool SetAchievement(const char* name);
bool StoreStats();
void* SteamFriends();
const char* GetPersonaName();
int GetFriendCount();
uint64_t GetFriendByIndex(int index);
const char* GetFriendPersonaName(uint64_t id);
