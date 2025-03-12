# Steam for Flax Engine

This repository contains a plugin project for [Flax Engine](https://flaxengine.com/) games with [Steam](https://partner.steamgames.com/) online platform implementation that covers: user profile, friends list, online presence, achevements, cloud savegames and more.

Minimum supported Flax version: `1.3`.

## Installation

1. Clone repo into `<game-project>\Plugins\OnlinePlatformSteam`

2. Add reference to *OnlinePlatformSteam* project in your game by modyfying `<game-project>.flaxproj` as follows:

```
...
"References": [
    {
        "Name": "$(EnginePath)/Flax.flaxproj"
    },
    {
        "Name": "$(ProjectPath)/Plugins/OnlinePlatformSteam/OnlinePlatformSteam.flaxproj"
    }
]
```

3. Add reference to Steam plugin module in you game code module by modyfying `Source/Game/Game.Build.cs` as follows (or any other game modules using Online):

```cs
/// <inheritdoc />
public override void Setup(BuildOptions options)
{
    base.Setup(options);

    ...

    switch (options.Platform.Target)
    {
    case TargetPlatform.Windows:
    case TargetPlatform.Linux:
    case TargetPlatform.Mac:
        options.PublicDependencies.Add("OnlinePlatformSteam");
        break;
    }
}
```

This will add reference to `OnlinePlatformSteam` module on Windows/Linux/Mac platforms that are supported by Steam.

4. Setup settings

Steam plugin automatically creates `steam_appid.txt` file with Steam AppId (for both Editor and cooked Game). You can add own settings in *Content* window by using *right-click*, then **New -> Settings**, specify name, select **SteamSettings** type and confirm.

5. Test it out!

Finally you can use Steam as online platform in your game:

```cs
// C#
using FlaxEngine.Online;
using FlaxEngine.Online.Steam;

var platform = new OnlinePlatformSteam();
Online.Initialize(platform);
```

```cpp
// C++
#include "Engine/Online/Online.h"
#include "OnlinePlatformSteam/OnlinePlatformSteam.h"

auto platform = New<OnlinePlatformSteam>();
Online::Initialize(platform);
```

Then use [Online](https://docs.flaxengine.com/manual/networking/online/index.html) system to access online platform (user profile, friends, achievements, cloud saves, etc.).

## License

This plugin ais released under **MIT License**.
