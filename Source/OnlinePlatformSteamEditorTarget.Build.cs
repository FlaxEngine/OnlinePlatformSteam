using Flax.Build;

public class OnlinePlatformSteamEditorTarget : GameProjectEditorTarget
{
    /// <inheritdoc />
    public override void Init()
    {
        base.Init();

        Platforms = new[]
        {
            TargetPlatform.Windows,
            TargetPlatform.Linux,
            TargetPlatform.Mac,
        };

        Modules.Add("OnlinePlatformSteam");
    }
}
