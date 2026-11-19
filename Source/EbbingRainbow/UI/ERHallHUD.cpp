// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHallHUD.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Core/ERLog.h"
#include "Hall/HallHUD.h"
#include "UI/ERHallButton.h"
#include "UI/ERUIStatics.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Run/RunSubsystem.h"

TSharedRef<SWidget> UERHallHUD::RebuildWidget()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERHallHUD::NativePreConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativePreConstruct();

	if (ZenHintText && !ZenHintMessage.IsEmpty())
	{
		ZenHintText->SetText(ZenHintMessage);
	}
	if (IsDesignTime())
	{
		ApplyZenState(bPreviewZenMode);
	}
}

void UERHallHUD::NativeConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();

	if (!bBaseCached)
	{
		bBaseCached = true;
		if (DockBox)
		{
			BaseDockTranslation = DockBox->GetRenderTransform().Translation;
			BaseDockOpacity = DockBox->GetRenderOpacity();
		}
	}

	if (PersonnelButton)
	{
		PersonnelButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandlePersonnelClicked);
	}
	if (RoutesButton)
	{
		RoutesButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleRoutesClicked);
	}
	if (ZenHintText && !ZenHintMessage.IsEmpty())
	{
		ZenHintText->SetText(ZenHintMessage);
	}

	ApplyZenState(false);
	EntranceProgress = bEnableEntranceAnimation ? 0.0f : 1.0f;
	if (Anim_Intro)
	{
		PlayAnimationForward(Anim_Intro);
	}
}

void UERHallHUD::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	if (!RootCanvas)
	{
		if (UCanvasPanel* Existing = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			RootCanvas = Existing;
		}
		else if (!WidgetTree->RootWidget)
		{
			RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WidgetTree->RootWidget = RootCanvas;
		}
	}
	if (!RootCanvas)
	{
		return;
	}
	RootCanvas->SetVisibility(ESlateVisibility::Visible);

	if (!DockBox)
	{
		DockBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DockBox"));
		UCanvasPanelSlot* DockSlot = RootCanvas->AddChildToCanvas(DockBox);
		DockSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		DockSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		DockSlot->SetPosition(FVector2D(0.0f, -56.0f));
		DockSlot->SetAutoSize(true);

		PersonnelButton = WidgetTree->ConstructWidget<UERHallButton>(UERHallButton::StaticClass(), TEXT("PersonnelButton"));
		PersonnelButton->Label = INVTEXT("人员管理");
		PersonnelButton->Caption = INVTEXT("P E R S O N N E L");
		PersonnelButton->AccentColor = FLinearColor(0.68f, 0.78f, 0.94f, 1.0f);
		PersonnelButton->bPrimary = false;
		DockBox->AddChildToHorizontalBox(PersonnelButton);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DockSpacer"));
		Gap->SetSize(FVector2D(24.0f, 1.0f));
		DockBox->AddChildToHorizontalBox(Gap);

		RoutesButton = WidgetTree->ConstructWidget<UERHallButton>(UERHallButton::StaticClass(), TEXT("RoutesButton"));
		RoutesButton->Label = INVTEXT("航线管理");
		RoutesButton->Caption = INVTEXT("R O U T E S");
		RoutesButton->AccentColor = FLinearColor(0.32f, 0.76f, 1.0f, 1.0f);
		RoutesButton->bPrimary = true;
		DockBox->AddChildToHorizontalBox(RoutesButton);
	}

	if (!DashboardContainer)
	{
		DashboardContainer = DockBox;
	}

	if (!ZenHintBorder)
	{
		ZenHintBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ZenHintBorder"));
		FSlateBrush PillBrush;
		PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PillBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.06f, 0.09f, 0.58f));
		PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PillBrush.OutlineSettings.CornerRadii = FVector4(18.0f, 18.0f, 18.0f, 18.0f);
		PillBrush.OutlineSettings.Width = 1.0f;
		PillBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.90f, 0.95f, 1.0f, 0.16f));
		ZenHintBorder->SetBrush(PillBrush);
		ZenHintBorder->SetBrushColor(FLinearColor::White);
		ZenHintBorder->SetPadding(FMargin(22.0f, 9.0f));
		ZenHintBorder->SetVisibility(ESlateVisibility::Hidden);

		ZenHintText = ERUI::MakeText(this, ZenHintMessage, EERTypeScale::Caption, FLinearColor(0.88f, 0.93f, 1.0f, 0.85f));
		FSlateFontInfo HintFont = T->GetFont(EERTypeScale::Caption);
		HintFont.Size = 11;
		HintFont.LetterSpacing = 40;
		ZenHintText->SetFont(HintFont);
		ZenHintBorder->SetContent(ZenHintText);

		UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(ZenHintBorder);
		HintSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		HintSlot->SetPosition(FVector2D(0.0f, -28.0f));
		HintSlot->SetAutoSize(true);
	}
}

void UERHallHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bEnableEntranceAnimation && EntranceProgress < 1.0f)
	{
		const float Dur = FMath::Max(0.05f, EntranceDuration);
		EntranceProgress = FMath::Min(EntranceProgress + InDeltaTime / Dur, 1.0f);
		const float Eased = 1.0f - FMath::Pow(1.0f - EntranceProgress, 3.0f);
		if (DockBox)
		{
			DockBox->SetRenderOpacity(BaseDockOpacity * Eased);
			DockBox->SetRenderTranslation(BaseDockTranslation + FVector2D(0.0f, (1.0f - Eased) * EntranceOffsetY));
		}
	}
}

FReply UERHallHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bZenMode)
	{
		ToggleZenMode();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UERHallHUD::ToggleZenMode()
{
	SetZenMode(!bZenMode);
}

void UERHallHUD::SetZenMode(bool bInZenMode)
{
	bZenMode = bInZenMode;
	ApplyZenState(bZenMode);
	if (Anim_ZenEnter && !IsDesignTime())
	{
		PlayAnimation(Anim_ZenEnter, 0.0f, 1, bZenMode ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse);
	}
	if (!IsDesignTime())
	{
		BP_OnZenModeChanged(bZenMode);
	}
}

void UERHallHUD::ApplyZenState(bool bInZen)
{
	if (DashboardContainer)
	{
		DashboardContainer->SetVisibility(bInZen ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	else if (DockBox)
	{
		DockBox->SetVisibility(bInZen ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ZenHintBorder)
	{
		ZenHintBorder->SetVisibility(bInZen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

UWorld* UERHallHUD::ResolvePlayWorld() const
{
	if (UWorld* W = GetWorld())
	{
		return W;
	}
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				if (UWorld* Candidate = Context.World())
				{
					return Candidate;
				}
			}
		}
		return GEngine->GetCurrentPlayWorld();
	}
	return nullptr;
}

void UERHallHUD::LaunchRouteMap()
{
	UWorld* World = ResolvePlayWorld();
	if (!World)
	{
		UE_LOG(LogERHall, Error, TEXT("[Hall] LaunchRouteMap failed: could not resolve active UWorld!"));
		return;
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (URunSubsystem* Run = GameInstance->GetSubsystem<URunSubsystem>())
		{
			if (!Run->HasActiveRun())
			{
				Run->StartNewRandomRun();
			}
		}
	}

	UE_LOG(LogERHall, Log, TEXT("[Hall] Launching Route Map (%s)..."), *RouteLevelName.ToString());
	UGameplayStatics::OpenLevel(World, RouteLevelName, true, RouteLevelOptions);
}

void UERHallHUD::HandlePersonnelClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Personnel Management clicked."));
	BP_OnPersonnelClicked();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenPersonnelPanel();
		}
		else
		{
			UE_LOG(LogERHall, Warning, TEXT("[Hall] Personnel panel skipped: HUD is not AHallHUD."));
		}
	}
}

void UERHallHUD::HandleRoutesClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Routes Management clicked -> Route Map."));
	BP_OnRoutesClicked();
	LaunchRouteMap();
}
