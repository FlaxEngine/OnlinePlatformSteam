// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

#if FLAX_EDITOR

using System;
using System.IO;
using FlaxEditor;
using FlaxEditor.Content;

namespace FlaxEngine.Online.Steam
{
    /// <summary>
    /// Steam online platform plugin for Editor.
    /// </summary>
    public sealed class SteamEditorPlugin : EditorPlugin
    {
        /// <inheritdoc />
        public override PluginDescription Description => new PluginDescription
        {
            Name = "Steam",
            Category = "Online",
            Description = "Online platform implementation for Steam.",
            Author = "Flax",
            RepositoryUrl = "https://github.com/FlaxEngine/OnlinePlatformSteam",
            Version = new Version(1, 0),
        };

        private AssetProxy _assetProxy;

        /// <inheritdoc />
        public override void InitializeEditor()
        {
            base.InitializeEditor();

            GameCooker.DeployFiles += OnDeployFiles;
            _assetProxy = new CustomSettingsProxy(typeof(SteamSettings), "Steam");
            Editor.ContentDatabase.Proxy.Add(_assetProxy);
        }

        /// <inheritdoc />
        public override void Deinitialize()
        {
            Editor.ContentDatabase.Proxy.Remove(_assetProxy);
            _assetProxy = null;
            GameCooker.DeployFiles -= OnDeployFiles;

            base.Deinitialize();
        }

        private void OnDeployFiles()
        {
            // Include steam_appid.txt file with a game
            var data = GameCooker.CurrentData;
            var settingsAsset = Engine.GetCustomSettings("Steam");
            var settings = settingsAsset?.CreateInstance<SteamSettings>();
            var appId = settings?.AppId ?? 480;
            switch (data.Platform)
            {
            case BuildPlatform.Windows32:
            case BuildPlatform.Windows64:
            case BuildPlatform.LinuxX64:
            case BuildPlatform.MacOSx64:
                File.WriteAllText(Path.Combine(data.NativeCodeOutputPath, "steam_appid.txt"), appId.ToString());
                break;
            }
        }
    }
}

#endif