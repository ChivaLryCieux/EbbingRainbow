// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUnitToken.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Battle/Units/BattleUnit.h"
#include "Components/Button.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UERUnitToken::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(ERUI::Theme()->SquadChipSize); }
	}
	return Super::RebuildWidget();
}

void UERUnitToken::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(ERUI::Theme()->SquadChipSize); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		ApplyPreview();
	}
}

void UERUnitToken::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(ERUI::Theme()->SquadChipSize); }
	}
	BindDelegates();
	Super::NativeConstruct();
}

void UERUnitToken::BindDelegates()
{
	// 委托绑定独立于 Build(): WBP 提供 HitButton 时同样生效
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UERUnitToken::HandleClicked);
		HitButton->OnHovered.AddDynamic(this, &UERUnitToken::HandleHoverBegin);
		HitButton->OnUnhovered.AddDynamic(this, &UERUnitToken::HandleHoverEnd);
	}
}

void UERUnitToken::NativeDestruct()
{
	Super::NativeDestruct();
}

void UERUnitToken::Build(float ChipSize)
{
	bBuiltInCode = true;
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HitButton"));
	FButtonStyle BtnStyle;
	const FSlateBrush Transparent = ERUI::TransparentBrush();
	BtnStyle.SetNormal(Transparent);
	BtnStyle.SetHovered(Transparent);
	BtnStyle.SetPressed(Transparent);
	BtnStyle.SetDisabled(Transparent);
	HitButton->SetStyle(BtnStyle);
	WidgetTree->RootWidget = HitButton;

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	HitButton->SetContent(Col);

	// 队伍色刻度
	TeamTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TeamTick"));
	TeamTick->SetBrush(ERUI::WhiteBrush(FVector2D(ChipSize, 3.0f)));
	TeamTick->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Col->AddChildToVerticalBox(TeamTick)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));

	// 方块主体: 首字符
	ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChipBox"));
	ChipBox->SetWidthOverride(ChipSize);
	ChipBox->SetHeightOverride(ChipSize);
	{
		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Ov"));
		GlyphText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
		UOverlaySlot* S = Ov->AddChildToOverlay(GlyphText);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
		ChipBox->SetContent(Ov);
	}
	Col->AddChildToVerticalBox(ChipBox);

	// 2px 血线
	HpLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpLine"));
	{
		FProgressBarStyle PBStyle;
		PBStyle.SetBackgroundImage(ERUI::WhiteBrush());
		PBStyle.BackgroundImage.TintColor = FSlateColor(T->BoneGhost);
		PBStyle.SetFillImage(ERUI::WhiteBrush());
		HpLine->SetWidgetStyle(PBStyle);
		HpLine->SetFillColorAndOpacity(T->Bone);
		HpLine->SetPercent(1.0f);
		HpLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	{
		USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LineBox"));
		LineBox->SetHeightOverride(2.0f);
		LineBox->SetContent(HpLine);
		Col->AddChildToVerticalBox(LineBox)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
	}

	// 当前回合金线
	ActiveRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ActiveRule"));
	ActiveRule->SetBrush(ERUI::WhiteBrush(FVector2D(ChipSize, 2.0f)));
	ActiveRule->SetColorAndOpacity(T->Accent);
	ActiveRule->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(ActiveRule)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	// 名字 (默认折叠)
	NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary);
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
}

void UERUnitToken::ApplyPreview()
{
	const UERUITheme* T = ERUI::Theme();
	bActive = bPreviewActive;
	bFocused = bPreviewFocused;
	bShowName = true;

	if (GlyphText)
	{
		GlyphText->SetText(PreviewGlyph);
		GlyphText->SetColorAndOpacity(FSlateColor(bPreviewActive ? T->Accent : T->Bone));
	}
	if (NameText)
	{
		NameText->SetText(PreviewName);
		NameText->SetColorAndOpacity(FSlateColor(bPreviewFocused ? T->Accent : T->BoneSecondary));
	}
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(bPreviewIsPlayer ? T->AllyMark : T->EnemyMark);
	}
	const float Pct = FMath::Clamp(PreviewHpPercent, 0.0f, 1.0f);
	if (HpLine)
	{
		HpLine->SetPercent(Pct);
		HpLine->SetFillColorAndOpacity(Pct < LowHpThreshold ? T->DangerColor : T->Bone);
	}
	if (PortraitImage)
	{
		if (UMaterialInstanceDynamic* MID = PortraitImage->GetDynamicMaterial())
		{
			if (DefaultPortrait)
			{
				MID->SetTextureParameterValue(TEXT("Portrait"), DefaultPortrait);
			}
			MID->SetScalarParameterValue(TEXT("GrayFraction"), 1.0f - Pct);
		}
	}
	if (ActiveRule)
	{
		ActiveRule->SetVisibility(bPreviewActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (FocusedFrame)
	{
		FocusedFrame->SetVisibility(bPreviewFocused ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetRenderOpacity(bPreviewAlive ? 1.0f : DeadOpacity);
	UpdateNameVisibility();
}

void UERUnitToken::Setup(ABattleUnit* InUnit, bool bInIsSquad)
{
	Unit = InUnit;
	bIsSquad = bInIsSquad;
	bShowName = bInIsSquad || bAlwaysShowName;

	// 默认尊重 WBP Designer 的宽高与字号; 仅纯 C++ 回退或显式勾选覆盖时才按 Theme 重设尺寸
	if (bBuiltInCode || bOverrideSizeFromTheme)
	{
		const float ChipSize = bIsSquad ? ERUI::Theme()->SquadChipSize : ERUI::Theme()->ChipSize;
		if (TeamTick)   { TeamTick->SetDesiredSizeOverride(FVector2D(ChipSize, 3.0f)); }
		if (ActiveRule) { ActiveRule->SetDesiredSizeOverride(FVector2D(ChipSize, 2.0f)); }
		if (ChipBox)    { ChipBox->SetWidthOverride(ChipSize); ChipBox->SetHeightOverride(ChipSize); }
	}
	if ((bBuiltInCode || bOverrideGlyphFontFromTheme) && GlyphText)
	{
		const UERUITheme* T = ERUI::Theme();
		GlyphText->SetFont(T->GetFont(bIsSquad ? EERTypeScale::Title : EERTypeScale::Caption));
	}

	RefreshFromUnit();
	UpdateNameVisibility();
}

void UERUnitToken::RefreshFromUnit()
{
	if (!Unit)
	{
		return;
	}
	const UERUITheme* T = ERUI::Theme();
	const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);

	if (GlyphText)
	{
		const FString& Name = Unit->Stats.DisplayName;
		GlyphText->SetText(FText::FromString(Name.Left(1)));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(Unit->Stats.DisplayName));
	}
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(bIsPlayer ? T->AllyMark : T->EnemyMark);
	}

	const float MaxHp = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
	const float Pct = (MaxHp > 0.0f) ? FMath::Clamp(Unit->Integrity / MaxHp, 0.0f, 1.0f) : 0.0f;
	if (HpLine)
	{
		HpLine->SetPercent(Pct);
		HpLine->SetFillColorAndOpacity(Pct < LowHpThreshold ? T->DangerColor : T->Bone);
	}
	if (PortraitImage)
	{
		if (UMaterialInstanceDynamic* MID = PortraitImage->GetDynamicMaterial())
		{
			if (DefaultPortrait)
			{
				MID->SetTextureParameterValue(TEXT("Portrait"), DefaultPortrait);
			}
			MID->SetScalarParameterValue(TEXT("GrayFraction"), 1.0f - Pct);
		}
	}

	ApplyDeadVisual();
	UpdateNameVisibility();
	BP_OnUnitRefreshed(Unit, Pct, bActive, bFocused);
}

void UERUnitToken::ApplyDeadVisual()
{
	if (!Unit)
	{
		return;
	}
	SetRenderOpacity(Unit->IsAlive() ? 1.0f : DeadOpacity);
}

void UERUnitToken::SetActiveFlag(bool bInActive)
{
	bActive = bInActive;
	if (ActiveRule)
	{
		ActiveRule->SetVisibility(bInActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (GlyphText)
	{
		GlyphText->SetColorAndOpacity(FSlateColor(bInActive ? ERUI::Theme()->Accent : ERUI::Theme()->Bone));
	}
	UpdateNameVisibility();
}

void UERUnitToken::SetFocusedFlag(bool bInFocused)
{
	bFocused = bInFocused;
	if (FocusedFrame)
	{
		FocusedFrame->SetVisibility(bInFocused ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (NameText)
	{
		NameText->SetColorAndOpacity(FSlateColor(bInFocused ? ERUI::Theme()->Accent : ERUI::Theme()->BoneSecondary));
	}
	UpdateNameVisibility();
}

void UERUnitToken::UpdateNameVisibility()
{
	if (!NameText)
	{
		return;
	}
	const bool bShow = bAlwaysShowName || (bShowName && (bActive || bFocused || bHovered || IsDesignTime()));
	NameText->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UERUnitToken::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Target = (bActive || bFocused || bHovered) ? 1.0f : 0.0f;
	MotionSpring.SetTarget(Target);
	const float M = MotionSpring.Tick(InDeltaTime);
	const float M01 = FMath::Clamp(M, 0.0f, 1.0f);

	if (bIsSquad)
	{
		SetRenderTranslation(FVector2D(6.0f * M, 0.0f));
	}
	else
	{
		SetRenderTranslation(FVector2D(0.0f, 4.0f * M));
	}

	if (TokenFrame)
	{
		const UERUITheme* T = ERUI::Theme();
		const FLinearColor Border = bActive ? T->Accent : (bFocused ? T->Bone : FLinearColor(0.92f, 0.92f, 0.90f, 0.20f + 0.55f * M01));
		TokenFrame->SetFrameColors(
			FLinearColor(0.008f, 0.009f, 0.012f, 0.90f),
			Border,
			bActive ? T->Accent : T->Bone,
			bActive ? T->Accent : T->BoneGhost);
		TokenFrame->SetMotionState(1.0f, M01, 1.0f);
	}
}

void UERUnitToken::HandleClicked()
{
	OnTokenClicked.Broadcast(Unit);
}

void UERUnitToken::HandleHoverBegin()
{
	bHovered = true;
	UpdateNameVisibility();
}

void UERUnitToken::HandleHoverEnd()
{
	bHovered = false;
	UpdateNameVisibility();
}

UWidget* UERUnitToken::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
