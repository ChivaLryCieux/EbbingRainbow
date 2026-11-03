// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EbbingRainbow : ModuleRules
{
	public EbbingRainbow(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"UMG",
			"Slate",
			"AssetRegistry",
			"NavigationSystem",
			"SlateCore",
			// ShaderCore.h (AddShaderSourceDirectoryMapping) lives in RenderCore in UE5.8
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"EbbingRainbow",
			"EbbingRainbow/Environment",
			"EbbingRainbow/WFC",
			"EbbingRainbow/WFC/Arch",
			"EbbingRainbow/WFC/Styles",
			"EbbingRainbow/WFC/Voxel",
			"EbbingRainbow/Battle",
			"EbbingRainbow/Battle/Core",
			"EbbingRainbow/Battle/Arena",
			"EbbingRainbow/Battle/Units",
			"EbbingRainbow/Battle/Skills",
			"EbbingRainbow/Battle/Player",
			"EbbingRainbow/Battle/UI",
			"EbbingRainbow/Hall",
			"EbbingRainbow/Hall/UI",
			"EbbingRainbow/Run",
			"EbbingRainbow/Run/UI",
			"EbbingRainbow/Motorcycle"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
