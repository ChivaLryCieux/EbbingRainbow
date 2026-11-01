// Copyright Epic Games, Inc. All Rights Reserved.

#include "EbbingRainbow.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

/**
 * Primary game module.
 *
 * Besides the default behaviour it registers the project shader directory as the
 * virtual path "/Project", which is what lets material Custom nodes (and any
 * global shader we add later) #include "/Project/Painterly/*.ush".
 */
class FEbbingRainbowModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

		const FString VirtualPath = TEXT("/Project");
		if (!AllShaderSourceDirectoryMappings().Contains(VirtualPath))
		{
			const FString ShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(VirtualPath, ShaderDir);
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FEbbingRainbowModule, EbbingRainbow, "EbbingRainbow" );

DEFINE_LOG_CATEGORY(LogEbbingRainbow)
