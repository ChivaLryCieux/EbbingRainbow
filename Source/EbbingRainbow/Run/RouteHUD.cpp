// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteHUD.h"
#include "Core/ERLog.h"
#include "UI/ERRouteHUD.h"
#include "UI/ERUITheme.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Run/RouteSceneBuilder.h"
#include "Run/RunSubsystem.h"

void ARouteHUD::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetGameInstance();
	URunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<URunSubsystem>() : nullptr;
	if (!Run || !Run->HasActiveRun())
	{
		UE_LOG(LogERRun, Error, TEXT("[RouteHUD] 没有进行中的 Run，航线图无法显示。"));
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogERRun, Error, TEXT("[RouteHUD] 找不到 PlayerController。"));
		return;
	}

	TSubclassOf<UUserWidget> WidgetClass = UERRouteHUD::StaticClass();
	if (const UERUISettings* UISettings = UERUISettings::Get())
	{
		if (!UISettings->RouteHUDWidgetClass.IsNull())
		{
			if (UClass* Loaded = UISettings->RouteHUDWidgetClass.LoadSynchronous())
			{
				WidgetClass = Loaded;
			}
		}
	}

	RouteWidget = CreateWidget<UERRouteHUD>(PC, WidgetClass);
	if (RouteWidget)
	{
		RouteWidget->AddToViewport(10);
		RouteWidget->InitializeRouteUI(Run);
		RouteWidget->OnNodeSelected.AddUniqueDynamic(this, &ARouteHUD::HandleNodeClicked);
	}

	UE_LOG(LogERRun, Log, TEXT("[RouteHUD] 航线图蓝图已挂到视口：seed=%d 节点=%d 当前节点=%d"),
		Run->GetRunSeed(), Run->GetGraph().Nodes.Num(), Run->GetCurrentNodeId());
}

void ARouteHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RouteWidget)
	{
		RouteWidget->RemoveFromParent();
		RouteWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool ARouteHUD::GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const
{
	return RouteWidget && RouteWidget->GetMapLayout(OutOriginPx, OutScalePx);
}

void ARouteHUD::HandleNodeClicked(int32 NodeId)
{
	UGameInstance* GameInstance = GetGameInstance();
	URunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<URunSubsystem>() : nullptr;
	if (!Run || !Run->IsNodeSelectable(NodeId))
	{
		return;
	}

	const FRouteNode* Node = Run->FindNode(NodeId);
	if (!Node)
	{
		return;
	}

	if (IsRouteNodeCombat(Node->Type))
	{
		Run->SetPendingNode(NodeId);
		UE_LOG(LogERRun, Log, TEXT("[Route] 进入战斗节点 %d (第 %d 层)"), Node->Id, Node->Layer + 1);

		if (RouteWidget)
		{
			RouteWidget->ShowNotification(FText::FromString(TEXT("进入战斗…")), 1.0f);
		}

		// 一场景一关卡: 自然战斗开自然岛关卡, 其余开混凝土浮岛关卡
		const TCHAR* BattleMap = (Node->Type == ERouteNodeType::BattleNature)
			? TEXT("/Game/Battle/Maps/Lvl_NatureArena")
			: TEXT("/Game/Battle/Maps/Lvl_ConcreteArena");

		UGameplayStatics::OpenLevel(this, FName(BattleMap), true,
			FString::Printf(TEXT("game=/Script/EbbingRainbow.BattleGameMode?node=%d"), NodeId));
		return;
	}

	// 安全节点：直接前进，无事发生
	if (Run->AdvanceToNode(NodeId))
	{
		UE_LOG(LogERRun, Log, TEXT("[Route] 安全节点 %d (第 %d 层)，无事发生"), Node->Id, Node->Layer + 1);
		if (RouteWidget)
		{
			RouteWidget->Refresh();
			RouteWidget->ShowNotification(FText::FromString(TEXT("安全节点 · 无事发生")));
		}
		for (TActorIterator<ARouteSceneBuilder> It(GetWorld()); It; ++It)
		{
			It->BuildScene(Run);
		}
	}
}
