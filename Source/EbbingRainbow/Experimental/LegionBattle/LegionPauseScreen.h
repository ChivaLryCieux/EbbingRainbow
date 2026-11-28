// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LegionPauseScreen.generated.h"

class UButton;
class UImage;
class UTextBlock;

/**
 * 军团战场暂停覆盖层 (按 Esc 呼出):
 *  - 视觉延续接驳屏与战终结算页的《控制:共振》式极简架构排版:
 *    1. 深墨半透明底幕 (冻结战场透出)
 *    2. 超大粗体纯白星球名 (端点星) + 黑色错位立体阴影
 *    3. 虹膜 (IRIS) 白色徽标
 *    4. 三项居中的高对比纯白文字 + 错位黑影菜单项 (无白底卡片):
 *       「继续连接」 / 「中止连接」 / 「设置」
 */
UCLASS()
class EBBINGRAINBOW_API ULegionPauseScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleDelegate OnResumeClicked;
	FSimpleDelegate OnAbortClicked;
	FSimpleDelegate OnSettingsClicked;

	void ShowPauseMenu();
	void HidePauseMenu();
	bool IsPauseMenuVisible() const;

	void TriggerResumeForTest() { HandleResumeClicked(); }
	void TriggerAbortForTest() { HandleAbortClicked(); }
	void TriggerSettingsForTest() { HandleSettingsClicked(); }

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	struct FPauseMenuItem
	{
		TObjectPtr<UButton> Button = nullptr;
		TObjectPtr<UTextBlock> Label = nullptr;
		TObjectPtr<UImage> Underline = nullptr;
		float HoverAlpha = 0.0f;
		float PressAlpha = 0.0f;
		bool bDanger = false;
	};

	void Build();
	void TickItemVisual(FPauseMenuItem& Item, float DeltaTime);

	UFUNCTION() void HandleResumeClicked();
	UFUNCTION() void HandleAbortClicked();
	UFUNCTION() void HandleSettingsClicked();

	bool bBuilt = false;
	bool bAborting = false;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlanetText;
	UPROPERTY(Transient) TObjectPtr<UImage> LogoImage;

	FPauseMenuItem ResumeItem;
	FPauseMenuItem AbortItem;
	FPauseMenuItem SettingsItem;
};
