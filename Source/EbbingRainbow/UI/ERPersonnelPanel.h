// Copyright Epic Games, Inc. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hall/HallRoster.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERPersonnelPanel.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UERPersonnelMemberRow;
class UERPrismBar;
class UERStatCell;
class UERTechDivider;
class UERTechFrame;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERPersonnelPanelClosed);

/**
 * 大厅「人员管理」面板 (WBP_PersonnelPanel)。
 *
 * 结构: 左侧成员列表 (四人) → 右侧档案 = 左立绘 / 右名字·学派·基本属性·四学派抗性·战术技能。
 * 数值来自 Hall/HallRoster.h (与战斗/远征共用同一份模板), 远征进行中时完整度显示当前值。
 * 立绘统一走占位图 (默认 /Game/UI/Textures/T_UI_HeroPortrait_Placeholder, 源文件根目录 IMG.PNG),
 * 等专属立绘就位后按 FHallHeroProfile::Portrait 逐个替换即可。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERPersonnelPanel : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UERPersonnelPanel();

	/** 打开面板 (刷新档案 + 播放入场) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void OpenPanel();

	/** 关闭面板 (先播退场, 结束后广播 OnPanelClosed) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void ClosePanel();

	/** 选中第 Index 名成员 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void SelectMember(int32 MemberIndex);

	/** 重新从 HallRoster / RunSubsystem 拉取档案与远征状态 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Personnel")
	void RefreshRoster();

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	bool IsPanelOpen() const { return bPanelOpen; }

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	int32 GetMemberCount() const { return Profiles.Num(); }

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	FHallHeroProfile GetSelectedProfile() const;

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	FText GetSquadSummary() const;

	UFUNCTION(BlueprintPure, Category = "ERUI|Personnel")
	FText GetSquadState() const;

	/** 面板真正关闭 (退场动画结束) 时广播; HallHUD 据此移除控件 */
	UPROPERTY(BlueprintAssignable, Category = "ERUI|Personnel")
	FOnERPersonnelPanelClosed OnPanelClosed;

	// ---- 可调参数 ----

	/** 成员条目蓝图类 (WBP_PersonnelMemberRow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERPersonnelMemberRow> MemberRowClass;

	/** 属性格蓝图类 (WBP_PersonnelStatCell) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERStatCell> StatCellClass;

	/** 占位立绘 (统一 IMG.PNG); 角色专属立绘为空时用它 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Personnel")
	TObjectPtr<UTexture2D> DefaultPortrait = nullptr;

	/**
	 * 兜底占位立绘 (软引用, 默认指向根目录 IMG.PNG 导入的贴图)。
	 * 即使 WBP/主题都没配 DefaultPortrait, 面板也一定有立绘可显示;
	 * 专属立绘到位后按 FHallHeroProfile::Portrait 覆盖即可。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Personnel")
	TSoftObjectPtr<UTexture2D> PlaceholderPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Personnel")
	bool bCloseOnOutsideClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bEnableEntranceAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceDuration = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceOffsetY = 16.0f;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewInDesigner = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Personnel", meta = (DisplayName = "On Panel Opened"))
	void BP_OnPanelOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Personnel", meta = (DisplayName = "On Panel Closed"))
	void BP_OnPanelClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Personnel", meta = (DisplayName = "On Member Selected"))
	void BP_OnMemberSelected(int32 MemberIndex);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* GetHoverCheckWidget() const override { return const_cast<UERPersonnelPanel*>(this); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	// ---- WBP 绑定 (全部 Optional, 缺失时由 C++ Build() 兜底) ----

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ScrimImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechFrame> PanelFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERPrismBar> HeaderPrismBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HeaderTitle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HeaderCaption;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SquadSummaryText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SquadStateText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CloseGlyphText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechDivider> HeaderDivider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> HeaderRule;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RailTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> MemberListBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RailHintText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> RailRule;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DetailContainer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> PortraitFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PortraitImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PortraitScrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PortraitIndexText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PortraitCaptionText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CallsignText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> SchoolChipBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SchoolChipText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RoleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TaglineText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechDivider> InfoDivider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InfoRule;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatsTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> PrimaryStatsBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> SecondaryStatsBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResistTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> ResistBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SkillsTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> SkillsBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FooterHintText;

	UFUNCTION() void HandleMemberSelected(int32 MemberIndex);
	UFUNCTION() void HandleCloseClicked();

private:
	void Build();
	void BindDelegates();
	void BuildMemberRows();
	void RefreshDetail();
	void RefreshHeader();
	void RefreshSelectionVisuals();
	void UpdateAnimation(float DeltaTime);

	UERStatCell* MakeStatCell(UHorizontalBox* Box, const FText& Label, const FText& Value, FLinearColor Tint);
	void PopulateSkills(const FHallHeroProfile& Profile);
	UTexture2D* ResolvePortrait(const FHallHeroProfile& Profile) const;
	FText FormatPercent(float Ratio, bool bSigned) const;
	UWorld* ResolvePlayWorld() const;
	class URunSubsystem* GetRunSubsystem() const;
	bool HasRunState() const;
	float GetShownIntegrity(int32 MemberIndex) const;

	UPROPERTY(Transient)
	TArray<FHallHeroProfile> Profiles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UERPersonnelMemberRow>> MemberRows;

	int32 SelectedIndex = 0;
	bool bPanelOpen = false;
	bool bClosing = false;
	float AnimT = 1.0f;
	float AnimDirection = 0.0f;
	FERRevealTrack DetailTrack;
	FERCipherReveal HeaderCaptionCipher;
	FERCipherReveal CallsignCipher;
	bool bBuilt = false;
	bool bDelegatesBound = false;
};
