// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/UI/BattleHUD.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

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
			if (UBattleManager* Mgr = GetBattleManager())
			{
				Mgr->OnTurnStarted.AddDynamic(this, &ABattlePlayerController::HandleTurnStarted);
				FocusOnActiveUnit();
			}
		});
	}
}

void ABattlePlayerController::HandleTurnStarted(ABattleUnit* Unit)
{
	FocusOnActiveUnit();
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
}

void ABattlePlayerController::StartSkillTargeting(int32 SkillIndex)
{
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->GetSkills().IsValidIndex(SkillIndex))
	{
		return;
	}

	SelectedSkillIndex = SkillIndex;
	SetInteractionMode(EPlayerInteractionMode::TargetingSkill);
}

void ABattlePlayerController::CancelCurrentAction()
{
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
			if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
			{
				Cam->FocusOn(Active->GetActorLocation());
			}
		}
	}
}

void ABattlePlayerController::OnLeftClick()
{
	float MX, MY;
	if (GetMousePosition(MX, MY))
	{
		if (ABattleHUD* HUD = Cast<ABattleHUD>(GetHUD()))
		{
			if (HUD->HandleMouseClick(FVector2D(MX, MY)) || HUD->IsMouseOverUI())
			{
				return;
			}
		}
	}

	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || Active->Team != EBattleTeam::Player || !Active->IsAlive())
	{
		return;
	}

	FHitResult Hit;
	if (RaycastUnderCursor(Hit, ECC_Visibility))
	{
		if (InteractionMode == EPlayerInteractionMode::Moving)
		{
			if (Mgr->RequestMove(Active, Hit.Location))
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
				Mgr->RequestMove(Active, Hit.Location);
			}
		}
	}
}

void ABattlePlayerController::OnRightClick()
{
	CancelCurrentAction();
}

void ABattlePlayerController::OnKey1()
{
	StartSkillTargeting(0);
}

void ABattlePlayerController::OnKey2()
{
	StartSkillTargeting(1);
}

void ABattlePlayerController::OnKeyM()
{
	ToggleMoveMode();
}

void ABattlePlayerController::OnKeySpace()
{
	RequestEndTurn();
}

void ABattlePlayerController::OnKeyF()
{
	FocusOnActiveUnit();
}

void ABattlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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

	const TArray<FBattleSkillDef>& Skills = Active->GetSkills();
	if (!Skills.IsValidIndex(SelectedSkillIndex)) return;

	const FBattleSkillDef& Skill = Skills[SelectedSkillIndex];
	UWorld* World = GetWorld();

	// 在脚底绘制射程范围圈
	const FVector Center = Active->GetActorLocation() - FVector(0, 0, Active->GetCapsuleHalfHeight() - 5.0f);
	DrawDebugCircle(World, Center, Skill.Range, 48, Skill.Color.ToFColor(true), false, -1.0f, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	// 若鼠标悬停在敌军上, 绘制连接线并提示是否在射程内
	if (CurrentHoveredUnit && CurrentHoveredUnit->IsAlive() && CurrentHoveredUnit->Team == EBattleTeam::Enemy)
	{
		const float Dist = FVector::Dist2D(Active->GetActorLocation(), CurrentHoveredUnit->GetActorLocation());
		const bool bInRange = (Dist <= Skill.Range);
		const FColor BeamColor = bInRange ? FColor::Yellow : FColor::Red;

		DrawDebugLine(World, Active->GetActorLocation() + FVector(0, 0, 30.0f),
			CurrentHoveredUnit->GetActorLocation() + FVector(0, 0, 30.0f), BeamColor, false, -1.0f, 0, 3.5f);
	}
}
