// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERBattleResult.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/ERActionButton.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

// ============================================================
// UERBattleResultRow
// ============================================================

TSharedRef<SWidget> UERBattleResultRow::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !ValueText) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERBattleResultRow::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !ValueText) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		FERResultRow Preview;
		Preview.Label = PreviewLabel;
		Preview.Value = PreviewValue;
		Preview.ValueTint = PreviewValueTint;
		SetRowData(Preview);
	}
}

void UERBattleResultRow::Build()
{
	const UERUITheme* T = ERUI::Theme();
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowBox"));
	WidgetTree->RootWidget = RowBox;
	RowBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	LabelText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	UHorizontalBoxSlot* LSlot = RowBox->AddChildToHorizontalBox(LabelText);
	LSlot->SetVerticalAlignment(VAlign_Center);
	LSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

	ValueText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->Bone, true);
	RowBox->AddChildToHorizontalBox(ValueText)->SetVerticalAlignment(VAlign_Center);
}

void UERBattleResultRow::SetRowData(const FERResultRow& InRow)
{
	if (LabelText)
	{
		LabelText->SetText(InRow.Label);
	}
	if (ValueText)
	{
		ValueText->SetText(InRow.Value);
		ValueText->SetColorAndOpacity(FSlateColor(InRow.ValueTint));
	}
}

// ============================================================
// UERBattleResult
// ============================================================

TSharedRef<SWidget> UERBattleResult::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage && !TitleText) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERBattleResult::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage && !TitleText) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		const UERUITheme* T = ERUI::Theme();
		if (TitleText)
		{
			TitleText->SetText(bPreviewVictory ? PreviewVictoryTitle : PreviewDefeatTitle);
			TitleText->SetColorAndOpacity(FSlateColor(bPreviewVictory ? T->Bone : ERUI::Fade(T->Bone, 0.55f)));
		}
		if (TitleRule)
		{
			TitleRule->SetColorAndOpacity(bPreviewVictory ? T->Accent : T->BoneGhost);
		}
		if (SubtitleText)
		{
			SubtitleText->SetText(PreviewSubtitle);
		}
		if (PrimaryButton)
		{
			PrimaryButton->SetLabel(PreviewButtonLabel);
			PrimaryButton->SetUrgent(true);
		}

		TArray<FERResultRow> PreviewRows;
		PreviewRows.Add({ INVTEXT("回合"), INVTEXT("4"), T->Bone });
		PreviewRows.Add({ INVTEXT("存活"), INVTEXT("3 / 3"), T->Bone });
		PreviewRows.Add({ INVTEXT("完整度"), INVTEXT("248 / 300"), T->BoneSecondary });
		PopulateRows(PreviewRows);
	}
}

void UERBattleResult::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage && !TitleText) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();
}

void UERBattleResult::BindDelegates()
{
	// 委托绑定独立于 Build(): WBP 提供 PrimaryButton 时同样生效
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (PrimaryButton)
	{
		PrimaryButton->OnActionClicked.AddDynamic(this, &UERBattleResult::HandlePrimaryClicked);
	}
}

void UERBattleResult::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootPanel"));
	WidgetTree->RootWidget = Root;

	// 全屏压暗
	DimImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimImage"));
	DimImage->SetBrush(ERUI::WhiteBrush());
	DimImage->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.55f));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(DimImage);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// letterbox 黑条
	const float BarH = 110.0f;
	TopBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TopBar"));
	TopBar->SetBrush(ERUI::WhiteBrush());
	TopBar->SetColorAndOpacity(T->InkScrim);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopBar);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, BarH));
	}
	BottomBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BottomBar"));
	BottomBar->SetBrush(ERUI::WhiteBrush());
	BottomBar->SetColorAndOpacity(T->InkScrim);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(BottomBar);
		S->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f, -BarH, 0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
	}

	// 中央内容列
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Col);
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	TitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, T->Bone);
	Col->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	TitleRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TitleRule"));
	TitleRule->SetBrush(ERUI::WhiteBrush(FVector2D(80.0f, T->RuleThickness)));
	TitleRule->SetColorAndOpacity(T->Accent);
	{
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TitleRule);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 14.0f));
	}

	SubtitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneTertiary);
	Col->AddChildToVerticalBox(SubtitleText)->SetHorizontalAlignment(HAlign_Center);

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RowsBox"));
	Col->AddChildToVerticalBox(RowsBox)->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));

	PrimaryButton = WidgetTree->ConstructWidget<UERActionButton>(UERActionButton::StaticClass(), TEXT("PrimaryButton"));
	{
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(PrimaryButton);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 34.0f, 0.0f, 0.0f));
	}
}

void UERBattleResult::PopulateRows(const TArray<FERResultRow>& Rows)
{
	if (!RowsBox || !WidgetTree)
	{
		return;
	}
	RowsBox->ClearChildren();
	const TSubclassOf<UERBattleResultRow> RowCls = RowWidgetClass
		? RowWidgetClass
		: TSubclassOf<UERBattleResultRow>(UERBattleResultRow::StaticClass());

	for (const FERResultRow& R : Rows)
	{
		UERBattleResultRow* RowWidget = WidgetTree->ConstructWidget<UERBattleResultRow>(RowCls);
		if (!RowWidget)
		{
			continue;
		}
		RowWidget->SetRowData(R);
		UVerticalBoxSlot* S = RowsBox->AddChildToVerticalBox(RowWidget);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, RowSpacing * 0.5f));
	}
}

void UERBattleResult::SetResult(bool bWon, const FText& Title, const FText& Subtitle,
	const TArray<FERResultRow>& Rows, const FText& ButtonLabel)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage && !TitleText) { Build(); }
	}
	const UERUITheme* T = ERUI::Theme();
	EntranceT = bEnableEntranceAnimation ? 0.0f : 1.0f;

	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetColorAndOpacity(FSlateColor(bWon ? T->Bone : ERUI::Fade(T->Bone, 0.55f)));
	}
	if (TitleRule)
	{
		TitleRule->SetColorAndOpacity(bWon ? T->Accent : T->BoneGhost);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
	}
	if (PrimaryButton)
	{
		PrimaryButton->SetLabel(ButtonLabel);
		PrimaryButton->SetUrgent(true);
	}

	PopulateRows(Rows);
	SetVisibility(ESlateVisibility::Visible);

	if (Anim_Intro && !IsDesignTime())
	{
		PlayAnimationForward(Anim_Intro);
	}
	if (!IsDesignTime())
	{
		BP_OnResultShown(bWon);
	}
}

void UERBattleResult::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bEnableEntranceAnimation && EntranceT < 1.0f)
	{
		const float Dur = FMath::Max(0.05f, EntranceDuration);
		EntranceT = FMath::Min(EntranceT + InDeltaTime / Dur, 1.0f);
		const float C = 1.0f - EntranceT;
		const float E = 1.0f - C * C * C;
		SetRenderOpacity(E);
		if (TopBar)
		{
			TopBar->SetRenderTranslation(FVector2D(0.0f, -LetterboxSlideOffset * (1.0f - E)));
		}
		if (BottomBar)
		{
			BottomBar->SetRenderTranslation(FVector2D(0.0f, LetterboxSlideOffset * (1.0f - E)));
		}
	}
}

void UERBattleResult::HandlePrimaryClicked()
{
	OnPrimaryClicked.Broadcast();
}
