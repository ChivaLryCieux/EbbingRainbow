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
			"Niagara",
			// 出击部署接驳屏: 轮询 Landscape 草皮实例数判定植被构筑完成度
			"Landscape",
			"Foliage",
			// 技能系统 2.0: 标签容器 (技能/状态/反应表的语义标识)
			"GameplayTags"
		});

		// =========================================================================
		// 实验性 AI 战术指挥模块总开关 (毕业设计实验点; 正式上线改为 false 即可一键剥离)
		// =========================================================================
		bool bEnableExperimentalAI = true;
		PublicDefinitions.Add("ER_WITH_AI_COMMAND=" + (bEnableExperimentalAI ? "1" : "0"));
		if (bEnableExperimentalAI)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"HTTP",
				"Json",
				"JsonUtilities"
			});
		}

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
