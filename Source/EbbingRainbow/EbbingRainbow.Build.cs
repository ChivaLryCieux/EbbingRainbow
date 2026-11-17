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
			"RenderCore",
			// UERUISettings (UDeveloperSettings)
			"DeveloperSettings",
			"ProceduralMeshComponent",
			// 技能系统 2.0: 标签容器 (技能/状态/反应表的语义标识)
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// 单一包含根: 所有 #include 用模块相对路径 ("Battle/Units/BattleUnit.h"),
		// 不再维护目录白名单, 避免同名头文件歧义
		PublicIncludePaths.Add("EbbingRainbow");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
