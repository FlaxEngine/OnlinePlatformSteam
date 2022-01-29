// This code was auto-generated. Do not modify it.

#include "Engine/Scripting/BinaryModule.h"
#include "OnlinePlatformSteam.Gen.h"

StaticallyLinkedBinaryModuleInitializer StaticallyLinkedBinaryModuleOnlinePlatformSteam(GetBinaryModuleOnlinePlatformSteam);

extern "C" BinaryModule* GetBinaryModuleOnlinePlatformSteam()
{
    static NativeBinaryModule module("OnlinePlatformSteam", MAssemblyOptions());
    return &module;
}
