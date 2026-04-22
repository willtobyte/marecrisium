using SteamAPI_InitSafe_t = bool(S_CALLTYPE*)();
using SteamAPI_Shutdown_t = void(S_CALLTYPE*)();
using SteamAPI_RunCallbacks_t = void(S_CALLTYPE*)();
using SteamUserStats_t = void*(S_CALLTYPE*)();
using GetAchievement_t = bool(S_CALLTYPE*)(void*, const char*, bool*);
using SetAchievement_t = bool(S_CALLTYPE*)(void*, const char*);
using StoreStats_t = bool(S_CALLTYPE*)(void*);
using SteamFriends_t = void*(S_CALLTYPE*)();
using GetPersonaName_t = const char*(S_CALLTYPE*)(void*);
using GetFriendCount_t = int(S_CALLTYPE*)(void*, int);
using GetFriendByIndex_t = uint64_t(S_CALLTYPE*)(void*, int, int);
using GetFriendPersonaName_t = const char*(S_CALLTYPE*)(void*, uint64_t);

static DYNLIB_HANDLE hSteamApi = DYNLIB_LOAD(STEAM_LIB_NAME);

#define LOAD_SYMBOL(name, sym) reinterpret_cast<name>(reinterpret_cast<void*>(DYNLIB_SYM(hSteamApi, sym)))

static const auto pSteamAPI_InitSafe = LOAD_SYMBOL(SteamAPI_InitSafe_t, "SteamAPI_InitSafe");
static const auto pSteamAPI_Shutdown = LOAD_SYMBOL(SteamAPI_Shutdown_t, "SteamAPI_Shutdown");
static const auto pSteamAPI_RunCallbacks = LOAD_SYMBOL(SteamAPI_RunCallbacks_t, "SteamAPI_RunCallbacks");
static const auto pSteamUserStats = LOAD_SYMBOL(SteamUserStats_t, "SteamAPI_SteamUserStats_v013");
static const auto pGetAchievement = LOAD_SYMBOL(GetAchievement_t, "SteamAPI_ISteamUserStats_GetAchievement");
static const auto pSetAchievement = LOAD_SYMBOL(SetAchievement_t, "SteamAPI_ISteamUserStats_SetAchievement");
static const auto pStoreStats = LOAD_SYMBOL(StoreStats_t, "SteamAPI_ISteamUserStats_StoreStats");
static const auto pSteamFriends = LOAD_SYMBOL(SteamFriends_t, "SteamAPI_SteamFriends_v018");
static const auto pGetPersonaName = LOAD_SYMBOL(GetPersonaName_t, "SteamAPI_ISteamFriends_GetPersonaName");
static const auto pGetFriendCount = LOAD_SYMBOL(GetFriendCount_t, "SteamAPI_ISteamFriends_GetFriendCount");
static const auto pGetFriendByIndex = LOAD_SYMBOL(GetFriendByIndex_t, "SteamAPI_ISteamFriends_GetFriendByIndex");
static const auto pGetFriendPersonaName = LOAD_SYMBOL(GetFriendPersonaName_t, "SteamAPI_ISteamFriends_GetFriendPersonaName");

bool SteamAPI_InitSafe() {
  if (pSteamAPI_InitSafe) {
    return pSteamAPI_InitSafe();
  }

  return false;
}

void SteamAPI_Shutdown() {
  if (pSteamAPI_Shutdown) {
    pSteamAPI_Shutdown();
  }
}

void SteamAPI_RunCallbacks() {
  if (pSteamAPI_RunCallbacks) {
    pSteamAPI_RunCallbacks();
  }
}

void* SteamUserStats() {
  if (pSteamUserStats) [[likely]]
    return pSteamUserStats();

  return nullptr;
}

void* SteamFriends() {
  if (pSteamFriends) [[likely]]
    return pSteamFriends();

  return nullptr;
}

bool GetAchievement(const char* name) {
  if (pGetAchievement) [[likely]] {
    if (auto stats = SteamUserStats()) [[likely]] {
      bool achieved = false;
      return pGetAchievement(stats, name, &achieved) && achieved;
    }
  }

  return false;
}

bool SetAchievement(const char* name) {
  if (pSetAchievement) [[likely]]
    if (auto stats = SteamUserStats()) [[likely]]
      return pSetAchievement(stats, name);

  return false;
}

bool StoreStats() {
  if (pStoreStats) [[likely]]
    if (auto stats = SteamUserStats()) [[likely]]
      return pStoreStats(stats);

  return false;
}

const char* GetPersonaName() {
  if (pGetPersonaName) [[likely]]
    if (auto friends = SteamFriends()) [[likely]]
      return pGetPersonaName(friends);

  return "";
}

int GetFriendCount() {
  if (pGetFriendCount) [[likely]]
    if (auto friends = SteamFriends()) [[likely]]
      // k_EFriendFlagImmediate = 0x04
      return pGetFriendCount(friends, 0x04);

  return 0;
}

uint64_t GetFriendByIndex(int index) {
  if (pGetFriendByIndex) [[likely]]
    if (auto friends = SteamFriends()) [[likely]]
      return pGetFriendByIndex(friends, index, 0x04);

  return 0;
}

const char* GetFriendPersonaName(uint64_t id) {
  if (pGetFriendPersonaName) [[likely]]
    if (auto friends = SteamFriends()) [[likely]]
      return pGetFriendPersonaName(friends, id);

  return "";
}
