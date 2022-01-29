// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

using System.IO;
using Flax.Build;
using Flax.Build.NativeCpp;

/// <summary>
/// Steamworks SDK
/// </summary>
public class Steamworks : DepsModule
{
    /// <inheritdoc />
    public override void Init()
    {
        base.Init();

        LicenseType = LicenseTypes.Custom;
        LicenseFilePath = "LICENSE";
        BinaryModuleName = null;
        BuildNativeCode = false;
    }

    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        var binariesFolder = Path.Combine(FolderPath, "Binaries", options.Platform.Target.ToString());
        switch (options.Platform.Target)
        {
        case TargetPlatform.Windows:
            options.OutputFiles.Add(Path.Combine(binariesFolder, "steam_api64.lib"));
            options.DependencyFiles.Add(Path.Combine(binariesFolder, "steam_api64.dll"));
            options.DelayLoadLibraries.Add("steam_api64.dll");
            break;
        case TargetPlatform.Linux:
            options.DependencyFiles.Add(Path.Combine(binariesFolder, "libsteam_api.so"));
            options.Libraries.Add(Path.Combine(binariesFolder, "libsteam_api.so"));
            break;
        case TargetPlatform.Mac:
            options.DependencyFiles.Add(Path.Combine(binariesFolder, "libsteam_api.dylib"));
            options.Libraries.Add(Path.Combine(binariesFolder, "libsteam_api.dylib"));
            break;
        default: throw new InvalidPlatformException(options.Platform.Target);
        }
    }
}
