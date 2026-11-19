// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Player/BattlePlayerController.h"
#include "Core/ERLog.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Terrain/BattleTerrainQuery.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/UI/BattleHUD.h"
#if ER_WITH_AI_COMMAND
#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#endif
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

/** 调试：>0 时我方回合 1.2 秒后自动结束，让 AI 自己把整场打完（观察镜头跟随 / 战斗流程用）。 */
static TAutoConsoleVariable<int32> CVarAutoPlay(
	TEXT("ER.AutoPlay"), 0, TEXT("调试：>0 时我方回合 1.2 秒后自动结束。"), ECVF_Cheat);

ABattlePlayerController::ABattlePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ABattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	// 监听 GameMode 完成以绑定先攻轮换镜头聚焦
	if (ABattleGameMode* GM = Cast<ABattleGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnBattleSetupComplete.AddLambda([this]() {
			BindTurnFocus();
		});
	}

	// 竞技场是在 InitGame 阶段同步生成的, 等控制器 BeginPlay 时委托早已广播过,
	// 所以这里必须主动补绑一次, 否则开局首回合与后续轮换都不会自动聚焦
	BindTurnFocus();
}

void ABattlePlayerController::BindTurnFocus()
{
	if (bTurnFocusBound)
	{
		return;
	}

	if (UBattleManager* Mgr = GetBattleManager())
	{
		Mgr->OnTurnStarted.AddDynamic(this, &ABattlePlayerController::HandleTurnStarted);
		bTurnFocusBound = true;

		// 当前回合单位立即聚焦 (Pawn 可能还没附体, 由 PlayerTick 补执行)
		if (ABattleUnit* Active = Mgr->GetActiveUnit())
		{
			HandleTurnStarted(Active);
		}
	}
}

void ABattlePlayerController::HandleTurnStarted(ABattleUnit* Unit)
{
	// 每进入一个新单位的回合, 无论敌我, 摄像机都特写锁定该棋子模型
	if (!Unit)
	{
		return;
	}

	// 先记下来: 摄像机 Pawn 可能尚未附体, 等首个 Tick 再真正执行
	PendingAutoFocusUnit = Unit;

	ApplyAutoFocus();
}

void ABattlePlayerController::ApplyAutoFocus()
{
	ABattleUnit* Unit = PendingAutoFocusUnit.Get();
	if (!Unit)
	{
		return;
	}

	ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn());
	if (!Cam)
	{
		return; // 还没附体, 保留 PendingAutoFocusUnit 等下一Tick
	}

	PendingAutoFocusUnit = nullptr;

	// 记入聚焦单位, 让编队面板同步高亮当前回合的我方单位
	FocusedUnit = Unit;
	Cam->FocusOnUnit(Unit);
}

void ABattlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABattlePlayerController::OnLeftClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABattlePlayerController::OnRightClick);

	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ABattlePlayerController::OnKeyM);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABattlePlayerController::OnKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABattlePlayerController::OnKey2);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ABattlePlayerController::OnKeySpace);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ABattlePlayerController::OnKeySpace);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABattlePlayerController::CancelCurrentAction);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ABattlePlayerController::OnKeyF);
}

UBattleManager* ABattlePlayerController::GetBattleManager() const
{
	if (UWorld* World = GetWorld())
	{
		if (ABattleGameMode* GM = Cast<ABattleGameMode>(World->GetAuthGameMode()))
		{
			return GM->GetBattleManager();
		}
	}
	return nullptr;
}

bool ABattlePlayerController::RaycastUnderCursor(FHitResult& OutHit, ECollisionChannel Channel) const
{
	return GetHitResultUnderCursor(Channel, false, OutHit);
}

void ABattlePlayerController::SetInteractionMode(EPlayerInteractionMode NewMode)
{
	InteractionMode = NewMode;
	if (InteractionMode != EPlayerInteractionMode::TargetingSkill)
	{
		SelectedSkillIndex = INDEX_NONE;
	}
}

void ABattlePlayerController::ToggleMoveMode()
{
	if (InteractionMode == EPlayerInteractionMode::Moving)
	{
		SetInteractionMode(EPlayerInteractionMode::Idle);
	}
	else
	{
		SetInteractionMode(EPlayerInteractionMode::Moving);
	}
	UE_LOG(LogERBattle, Log, TEXT("[BattleInput] ToggleMoveMode -> InteractionMode=%d (0=Idle 1=Moving 2=Targeting)"), (int32)InteractionMode);
}

void ABattlePlayerController::StartSkillTargeting(int32 SkillIndex)
{
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->GetSkillSpecs().IsValidIndex(SkillIndex))
	{
		return;
	}

	// 仅自身目标的技能 (自身增益 / 自身为圆心的周身 AoE) 一键即放, 不再要求点选自己
	const FBattleSkillSpec& Spec = Active->GetSkillSpecs()[SkillIndex];
	const UBattleSkillData* Skill = Spec.SkillData;
	if (Skill && Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
	{
		Mgr->RequestSkill(Active, SkillIndex, Active);
		return;
	}

	SelectedSkillIndex = SkillIndex;
	SetInteractionMode(EPlayerInteractionMode::TargetingSkill);
}

void ABattlePlayerController::CancelCurrentAction()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	SetInteractionMode(EPlayerInteractionMode::Idle);
}

void ABattlePlayerController::RequestEndTurn()
{
	if (UBattleManager* Mgr = GetBattleManager())
	{
		if (ABattleUnit* Active = Mgr->GetActiveUnit())
		{
			if (Active->Team == EBattleTeam::Player)
			{
				SetInteractionMode(EPlayerInteractionMode::Idle);
				Mgr->EndCurrentTurn();
			}
		}
	}
}

void ABattlePlayerController::FocusOnActiveUnit()
{
	if (UBattleManager* Mgr = GetBattleManager())
	{
		if (ABattleUnit* Active = Mgr->GetActiveUnit())
		{
			ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn());

			// 已经在跟随当前单位 -> 再按一次 F 收回全景 (与点击名牌的开关行为一致)
			if (Cam && Cam->IsFollowingUnit() && Cam->GetFocusedUnit() == Active)
			{
				FocusedUnit = nullptr;
				Cam->ClearFocus();
				return;
			}

			// 记入聚焦单位, 让编队面板同步高亮当前回合的我方单位
			FocusedUnit = Active;

			if (Cam)
			{
				// 用特写锁定而非单纯平移: 拉近臂长 + 抬平俯仰, 让模型占据画面主体
				Cam->FocusOnUnit(Active);
			}
		}
	}
}

void ABattlePlayerController::FocusOnUnit(ABattleUnit* Unit)
{
	// 再次点击已聚焦的单位 -> 取消聚焦, 摄像机回到竞技场全景
	if (FocusedUnit.Get() == Unit)
	{
		FocusedUnit = nullptr;
		if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
		{
			Cam->ClearFocus();
		}
		UE_LOG(LogERBattle, Log, TEXT("[BattleFocus] 解除聚焦, 回到全景锚点 (Unit=%s)"),
			Unit ? *Unit->Stats.DisplayName : TEXT("none"));
		return;
	}

	FocusedUnit = Unit;

	if (!Unit)
	{
		if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
		{
			Cam->ClearFocus();
		}
		return;
	}

	UE_LOG(LogERBattle, Log, TEXT("[BattleFocus] 请求聚焦 -> %s @ (%.0f, %.0f, %.0f)"),
		*Unit->Stats.DisplayName, Unit->GetActorLocation().X, Unit->GetActorLocation().Y, Unit->GetActorLocation().Z);

	if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
	{
		Cam->FocusOnUnit(Unit);
	}
}

void ABattlePlayerController::OnLeftClick()
{
	float MX, MY;
	if (GetMousePosition(MX, MY))
	{
		if (ABattleHUD* HUD = Cast<ABattleHUD>(GetHUD()))
		{
			const bool bHandled = HUD->HandleMouseClick(FVector2D(MX, MY));
			const bool bOverUI = HUD->IsMouseOverUI();
			if (bHandled || bOverUI)
			{
				UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 左键被 UI 吞掉: 鼠标=(%.0f,%.0f) handled=%d overUI=%d"), MX, MY, bHandled, bOverUI);
				return;
			}
		}
	}

	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 左键忽略: 战斗状态=%d (需 TurnActive=2)"), Mgr ? (int32)Mgr->GetBattleState() : -1);
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->IsAlive())
	{
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 左键忽略: 无有效玩家活动单位 Active=%s"), *GetNameSafe(Active));
		return;
	}

	FHitResult Hit;
	if (RaycastUnderCursor(Hit, ECC_Visibility))
	{
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 左键场景命中: 模式=%d 命中=%s 位置=(%.0f,%.0f,%.0f)"),
			(int32)InteractionMode, *GetNameSafe(Hit.GetActor()), Hit.Location.X, Hit.Location.Y, Hit.Location.Z);
		if (InteractionMode == EPlayerInteractionMode::Moving)
		{
			const bool bMoved = Mgr->RequestMove(Active, Hit.Location);
			UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 移动请求 -> %d"), bMoved);
			if (bMoved)
			{
				SetInteractionMode(EPlayerInteractionMode::Idle);
			}
		}
		else if (InteractionMode == EPlayerInteractionMode::TargetingSkill)
		{
			if (ABattleUnit* TargetUnit = Cast<ABattleUnit>(Hit.GetActor()))
			{
				if (Mgr->RequestSkill(Active, SelectedSkillIndex, TargetUnit))
				{
					SetInteractionMode(EPlayerInteractionMode::Idle);
				}
			}
		}
		else
		{
			if (ABattleUnit* TargetUnit = Cast<ABattleUnit>(Hit.GetActor()))
			{
				if (TargetUnit->Team == EBattleTeam::Enemy && TargetUnit->IsAlive())
				{
					// 点击敌人快捷发起技能 0 (近战/攻击)
					StartSkillTargeting(0);
					if (Mgr->RequestSkill(Active, SelectedSkillIndex, TargetUnit))
					{
						SetInteractionMode(EPlayerInteractionMode::Idle);
					}
					return;
				}
			}

			// 默认 Idle 状态下: 点击地面直接发起移动
			if (Active->AP >= 0.5f)
			{
				const bool bMoved = Mgr->RequestMove(Active, Hit.Location);
				UE_LOG(LogERBattle, Log, TEXT("[BattleInput] Idle 地面直移 -> %d (AP=%d)"), bMoved, Active->AP);
			}
			else
			{
				UE_LOG(LogERBattle, Log, TEXT("[BattleInput] Idle 地面点击忽略: AP=%d 不足"), Active->AP);
			}
		}
	}
	else
	{
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] 左键射线未命中任何物体 (ECC_Visibility)"));
	}
}

void ABattlePlayerController::OnRightClick()
{
	CancelCurrentAction();
}

void ABattlePlayerController::OnKey1()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	StartSkillTargeting(0);
}

void ABattlePlayerController::OnKey2()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	StartSkillTargeting(1);
}

void ABattlePlayerController::OnKeyM()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	ToggleMoveMode();
}

void ABattlePlayerController::OnKeySpace()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	RequestEndTurn();
}

void ABattlePlayerController::OnKeyF()
{
#if ER_WITH_AI_COMMAND
	if (UERAICommandPanel::IsAnyTextInputFocused()) return;
#endif
	FocusOnActiveUnit();
}

void ABattlePlayerController::BattleCheatWin()
{
	if (UWorld* World = GetWorld())
	{
		if (ABattleGameMode* GM = Cast<ABattleGameMode>(World->GetAuthGameMode()))
		{
			UE_LOG(LogERBattle, Log, TEXT("[Battle] 控制台命令 BattleCheatWin: 秒杀全部敌人"));
			GM->DebugKillAllEnemies();
		}
	}
}

void ABattlePlayerController::BattleResultReturn()
{
	if (UWorld* World = GetWorld())
	{
		if (ABattleGameMode* GM = Cast<ABattleGameMode>(World->GetAuthGameMode()))
		{
			UE_LOG(LogERBattle, Log, TEXT("[Battle] 控制台命令 BattleResultReturn: 结算返回"));
			GM->ReturnFromBattle();
		}
	}
}

void ABattlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 摄像机 Pawn 附体后才补做一次回合自动聚焦
	if (PendingAutoFocusUnit.Get() != nullptr)
	{
		ApplyAutoFocus();
	}

	// 摄像机被手动平移接管后会自行解除单位锁定, 这里同步清除控制器的聚焦记录,
	// 避免编队面板还高亮着一个已经不看了的单位
	if (FocusedUnit.Get() != nullptr)
	{
		if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
		{
			if (Cam->GetFocusedUnit() != FocusedUnit.Get())
			{
				FocusedUnit = nullptr;
			}
		}
	}

	// 调试自动战斗: 我方回合超时自动结束, 让 AI 继续跑
	if (CVarAutoPlay.GetValueOnGameThread() > 0)
	{
		if (UBattleManager* Mgr = GetBattleManager())
		{
			ABattleUnit* Active = (Mgr->GetBattleState() == EBattleState::TurnActive) ? Mgr->GetActiveUnit() : nullptr;
			if (Active && Active->Team == EBattleTeam::Player)
			{
				AutoPlayElapsed += DeltaTime;
				if (AutoPlayElapsed >= 1.2f)
				{
					AutoPlayElapsed = 0.0f;
					RequestEndTurn();
				}
			}
			else
			{
				AutoPlayElapsed = 0.0f;
			}
		}
	}

	UpdateHoveredUnit();

	if (InteractionMode == EPlayerInteractionMode::Moving)
	{
		DrawMovePreview();
	}
	else if (InteractionMode == EPlayerInteractionMode::TargetingSkill)
	{
		DrawSkillPreview();
	}
}

void ABattlePlayerController::UpdateHoveredUnit()
{
	FHitResult Hit;
	ABattleUnit* NewHovered = nullptr;
	if (RaycastUnderCursor(Hit, ECC_Visibility))
	{
		NewHovered = Cast<ABattleUnit>(Hit.GetActor());
	}

	if (NewHovered != CurrentHoveredUnit)
	{
		if (CurrentHoveredUnit)
		{
			CurrentHoveredUnit->SetHovered(false);
		}
		CurrentHoveredUnit = NewHovered;
		if (CurrentHoveredUnit)
		{
			CurrentHoveredUnit->SetHovered(true);
		}
	}
}

void ABattlePlayerController::DrawMovePreview()
{
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr) return;
	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->IsAlive()) return;

	FHitResult Hit;
	if (RaycastUnderCursor(Hit, ECC_Visibility))
	{
		TArray<FVector> Path;
		float CostAP = 0.0f;
		bool bTruncated = false;

		if (Active->PlanMove(Hit.Location, Path, CostAP, bTruncated) && Path.Num() >= 2)
		{
			UWorld* World = GetWorld();
			for (int32 i = 0; i < Path.Num() - 1; ++i)
			{
				const FVector P1 = Path[i] + FVector(0, 0, 10.0f);
				const FVector P2 = Path[i + 1] + FVector(0, 0, 10.0f);
				DrawDebugLine(World, P1, P2, FColor::Green, false, -1.0f, 0, 4.0f);
			}

			const FVector Dest = Path.Last() + FVector(0, 0, 15.0f);
			DrawDebugSphere(World, Dest, 35.0f, 12, bTruncated ? FColor::Red : FColor::Green, false, -1.0f, 0, 2.0f);
		}
	}
}

void ABattlePlayerController::DrawSkillPreview()
{
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr) return;
	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->IsAlive()) return;

	const TArray<FBattleSkillSpec>& Skills = Active->GetSkillSpecs();
	if (!Skills.IsValidIndex(SelectedSkillIndex)) return;

	const FBattleSkillSpec& Spec = Skills[SelectedSkillIndex];
	const UBattleSkillData* Skill = Spec.SkillData;
	if (!Skill) return;

	UWorld* World = GetWorld();

	// 在脚底绘制射程范围圈 (自身 AoE 画作用半径; 冷却中转为灰色)
	const FVector Center = Active->GetActorLocation() - FVector(0, 0, Active->GetCapsuleHalfHeight() - 5.0f);
	const FColor RingColor = Spec.IsOnCooldown() ? FColor(120, 120, 120) : Skill->ThemeColor.ToFColor(true);
	const float PreviewRadius = (Skill->TargetFilter == ESkillTargetFilter::SelfOnly && Skill->AoE.IsArea())
		? Skill->AoE.Radius : Skill->Range;
	DrawDebugCircle(World, Center, PreviewRadius, 48, RingColor, false, -1.0f, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	// 若鼠标悬停在敌军上: 3D 射程 + 视线/掩体提示 (地形耦合)
	if (CurrentHoveredUnit && CurrentHoveredUnit->IsAlive())
	{
		const float Dist = FVector::Dist(Active->GetActorLocation(), CurrentHoveredUnit->GetActorLocation());
		const bool bInRange = (Dist <= Skill->Range);

		bool bHasLOS = true;
		FBattleTerrainContext TerrainCtx;
		if (UBattleTerrainQuery* Terrain = UBattleTerrainQuery::Get(World))
		{
			TerrainCtx = Terrain->Evaluate(Active, CurrentHoveredUnit);
			bHasLOS = TerrainCtx.bHasLineOfSight;
		}

		FColor BeamColor = FColor::Yellow;
		if (!bInRange)
		{
			BeamColor = FColor::Red;
		}
		else if (Skill->bRequiresLineOfSight && !bHasLOS)
		{
			BeamColor = FColor(160, 60, 255); // 紫色: 无视线
		}
		else if (TerrainCtx.Cover == ECoverQuality::Half)
		{
			BeamColor = FColor(255, 170, 40); // 橙色: 目标有掩体
		}

		DrawDebugLine(World, Active->GetActorLocation() + FVector(0, 0, 30.0f),
			CurrentHoveredUnit->GetActorLocation() + FVector(0, 0, 30.0f), BeamColor, false, -1.0f, 0, 3.5f);

		// AoE 落点范围圈
		if (Skill->AoE.IsArea())
		{
			DrawDebugSphere(World, CurrentHoveredUnit->GetActorLocation(), Skill->AoE.Radius, 20,
				FColor(255, 120, 40), false, -1.0f, 0, 1.5f);
		}
	}
}
