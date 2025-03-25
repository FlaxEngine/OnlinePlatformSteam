// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

#pragma once

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC

#include "Engine/Core/Config/Settings.h"
#include "Engine/Online/IOnlinePlatform.h"
#include "Engine/Scripting/ScriptingObject.h"

/// <summary>
/// The settings for Steam online platform.
/// </summary>
API_CLASS(Namespace="FlaxEngine.Online.Steam") class ONLINEPLATFORMSTEAM_API SteamSettings : public SettingsBase
{
    API_AUTO_SERIALIZATION();
    DECLARE_SCRIPTING_TYPE_NO_SPAWN(SteamSettings);
    DECLARE_SETTINGS_GETTER(SteamSettings);
public:
    // App ID of the game.
    API_FIELD(Attributes="EditorOrder(0)")
    uint32 AppId = 0;
};

/// <summary>
/// The online platform implementation for Steam.
/// </summary>
API_CLASS(Sealed, Namespace="FlaxEngine.Online.Steam") class ONLINEPLATFORMSTEAM_API OnlinePlatformSteam : public ScriptingObject, public IOnlinePlatform
{
    DECLARE_SCRIPTING_TYPE(OnlinePlatformSteam);
private:
    class ISteamClient* _steamClient = nullptr;
    class ISteamUser* _steamUser = nullptr;
    class ISteamFriends* _steamFriends = nullptr;
    class ISteamUserStats* _steamUserStats = nullptr;
    class ISteamRemoteStorage* _steamRemoteStorage = nullptr;
    class ISteamUtils* _steamUtils = nullptr;
    bool _hasCurrentStats = false;
    bool _hasModifiedStats = false;

public:
    // [IOnlinePlatform]
    bool Initialize() override;
    void Deinitialize() override;
    bool UserLogin(User* localUser) override;
    bool UserLogout(User* localUser) override;
    bool GetUserLoggedIn(User* localUser) override;
    bool GetUser(OnlineUser& user, User* localUser) override;
    bool GetFriends(Array<OnlineUser, HeapAllocation>& friends, User* localUser) override;
    bool GetAchievements(Array<OnlineAchievement, HeapAllocation>& achievements, User* localUser) override;
    bool UnlockAchievement(const StringView& name, User* localUser) override;
    bool UnlockAchievementProgress(const StringView& name, float progress, User* localUser) override;
#if !BUILD_RELEASE
    bool ResetAchievements(User* localUser) override;
#endif
    bool GetStat(const StringView& name, float& value, User* localUser) override;
    bool SetStat(const StringView& name, float value, User* localUser) override;
    bool GetLeaderboard(const StringView& name, OnlineLeaderboard& value, User* localUser) override;
    bool GetOrCreateLeaderboard(const StringView& name, OnlineLeaderboardSortModes sortMode, OnlineLeaderboardValueFormats valueFormat, OnlineLeaderboard& value, User* localUser) override;
    bool GetLeaderboardEntries(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry, HeapAllocation>& entries, int32 start, int32 count) override;
    bool GetLeaderboardEntriesAroundUser(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry, HeapAllocation>& entries, int32 start, int32 count) override;
    bool GetLeaderboardEntriesForFriends(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry, HeapAllocation>& entries) override;
    bool GetLeaderboardEntriesForUsers(const OnlineLeaderboard& leaderboard, Array<OnlineLeaderboardEntry, HeapAllocation>& entries, const Array<OnlineUser, HeapAllocation>& users) override;
    bool SetLeaderboardEntry(const OnlineLeaderboard& leaderboard, int32 score, bool keepBest) override;
    bool GetSaveGame(const StringView& name, Array<byte, HeapAllocation>& data, User* localUser) override;
    bool SetSaveGame(const StringView& name, const Span<byte>& data, User* localUser) override;

private:
    bool RequestCurrentStats();
    bool GetLeaderboard(uint64 call, OnlineLeaderboard& leaderboard) const;
    uint64 GetLeaderboardHandle(const OnlineLeaderboard& leaderboard);
    bool GetLeaderboardEntries(uint64 call, Array<OnlineLeaderboardEntry, HeapAllocation>& entries) const;
    void OnUpdate();
};

#endif
