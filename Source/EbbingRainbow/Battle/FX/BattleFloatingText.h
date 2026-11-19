// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleFloatingText.generated.h"

class UTextRenderComponent;
class UWidgetComponent;
class UERFloatingText;
class UBattleFXPool;

/**
 * 战场浮动跳字 (伤害数字 / 治疗 / 护盾 / 状态提示)
 * 默认使用屏幕空间 UMG WidgetComponent (WBP_FloatingText, 支持清晰中文复合字体与蓝图手调),
 * 同时保留 TextRenderComponent 作为无 WBP 时的兜底。
 *
 * 经 UBattleFXPool 池化: 到期不销毁, 隐藏回池复用。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API ABattleFloatingText : public AActor
{
	GENERATED_BODY()

public:
	ABattleFloatingText();

	virtual void Tick(float DeltaSeconds) override;

	/** 池化获取入口 (由 UBattleFXPool 调用): 传送到位 + 重置内容 + 显示开 Tick */
	void PoolAcquire(const FVector& Location, const FString& Text, const FColor& InColor, float InWorldSize = 36.0f);

	/** 池化归还: 隐藏 + 停 Tick (由 UBattleFXPool 调用) */
	void PoolRelease();

	/** 便捷静态生成函数 (走对象池, 保留旧签名) */
	static ABattleFloatingText* SpawnFloatingText(UWorld* World, const FVector& Location, const FString& Text, const FColor& Color, float WorldSize = 36.0f);

	// ---- 可在 BP_BattleFloatingText 中手调的弹道与动效参数 ----

	/** 跳字 UMG 控件类 (例如 WBP_FloatingText; 为空时回退到 TextRender) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	TSubclassOf<UERFloatingText> FloatingWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float UpSpeedMin = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float UpSpeedMax = 190.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float Gravity = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float ScatterXY = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float ScatterZ = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float CameraRightScatter = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float NormalLifeTime = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float CritLifeTime = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float CritSizeThreshold = 44.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float NormalPopScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float CritPopScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float PopDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float FadeStartFraction = 0.55f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> WidgetComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TextRender;

private:
	FColor BaseColor = FColor(255, 80, 80);
	FVector Velocity = FVector::ZeroVector;
	float LifeTime = 1.1f;
	float ElapsedTime = 0.0f;
	float PopStartScale = 1.4f;
	uint8 LastWrittenAlpha = 255;
};
