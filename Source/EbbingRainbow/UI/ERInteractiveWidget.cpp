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

bool UERInteractiveWidget::IsInteractiveHovered() const
{
	const UWidget* Check = GetHoverCheckWidget();
	return Check && Check->IsHovered();
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
		if (IW->IsInteractiveHovered())
		{
			bAny = true;
			break;
		}
	}
	return bAny;
}
