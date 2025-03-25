// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC

#include "OnlinePlatformSteam.h"
#include "Engine/Content/Content.h"
#include "Engine/Content/JsonAsset.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Types/TimeSpan.h"
#include "Engine/Core/Config/GameSettings.h"
#include "Engine/Core/Collections/Array.h"
#include "Engine/Engine/Engine.h"
#include "Engine/Utilities/StringConverter.h"
#include "Engine/Profiler/ProfilerCPU.h"
#if USE_EDITOR
#include "Engine/Engine/Globals.h"
#include "Engine/Platform/FileSystem.h"
#include "Engine/Platform/File.h"
#endif
#include <Steamworks/steam_api.h>

IMPLEMENT_GAME_SETTINGS_GETTER(SteamSettings, "Steam");

extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char* pchDebugText)
{
    switch (nSeverity)
    {
    case 0:
        LOG(Info, "[Steam] ", String(pchDebugText));
        break;
    default:
        LOG(Warning, "[Steam] ", String(pchDebugText));
        break;
    }
}

DateTime DateTimeFromUnixTimestamp(int32 unixTime)
{
    return DateTime(1970, 1, 1) + TimeSpan(static_cast<int64>(unixTime) * TimeSpan::TicksPerSecond);
}

Guid GetUserId(CSteamID id)
{
    // Steam uses 64 bits, Guid is 128 bits
    const uint64 data[2] = { id.ConvertToUint64(), 0 };
    return *(Guid*)data;
}

CSteamID GetSteamId(const Guid& id)
{
    // Steam uses 64 bits, Guid is 128 bits
    CSteamID result;
    result.SetFromUint64(*(uint64*)&id);
    return result;
}

OnlinePresenceStates GetUserPresence(EPersonaState state)
{
    switch (state)
    {
    case k_EPersonaStateOffline:
    case k_EPersonaStateInvisible:
        return OnlinePresenceStates::Offline;
    case k_EPersonaStateOnline:
    case k_EPersonaStateLookingToTrade:
    case k_EPersonaStateLookingToPlay:
        return OnlinePresenceStates::Online;
    case k_EPersonaStateBusy:
        return OnlinePresenceStates::Busy;
    case k_EPersonaStateAway:
    case k_EPersonaStateSnooze:
        return OnlinePresenceStates::Away;
    default:
        return OnlinePresenceStates::Online;
    }
}

OnlineLeaderboardSortModes GetLeaderboardSortMode(ELeaderboardSortMethod value)
{
    switch (value)
    {
    case k_ELeaderboardSortMethodNone:
        return OnlineLeaderboardSortModes::None;
    case k_ELeaderboardSortMethodAscending:
        return OnlineLeaderboardSortModes::Ascending;
    case k_ELeaderboardSortMethodDescending:
        return OnlineLeaderboardSortModes::Descending;
    }
    return OnlineLeaderboardSortModes::None;
}

ELeaderboardSortMethod GetLeaderboardSortMode(OnlineLeaderboardSortModes value)
{
    switch (value)
    {
    case OnlineLeaderboardSortModes::None:
        return k_ELeaderboardSortMethodNone;
    case OnlineLeaderboardSortModes::Ascending:
        return k_ELeaderboardSortMethodAscending;
    case OnlineLeaderboardSortModes::Descending:
        return k_ELeaderboardSortMethodDescending;
    }
    return k_ELeaderboardSortMethodNone;
}

OnlineLeaderboardValueFormats GetLeaderboardValueFormat(ELeaderboardDisplayType value)
{
    switch (value)
    {
    case k_ELeaderboardDisplayTypeNone:
        return OnlineLeaderboardValueFormats::Undefined;
    case k_ELeaderboardDisplayTypeNumeric:
        return OnlineLeaderboardValueFormats::Numeric;
    case k_ELeaderboardDisplayTypeTimeSeconds:
        return OnlineLeaderboardValueFormats::Seconds;
    case k_ELeaderboardDisplayTypeTimeMilliSeconds:
        return OnlineLeaderboardValueFormats::Milliseconds;
    }
    return OnlineLeaderboardValueFormats::Undefined;
}

ELeaderboardDisplayType GetLeaderboardValueFormat(OnlineLeaderboardValueFormats value)
{
    switch (value)
    {
    case OnlineLeaderboardValueFormats::Undefined:
        return k_ELeaderboardDisplayTypeNone;
    case OnlineLeaderboardValueFormats::Numeric:
        return k_ELeaderboardDisplayTypeNumeric;
    case OnlineLeaderboardValueFormats::Seconds:
        return k_ELeaderboardDisplayTypeTimeSeconds;
    case OnlineLeaderboardValueFormats::Milliseconds:
        return k_ELeaderboardDisplayTypeTimeMilliSeconds;
    }
    return k_ELeaderboardDisplayTypeNone;
}

template <typename Result>
bool WaitForCall(ISteamUtils* steamUtils, SteamAPICall_t call, Result& result)
{
    // Wait for the call
    if (call == k_uAPICallInvalid || !steamUtils)
        return true;
    PROFILE_CPU();
    bool failed = false;
    while (!Engine::ShouldExit() && !steamUtils->IsAPICallCompleted(call, &failed) && !failed)
    {
        Platform::Sleep(1);
    }
    if (failed)
    {
        ESteamAPICallFailure failure = steamUtils->GetAPICallFailureReason(call);
        return true;
    }

    // Get result
    if (!steamUtils->GetAPICallResult(call, &result, sizeof(result), result.k_iCallback, &failed))
        return true;

    return false;
}

OnlinePlatformSteam::OnlinePlatformSteam(const SpawnParams& params)
    : ScriptingObject(params)
{
}

bool OnlinePlatformSteam::Initialize()
{
    // Get Steam settings
    const auto settings = SteamSettings::Get();
    LOG(Info, "Initializing Steam API with AppId={0}", settings->AppId);
#if USE_EDITOR
    const uint32 appId = settings->AppId ? settings->AppId : 480;
#else
    const uint32 appId = settings->AppId;
#endif

#if USE_EDITOR
    // When running from Editor ensure to place steam appid config file in the root of the project
    const String steamAppIdFile = Globals::ProjectFolder / TEXT("steam_appid.txt");
    File::WriteAllText(steamAppIdFile, StringUtils::ToString(appId), Encoding::ANSI);
#endif

    // Give Steam a chance to relaunch a game via Steam App
    if (SteamAPI_RestartAppIfNecessary(appId))
    {
        LOG(Info, "Restarting game via Steam...");
        Engine::RequestExit(0);
        return true;
    }

    // Init Steam API
    if (!SteamAPI_Init())
    {
        LOG(Error, "SteamAPI init failed");
        return true;
    }
#define GET_STEAM_API(var, api) var = api(); if (!var) return true
    GET_STEAM_API(_steamClient, SteamClient);
    GET_STEAM_API(_steamUser, SteamUser);
    GET_STEAM_API(_steamFriends, SteamFriends);
    GET_STEAM_API(_steamUserStats, SteamUserStats);
    GET_STEAM_API(_steamRemoteStorage, SteamRemoteStorage);
    GET_STEAM_API(_steamUtils, SteamUtils);
#undef GET_STEAM_API

    _steamClient->SetWarningMessageHook(&SteamAPIDebugTextHook);
    Engine::LateUpdate.Bind<OnlinePlatformSteam, &OnlinePlatformSteam::OnUpdate>(this);

    return false;
}

void OnlinePlatformSteam::Deinitialize()
{
    if (!_steamClient)
        return;
    _steamClient = nullptr;
    _steamUser = nullptr;
    _steamFriends = nullptr;
    _steamUserStats = nullptr;
    _steamRemoteStorage = nullptr;
    _steamUtils = nullptr;
    _hasCurrentStats = false;
    _hasModifiedStats = false;
    Engine::LateUpdate.Unbind<OnlinePlatformSteam, &OnlinePlatformSteam::OnUpdate>(this);
    SteamAPI_Shutdown();
}

bool OnlinePlatformSteam::UserLogin(User* localUser)
{
    if (_steamUser && _steamUser->BLoggedOn())
    {
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::UserLogout(User* localUser)
{
    return false;
}

bool OnlinePlatformSteam::GetUserLoggedIn(User* localUser)
{
    return _steamUser && _steamUser->BLoggedOn();
}

bool OnlinePlatformSteam::GetUser(OnlineUser& user, User* localUser)
{
    if (_steamUser && _steamUser->BLoggedOn())
    {
        user.Id = GetUserId(_steamUser->GetSteamID());
        user.Name = _steamFriends->GetPersonaName();
        user.PresenceState = GetUserPresence(_steamFriends->GetPersonaState());
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::GetFriends(Array<OnlineUser>& friends, User* localUser)
{
    if (_steamUser && _steamUser->BLoggedOn())
    {
        const int32 friendsCount = _steamFriends->GetFriendCount(k_EFriendFlagImmediate);
        friends.Resize(friendsCount);
        for (int32 i = 0; i < friendsCount; i++)
        {
            auto& user = friends[i];
            const CSteamID friendId = _steamFriends->GetFriendByIndex(i, k_EFriendFlagImmediate);
            user.Id = GetUserId(friendId);
            user.Name = _steamFriends->GetFriendPersonaName(friendId);
            user.PresenceState = GetUserPresence(_steamFriends->GetFriendPersonaState(friendId));
        }
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::GetAchievements(Array<OnlineAchievement>& achievements, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const int32 count = _steamUserStats->GetNumAchievements();
        achievements.Resize(count);
        for (int32 i = 0; i < count; i++)
        {
            auto& achievement = achievements[i];
            const char* name = _steamUserStats->GetAchievementName(i);
            achievement.Identifier = name;
            // TODO: map Steam achievement name into game-specific name
            achievement.Name = achievement.Identifier;
            const char* title = _steamUserStats->GetAchievementDisplayAttribute(name, "name");
            achievement.Title.SetUTF8(title, StringUtils::Length(title));
            const char* desc = _steamUserStats->GetAchievementDisplayAttribute(name, "desc");
            achievement.Description.SetUTF8(desc, StringUtils::Length(desc));
            achievement.IsHidden = StringUtils::Compare(_steamUserStats->GetAchievementDisplayAttribute(name, "hidden"), "1") == 0;
            bool unlocked;
            uint32 unlockTime;
            if (_steamUserStats->GetAchievementAndUnlockTime(name, &unlocked, &unlockTime) && unlocked)
                achievement.UnlockTime = DateTimeFromUnixTimestamp((int32)unlockTime);
            achievement.Progress = unlocked ? 100.0f : 0.0f;
        }
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::UnlockAchievement(const StringView& name, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        // TODO: map game-specific name into Steam achievement name
        const StringAsANSI<> nameStr(name.Get(), name.Length());
        if (_steamUserStats->SetAchievement(nameStr.Get()))
        {
            _hasModifiedStats = true;
            _steamUserStats->IndicateAchievementProgress(nameStr.Get(), 100, 100);
            return false;
        }
    }
    return true;
}

bool OnlinePlatformSteam::UnlockAchievementProgress(const StringView& name, float progress, User* localUser)
{
    if (progress >= 100.0f)
        return UnlockAchievement(name, localUser);
    return false;
}

#if !BUILD_RELEASE

bool OnlinePlatformSteam::ResetAchievements(User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        _hasCurrentStats = false;
        _hasModifiedStats = false;
        _steamUserStats->ResetAllStats(true);
        return false;
    }
    return true;
}

#endif

bool OnlinePlatformSteam::GetStat(const StringView& name, float& value, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const StringAsANSI<k_cchStatNameMax> nameStr(name.Get(), name.Length());
        return !_steamUserStats->GetStat(nameStr.Get(), &value);
    }
    return true;
}

bool OnlinePlatformSteam::SetStat(const StringView& name, float value, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const StringAsANSI<k_cchStatNameMax> nameStr(name.Get(), name.Length());
        if (_steamUserStats->SetStat(nameStr.Get(), value))
        {
            _hasModifiedStats = true;
            return false;
        }
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboard(const StringView& name, OnlineLeaderboard& value, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const StringAsANSI<k_cchLeaderboardNameMax> nameStr(name.Get(), name.Length());
        SteamAPICall_t call = _steamUserStats->FindLeaderboard(nameStr.Get());
        value.Name = name;
        return GetLeaderboard(call, value);
    }
    return true;
}

bool OnlinePlatformSteam::GetOrCreateLeaderboard(const StringView& name, OnlineLeaderboardSortModes sortMode, OnlineLeaderboardValueFormats valueFormat, OnlineLeaderboard& value, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const StringAsANSI<k_cchLeaderboardNameMax> nameStr(name.Get(), name.Length());
        const auto sortMethod = GetLeaderboardSortMode(sortMode);
        const auto displayMode = GetLeaderboardValueFormat(valueFormat);
        SteamAPICall_t call = _steamUserStats->FindOrCreateLeaderboard(nameStr.Get(), sortMethod, displayMode);
        value.Name = name;
        return GetLeaderboard(call, value);
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboardEntries(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry>& entries, int32 start, int32 count)
{
    if (SteamLeaderboard_t steamLeaderboard = GetLeaderboardHandle(leaderboard))
    {
        SteamAPICall_t call = _steamUserStats->DownloadLeaderboardEntries(steamLeaderboard, k_ELeaderboardDataRequestGlobal, start + 1, start + count);
        return GetLeaderboardEntries(call, entries);
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboardEntriesAroundUser(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry>& entries, int32 start, int32 count)
{
    if (SteamLeaderboard_t steamLeaderboard = GetLeaderboardHandle(leaderboard))
    {
        SteamAPICall_t call = _steamUserStats->DownloadLeaderboardEntries(steamLeaderboard, k_ELeaderboardDataRequestGlobalAroundUser, start, start + count);
        return GetLeaderboardEntries(call, entries);
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboardEntriesForFriends(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry>& entries)
{
    if (SteamLeaderboard_t steamLeaderboard = GetLeaderboardHandle(leaderboard))
    {
        SteamAPICall_t call = _steamUserStats->DownloadLeaderboardEntries(steamLeaderboard, k_ELeaderboardDataRequestFriends, 0, 0);
        return GetLeaderboardEntries(call, entries);
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboardEntriesForUsers(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry>& entries, const Array<OnlineUser>& users)
{
    if (SteamLeaderboard_t steamLeaderboard = GetLeaderboardHandle(leaderboard))
    {
        Array<CSteamID, InlinedAllocation<8>> steamUsers;
        steamUsers.Resize(users.Count());
        for (int32 i = 0; i < users.Count(); i++)
            steamUsers[i] = GetSteamId(users[i].Id);
        SteamAPICall_t call = _steamUserStats->DownloadLeaderboardEntriesForUsers(steamLeaderboard, steamUsers.Get(), steamUsers.Count());
        return GetLeaderboardEntries(call, entries);
    }
    return true;
}

bool OnlinePlatformSteam::SetLeaderboardEntry(const OnlineLeaderboard& leaderboard, int32 score, bool keepBest)
{
    if (SteamLeaderboard_t steamLeaderboard = GetLeaderboardHandle(leaderboard))
    {
        SteamAPICall_t call = _steamUserStats->UploadLeaderboardScore(steamLeaderboard, keepBest ? k_ELeaderboardUploadScoreMethodKeepBest : k_ELeaderboardUploadScoreMethodForceUpdate, score, nullptr, 0);
        return call == 0;
    }
    return true;
}

bool OnlinePlatformSteam::GetSaveGame(const StringView& name, Array<byte, HeapAllocation>& data, User* localUser)
{
    PROFILE_CPU();
    if (_steamRemoteStorage)
    {
        const StringAsANSI<> nameStr(name.Get(), name.Length());
        data.Clear();
        if (_steamRemoteStorage->FileExists(nameStr.Get()))
        {
            const int32 size = _steamRemoteStorage->GetFileSize(nameStr.Get());
            if (size > 0)
            {
                data.Resize(size);
                const int32 read = _steamRemoteStorage->FileRead(nameStr.Get(), data.Get(), size);
                if (read != size)
                {
                    data.Clear();
                    return true;
                }
            }
        }
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::SetSaveGame(const StringView& name, const Span<byte>& data, User* localUser)
{
    PROFILE_CPU();
    if (_steamRemoteStorage)
    {
        const StringAsANSI<> nameStr(name.Get(), name.Length());
        if (data.Length() > 0)
        {
            // Write
            return !_steamRemoteStorage->FileWrite(nameStr.Get(), data.Get(), data.Length());
        }
        else if (_steamRemoteStorage->FileExists(nameStr.Get()))
        {
            // Delete
            return !_steamRemoteStorage->FileDelete(nameStr.Get());
        }
        return false;
    }
    return true;
}

bool OnlinePlatformSteam::RequestCurrentStats()
{
    if (!_hasCurrentStats)
    {
        _hasCurrentStats = true;
        return _steamUserStats->RequestCurrentStats();
    }
    return true;
}

bool OnlinePlatformSteam::GetLeaderboard(SteamAPICall_t call, OnlineLeaderboard& leaderboard) const
{
    LeaderboardFindResult_t result;
    if (WaitForCall(_steamUtils, call, result))
        return true;
    if (result.m_bLeaderboardFound == 0)
    {
        LOG(Error, "Steam leaderboard '{}' not found", leaderboard.Name);
        return true;
    }

    // Get leaderboard properties
    leaderboard.Identifier = StringUtils::ToString(result.m_hSteamLeaderboard);
    leaderboard.Name = _steamUserStats->GetLeaderboardName(result.m_hSteamLeaderboard);
    leaderboard.SortMode = GetLeaderboardSortMode(_steamUserStats->GetLeaderboardSortMethod(result.m_hSteamLeaderboard));
    leaderboard.ValueFormat = GetLeaderboardValueFormat(_steamUserStats->GetLeaderboardDisplayType(result.m_hSteamLeaderboard));
    leaderboard.EntriesCount = _steamUserStats->GetLeaderboardEntryCount(result.m_hSteamLeaderboard);
    return false;
}

uint64 OnlinePlatformSteam::GetLeaderboardHandle(const OnlineLeaderboard& leaderboard)
{
    static_assert(sizeof(uint64) == sizeof(SteamLeaderboard_t), "Update API.");
    SteamLeaderboard_t steamLeaderboard = 0;
    if (_steamUserStats &&
        _steamUser->BLoggedOn() &&
        RequestCurrentStats() &&
        !StringUtils::Parse(leaderboard.Identifier.Get(), leaderboard.Identifier.Length(), &steamLeaderboard) &&
        steamLeaderboard != 0)
    {
        return steamLeaderboard;
    }
    return 0;
}

bool OnlinePlatformSteam::GetLeaderboardEntries(SteamAPICall_t call, Array<OnlineLeaderboardEntry>& entries) const
{
    LeaderboardScoresDownloaded_t result;
    if (WaitForCall(_steamUtils, call, result))
        return true;

    // Get entries
    entries.Resize(result.m_cEntryCount);
    for (int32 i = 0; i < result.m_cEntryCount; i++)
    {
        LeaderboardEntry_t e = {};
        _steamUserStats->GetDownloadedLeaderboardEntry(result.m_hSteamLeaderboardEntries, i, &e, nullptr, 0);

        auto& entry = entries[i];
        entry.User.Id = GetUserId(e.m_steamIDUser);
        entry.User.PresenceState = OnlinePresenceStates::Offline;
        if (e.m_steamIDUser == _steamUser->GetSteamID())
        {
            // Local user
            entry.User.Id = GetUserId(_steamUser->GetSteamID());
            entry.User.Name = _steamFriends->GetPersonaName();
            entry.User.PresenceState = GetUserPresence(_steamFriends->GetPersonaState());
        }
        else if (_steamFriends)
        {
            // Friend?
            entry.User.Name = _steamFriends->GetFriendPersonaName(e.m_steamIDUser);
            entry.User.PresenceState = GetUserPresence(_steamFriends->GetFriendPersonaState(e.m_steamIDUser));
        }
        entry.Rank = e.m_nGlobalRank;
        entry.Score = e.m_nScore;
    }

    return false;
}

void OnlinePlatformSteam::OnUpdate()
{
    // TODO: delay StoreStats calls frequency to be once a minute or so
    if (_hasModifiedStats)
    {
        _hasModifiedStats = false;
        _steamUserStats->StoreStats();
    }

    SteamAPI_RunCallbacks();
}

#endif
