// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/UI/HallMainWidget.h"
#include "Hall/UI/SHallMainOverlay.h"

void UHallMainWidget::ToggleZenMode()
{
	if (SlateOverlay.IsValid())
	{
		SlateOverlay->ToggleZenMode();
	}
}

TSharedRef<SWidget> UHallMainWidget::RebuildWidget()
{
	SlateOverlay = SNew(SHallMainOverlay);
	return SlateOverlay.ToSharedRef();
}
