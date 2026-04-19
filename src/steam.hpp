#pragma once

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
