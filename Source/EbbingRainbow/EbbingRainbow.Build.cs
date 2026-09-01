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
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"EbbingRainbow",
			"EbbingRainbow/Variant_Platforming",
			"EbbingRainbow/Variant_Platforming/Animation",
			"EbbingRainbow/Variant_Combat",
			"EbbingRainbow/Variant_Combat/AI",
			"EbbingRainbow/Variant_Combat/Animation",
			"EbbingRainbow/Variant_Combat/Gameplay",
			"EbbingRainbow/Variant_Combat/Interfaces",
			"EbbingRainbow/Variant_Combat/UI",
			"EbbingRainbow/Variant_SideScrolling",
			"EbbingRainbow/Variant_SideScrolling/AI",
			"EbbingRainbow/Variant_SideScrolling/Gameplay",
			"EbbingRainbow/Variant_SideScrolling/Interfaces",
			"EbbingRainbow/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
