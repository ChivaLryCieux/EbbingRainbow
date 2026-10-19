// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WFCGenerationTypes.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DStyleTable.h"
#include "WFC3DArenaHost.generated.h"

/**
 * 三参数地形生成容器。
 * 这是战斗层唯一需要打交道的 WFC 入口：
 *   Generate({ Style, Scale, Seed })
 * 它只负责：查风格表 -> 创建/复用具体生成器 -> 转发请求 -> 保存结果。
 * 它不关心地牢、中庭、螺旋等任何具体拓扑。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaHost : public AActor
{
	GENERATED_BODY()

public:
	AWFC3DArenaHost();

	virtual void BeginPlay() override;

	/** 三参数生成入口 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WFC|Host")
	bool Generate(const FWFCGenerationRequest& Request);

	/** 用 PreviewRequest 生成（编辑器调参用） */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WFC|Host")
	bool GeneratePreview();

	/** 清理已生成的实例，保留生成器 Actor 以便复用 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Host")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "WFC|Host")
	const FWFCGenerationResult& GetLastResult() const { return LastResult; }

	UFUNCTION(BlueprintPure, Category = "WFC|Host")
	AWFC3DArenaGeneratorBase* GetActiveGenerator() const { return ActiveGenerator; }

public:
	/** 风格表；为空时回退到 FallbackGeneratorClass */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Host")
	TObjectPtr<UWFC3DStyleTable> StyleTable = nullptr;

	/** 风格表未配置或解析失败时的兜底生成器类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Host")
	TSubclassOf<AWFC3DArenaGeneratorBase> FallbackGeneratorClass;

	/** 编辑器预览参数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Host")
	FWFCGenerationRequest PreviewRequest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Host")
	bool bAutoGenerateOnBeginPlay = false;

private:
	/** 确保当前生成器是目标类；风格切换时销毁旧生成器 */
	bool EnsureActiveGenerator(TSubclassOf<AWFC3DArenaGeneratorBase> Class);

	UPROPERTY(Transient)
	TObjectPtr<AWFC3DArenaGeneratorBase> ActiveGenerator = nullptr;

	FWFCGenerationResult LastResult;

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot = nullptr;
};
