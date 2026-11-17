// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERInteractiveWidget.h"
#include "Components/Widget.h"

void UERInteractiveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Registry().AddUnique(this);
}

void UERInteractiveWidget::NativeDestruct()
{
	Registry().RemoveAll([this](const TWeakObjectPtr<UERInteractiveWidget>& W)
	{
		return !W.IsValid() || W.Get() == this;
	});
	Super::NativeDestruct();
}

bool UERInteractiveWidget::IsAnyHovered()
{
	bool bAny = false;
	Registry().RemoveAll([](const TWeakObjectPtr<UERInteractiveWidget>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<UERInteractiveWidget>& W : Registry())
	{
		UERInteractiveWidget* IW = W.Get();
		if (!IW || IW->GetVisibility() != ESlateVisibility::Visible)
		{
			continue;
		}
		UWidget* Check = IW->GetHoverCheckWidget();
		if (Check && Check->IsHovered())
		{
			bAny = true;
			break;
		}
	}
	return bAny;
}
