// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class audiov1 : ModuleRules
{
	public audiov1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", 
			"CoreUObject",
			"Engine",
			"AudioCapture",
            "tp",
            "TcpScenv1",
            "AudioMixer",
            "InputCore",
			"HeadMountedDisplay" });
	}
}
