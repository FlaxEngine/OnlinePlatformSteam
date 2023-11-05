// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC

#include "OnlinePlatformSteam.h"
#include "Engine/Content/Content.h"
#include "Engine/Content/JsonAsset.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Config/GameSettings.h"
#include "Engine/Core/Collections/Array.h"
#include "Engine/Engine/Engine.h"
#include "Engine/Utilities/StringConverter.h"
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
                achievement.UnlockTime = DateTime::FromUnixTimestamp((int32)unlockTime);
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
        const StringAsANSI<> nameStr(name.Get(), name.Length());
        return !_steamUserStats->GetStat(nameStr.Get(), &value);
    }
    return true;
}

bool OnlinePlatformSteam::SetStat(const StringView& name, float value, User* localUser)
{
    if (_steamUserStats && _steamUser->BLoggedOn() && RequestCurrentStats())
    {
        const StringAsANSI<> nameStr(name.Get(), name.Length());
        if (_steamUserStats->SetStat(nameStr.Get(), value))
        {
            _hasModifiedStats = true;
            return false;
        }
    }
    return true;
}

bool OnlinePlatformSteam::GetSaveGame(const StringView& name, Array<byte, HeapAllocation>& data, User* localUser)
{
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
