// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RoutePlayerController.h"
#include "Core/ERLog.h"

#include "Run/RouteGraphGenerator.h"
#include "Run/RouteHUD.h"
#include "Run/RunSubsystem.h"

ARoutePlayerController::ARoutePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ARoutePlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ARoutePlayerController::RouteDump()
{
	URunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunSubsystem>() : nullptr;
	if (!Run || !Run->HasActiveRun())
	{
		UE_LOG(LogERRun, Warning, TEXT("[RouteDump] 没有进行中的 Run。"));
		return;
	}

	const FRouteGraph& Graph = Run->GetGraph();
	UE_LOG(LogERRun, Log, TEXT("[RouteDump] %s"), *FRouteGraphGenerator::MakeReport(Graph));

	for (const FRouteNode& Node : Graph.Nodes)
	{
		FString Next;
		for (const int32 NextId : Node.Next)
		{
			Next += FString::Printf(TEXT("%d "), NextId);
		}
		UE_LOG(LogERRun, Log, TEXT("[RouteDump] id=%d L=%d S=%d T=%d visited=%d next[%s]"),
			Node.Id, Node.Layer, Node.Slot, static_cast<int32>(Node.Type), Node.bVisited ? 1 : 0, *Next);
	}
}

void ARoutePlayerController::RouteSelect(int32 NodeId)
{
	if (ARouteHUD* RouteHUD = Cast<ARouteHUD>(GetHUD()))
	{
		RouteHUD->HandleNodeClicked(NodeId);
		return;
	}

	UE_LOG(LogERRun, Warning, TEXT("[RouteSelect] 当前 HUD 不是 ARouteHUD。"));
}
