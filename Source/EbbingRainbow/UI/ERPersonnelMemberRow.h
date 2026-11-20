// Copyright Epic Games, Inc. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hall/HallRoster.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERPersonnelMemberRow.generated.h"

class UBorder;
class UButton;
class UERTechFrame;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnERPersonnelMemberSelected, int32, MemberIndex);

/**
 * 人员管理面板左侧的成员条目:
 *   [学派色刻度] [立绘缩略] [名字 / 学派·定位] [完整度]
 * 选中态 = 学派色刻度亮起 + 玻璃提亮 + 发丝描边; 悬停有极轻的提亮 (不做位移, 保持安静)。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERPersonnelMemberRow : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void Setup(int32 InIndex, const FHallHeroProfile& Profile, float CurrentIntegrity, bool bHasRunState, bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void SetPortrait(UTexture2D* InTexture);

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	int32 GetMemberIndex() const { return MemberIndex; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI|Personnel")
	FOnERPersonnelMemberSelected OnMemberSelected;

	/** 面板在打开时把占位立绘塞给每一行 (WBP 未指定时用) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Personnel")
	TObjectPtr<UTexture2D> DefaultPortrait = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bDriveHoverFromCode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float HoverInterpSpeed = 12.0f;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewName = INVTEXT("磐");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewSchool = INVTEXT("力学 · 先锋");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewIntegrity = INVTEXT("60");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewSelected = true;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechFrame> RowFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RowBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectRule;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ThumbFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitThumb;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MemberIndexText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MemberName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SchoolDot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MemberSchool;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MemberIntegrity;

	UFUNCTION()
	void HandleClicked();

private:
	void Build();
	void RefreshVisuals(float DeltaTime, bool bDesignPreview);

	int32 MemberIndex = INDEX_NONE;
	bool bSelected = false;
	bool bHovered = false;
	bool bWasHovered = false;
	bool bBuilt = false;
	bool bDelegatesBound = false;
	FERFloatSpring HoverSpring = FERFloatSpring::Tactile(0.0f);
	FERFloatSpring SelectSpring = FERFloatSpring::Snappy(0.0f);
	float SweepProgress = 1.0f;

	FLinearColor Accent = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);
	float IntegrityAlpha = 1.0f;
	bool bLowIntegrity = false;
};
