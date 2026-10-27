// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/UI/SHallMainOverlay.h"
#include "Hall/UI/HallGlassButton.h"
#include "Kismet/GameplayStatics.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/SlateRenderTransform.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void SHallMainOverlay::Construct(const FArguments& InArgs)
{
	OwningWorld = InArgs._OwningWorld;

	// 观景模式提示胶囊: 与按钮一致的烟熏玻璃质感
	ZenHintBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	ZenHintBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.06f, 0.09f, 0.55f));
	ZenHintBrush.OutlineSettings.CornerRadii = FVector4(18.0f, 18.0f, 18.0f, 18.0f);
	ZenHintBrush.OutlineSettings.Width = 1.0f;
	ZenHintBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.90f, 0.95f, 1.0f, 0.16f));

	ChildSlot
	[
		SNew(SOverlay)

		// =================================================================
		// 主界面层: 底部居中双按钮玻璃 Dock (观景模式下整体隐藏)
		// =================================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(MainDashboardContainer, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
			.Padding(0.0f)
			[
				SNew(SOverlay)

				// 底部居中: [人员管理] + [航线管理]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 56.0f))
				[
					SAssignNew(DockContainer, SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					.Padding(0.0f)
					[
						SNew(SHorizontalBox)

						// 人员管理: 次级玻璃按钮 (银蓝指示灯, 暂无功能)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SHallGlassButton)
							.Label(FText::FromString(TEXT("人员管理")))
							.Caption(FText::FromString(TEXT("P E R S O N N E L")))
							.AccentColor(FLinearColor(0.68f, 0.78f, 0.94f, 1.0f))
							.bPrimary(false)
							.ButtonWidth(264.0f)
							.OnClicked(this, &SHallMainOverlay::HandlePersonnelClicked)
						]

						// 双按钮间距
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(24.0f)
						]

						// 航线管理: 主玻璃按钮 (青色呼吸光晕, 临时作为开始战斗入口)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SHallGlassButton)
							.Label(FText::FromString(TEXT("航线管理")))
							.Caption(FText::FromString(TEXT("R O U T E S")))
							.AccentColor(FLinearColor(0.32f, 0.76f, 1.0f, 1.0f))
							.bPrimary(true)
							.ButtonWidth(264.0f)
							.OnClicked(this, &SHallMainOverlay::HandleRoutesClicked)
						]
					]
				]
			]
		]

		// =================================================================
		// 全屏观景模式提示胶囊 (Zen Hint)
		// =================================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
		[
			SAssignNew(ZenHintBorder, SBorder)
			.BorderImage(&ZenHintBrush)
			.Padding(FMargin(22.0f, 9.0f))
			.Visibility(EVisibility::Hidden)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("观景模式 · 点击任意位置或按 Esc 返回界面")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.ColorAndOpacity(FLinearColor(0.88f, 0.93f, 1.0f, 0.85f))
			]
		]
	];
}

void SHallMainOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 底部 Dock 入场动画: 上浮 + 淡入 (ease-out cubic, 约 0.9s)
	if (EntranceProgress < 1.0f)
	{
		EntranceProgress = FMath::Min(EntranceProgress + InDeltaTime / 0.9f, 1.0f);
		const float Eased = 1.0f - FMath::Pow(1.0f - EntranceProgress, 3.0f);
		if (DockContainer.IsValid())
		{
			DockContainer->SetRenderOpacity(Eased);
			DockContainer->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, (1.0f - Eased) * 34.0f)));
		}
	}
}

FReply SHallMainOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// 观景模式下点击任意位置恢复 UI
	if (bZenMode)
	{
		ToggleZenMode();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SHallMainOverlay::ToggleZenMode()
{
	bZenMode = !bZenMode;
	if (MainDashboardContainer.IsValid())
	{
		MainDashboardContainer->SetVisibility(bZenMode ? EVisibility::Hidden : EVisibility::Visible);
	}
	if (ZenHintBorder.IsValid())
	{
		ZenHintBorder->SetVisibility(bZenMode ? EVisibility::Visible : EVisibility::Hidden);
	}
}

UWorld* SHallMainOverlay::FindPlayWorld() const
{
	if (OwningWorld.IsValid())
	{
		return OwningWorld.Get();
	}
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				if (UWorld* CandidateWorld = Context.World())
				{
					return CandidateWorld;
				}
			}
		}
		return GEngine->GetCurrentPlayWorld();
	}
	return nullptr;
}

void SHallMainOverlay::LaunchBattleLevel()
{
	UWorld* World = FindPlayWorld();
	if (World)
	{
		UE_LOG(LogTemp, Log, TEXT("[Hall] Launching Battle Level (/Game/Battle/Maps/Lvl_Battle)..."));
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Battle")), true, TEXT("game=/Script/EbbingRainbow.BattleGameMode"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Hall] LaunchBattleLevel failed: could not resolve active UWorld!"));
	}
}

FReply SHallMainOverlay::HandlePersonnelClicked()
{
	// 人员管理: 功能待实现
	UE_LOG(LogTemp, Log, TEXT("[Hall] Personnel Management clicked (not implemented yet)."));
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleRoutesClicked()
{
	// 航线管理: 临时作为开始战斗入口
	UE_LOG(LogTemp, Log, TEXT("[Hall] Routes Management clicked -> Start Battle."));
	LaunchBattleLevel();
	return FReply::Handled();
}
