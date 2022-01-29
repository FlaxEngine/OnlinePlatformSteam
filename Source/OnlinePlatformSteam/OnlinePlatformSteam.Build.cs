// Copyright (c) 2012-2022 Wojciech Figat. All rights reserved.

using Flax.Build;
using Flax.Build.NativeCpp;

/// <summary>
/// Online services module for Steam platform.
/// </summary>
public class OnlinePlatformSteam : GameModule
{
    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        options.PublicDependencies.Add("Online");
        options.PrivateDependencies.Add("Steamworks");
    }
}
