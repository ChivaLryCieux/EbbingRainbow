// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERBattleResult.h"
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

TSharedRef<SWidget> UERBattleResult::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERBattleResult::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage) { Build(); }
	}
	Super::NativeConstruct();
}

void UERBattleResult::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// 全屏压暗
	DimImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	DimImage->SetBrush(ERUI::WhiteBrush());
	DimImage->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.55f));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(DimImage);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// letterbox 黑条
	const float BarH = 110.0f;
	TopBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TopBar->SetBrush(ERUI::WhiteBrush());
	TopBar->SetColorAndOpacity(T->InkScrim);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopBar);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, BarH));
	}
	BottomBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	BottomBar->SetBrush(ERUI::WhiteBrush());
	BottomBar->SetColorAndOpacity(T->InkScrim);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(BottomBar);
		S->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f, -BarH, 0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
	}

	// 中央内容列
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Col);
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	TitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, T->Bone);
	Col->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	TitleRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TitleRule->SetBrush(ERUI::WhiteBrush(FVector2D(80.0f, T->RuleThickness)));
	TitleRule->SetColorAndOpacity(T->Accent);
	{
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TitleRule);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 14.0f));
	}

	SubtitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneTertiary);
	Col->AddChildToVerticalBox(SubtitleText)->SetHorizontalAlignment(HAlign_Center);

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Col->AddChildToVerticalBox(RowsBox)->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));

	PrimaryButton = WidgetTree->ConstructWidget<UERActionButton>(UERActionButton::StaticClass());
	PrimaryButton->OnActionClicked.AddDynamic(this, &UERBattleResult::HandlePrimaryClicked);
	{
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(PrimaryButton);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 34.0f, 0.0f, 0.0f));
	}
}

void UERBattleResult::SetResult(bool bWon, const FText& Title, const FText& Subtitle,
	const TArray<FERResultRow>& Rows, const FText& ButtonLabel)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!DimImage) { Build(); }
	}
	const UERUITheme* T = ERUI::Theme();
	EntranceT = 0.0f;

	TitleText->SetText(Title);
	TitleText->SetColorAndOpacity(FSlateColor(bWon ? T->Bone : ERUI::Fade(T->Bone, 0.55f)));
	TitleRule->SetColorAndOpacity(bWon ? T->Accent : T->BoneGhost);
	SubtitleText->SetText(Subtitle);
	PrimaryButton->SetLabel(ButtonLabel);
	PrimaryButton->SetUrgent(true);

	RowsBox->ClearChildren();
	for (const FERResultRow& R : Rows)
	{
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* L = ERUI::MakeText(this, R.Label, EERTypeScale::Micro, T->BoneTertiary);
		RowBox->AddChildToHorizontalBox(L)->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		UTextBlock* V = ERUI::MakeText(this, R.Value, EERTypeScale::Caption, R.ValueTint, true);
		RowBox->AddChildToHorizontalBox(V);
		UVerticalBoxSlot* S = RowsBox->AddChildToVerticalBox(RowBox);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 2.5f));
	}

	SetVisibility(ESlateVisibility::Visible);
}

void UERBattleResult::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (EntranceT < 1.0f)
	{
		EntranceT = FMath::Min(EntranceT + InDeltaTime / 0.5f, 1.0f);
		const float C = 1.0f - EntranceT;
		const float E = 1.0f - C * C * C;
		SetRenderOpacity(E);
		if (TopBar)
		{
			TopBar->SetRenderTranslation(FVector2D(0.0f, -20.0f * (1.0f - E)));
		}
		if (BottomBar)
		{
			BottomBar->SetRenderTranslation(FVector2D(0.0f, 20.0f * (1.0f - E)));
		}
	}
}

// 按钮回调绑定在 Build 里: AddDynamic 到 HandlePrimaryClicked
void UERBattleResult::HandlePrimaryClicked()
{
	OnPrimaryClicked.Broadcast();
}
