// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionPlayerController.h"

#include "Battle/Player/TacticalCameraPawn.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ALegionPlayerController::ALegionPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ALegionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void ALegionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) { return; }

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALegionPlayerController::OnLeftPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ALegionPlayerController::OnLeftReleased).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALegionPlayerController::OnRightPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ALegionPlayerController::OnRightReleased).bExecuteWhenPaused = true;

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ALegionPlayerController::OnKey1).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ALegionPlayerController::OnKey2).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ALegionPlayerController::OnKey3).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ALegionPlayerController::OnKey4).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ALegionPlayerController::OnKey5).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ALegionPlayerController::OnKey6).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ALegionPlayerController::OnKey7).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ALegionPlayerController::OnKey8).bExecuteWhenPaused = true;

	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ALegionPlayerController::OnKeyZ).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ALegionPlayerController::OnKeyX).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALegionPlayerController::OnKeyG).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ALegionPlayerController::OnKeyTab).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ALegionPlayerController::OnKeySpace).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ALegionPlayerController::OnKeyRestart).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALegionPlayerController::OnEscape).bExecuteWhenPaused = true;
}

void ALegionPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	PruneInvalidSelections();

	float MX = 0.0f, MY = 0.0f;
	if (GetMousePosition(MX, MY))
	{
		if (bLeftDown)
		{
			LeftDragCurScreen = FVector2D(MX, MY);
			if (!bMarqueeActive && FVector2D::Distance(LeftDragStartScreen, LeftDragCurScreen) > 14.0f)
			{
				bMarqueeActive = true;
			}
		}
		if (bRightDragging)
		{
			FVector GroundHit;
			if (TraceGroundUnderCursor(GroundHit))
			{
				RightDragCurrentWorld = GroundHit;
			}
		}
	}

	FHitResult Hit;
	ALegionUnit* Hover = PickLegion(Hit);
	ALegionUnit* Prev = HoveredLegion.Get();
	if (Hover != Prev)
	{
		if (Prev) { Prev->SetHovered(false); }
		if (Hover) { Hover->SetHovered(true); }
		HoveredLegion = Hover;
	}

	CurrentMouseCursor = (Hover && Hover->GetTeam() == ELegionTeam::Enemy && SelectedLegions.Num() > 0)
		? EMouseCursor::Crosshairs : EMouseCursor::Default;
}

void ALegionPlayerController::PruneInvalidSelections()
{
	for (int32 i = SelectedLegions.Num() - 1; i >= 0; --i)
	{
		ALegionUnit* L = SelectedLegions[i].Get();
		if (!L || L->IsWiped())
		{
			if (L) { L->SetSelected(false); }
			SelectedLegions.RemoveAt(i);
		}
	}
}

ALegionUnit* ALegionPlayerController::GetSelectedLegion() const
{
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get())
		{
			if (!L->IsWiped()) { return L; }
		}
	}
	return nullptr;
}

bool ALegionPlayerController::IsLegionSelected(const ALegionUnit* Legion) const
{
	if (!Legion) { return false; }
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (Ptr.Get() == Legion) { return true; }
	}
	return false;
}

void ALegionPlayerController::ClearSelection()
{
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get()) { L->SetSelected(false); }
	}
	SelectedLegions.Reset();
}

void ALegionPlayerController::SetSingleSelection(ALegionUnit* Legion)
{
	ClearSelection();
	if (Legion && !Legion->IsWiped())
	{
		Legion->SetSelected(true);
		SelectedLegions.Add(Legion);
	}
}

void ALegionPlayerController::ToggleSelection(ALegionUnit* Legion)
{
	if (!Legion || Legion->IsWiped()) { return; }
	for (int32 i = 0; i < SelectedLegions.Num(); ++i)
	{
		if (SelectedLegions[i].Get() == Legion)
		{
			Legion->SetSelected(false);
			SelectedLegions.RemoveAt(i);
			return;
		}
	}
	Legion->SetSelected(true);
	SelectedLegions.Add(Legion);
}

void ALegionPlayerController::SelectLegionByIndex(int32 Index)
{
	OnKeySelect(Index);
}

void ALegionPlayerController::TriggerSelectedSkills()
{
	PruneInvalidSelections();
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get())
		{
			L->TriggerActiveSkill();
		}
	}
}

ALegionUnit* ALegionPlayerController::PickLegion(FHitResult& OutHit) const
{
	if (GetHitResultUnderCursor(ECC_Visibility, false, OutHit))
	{
		return Cast<ALegionUnit>(OutHit.GetActor());
	}
	return nullptr;
}

bool ALegionPlayerController::TraceGroundUnderCursor(FVector& OutWorld) const
{
	FHitResult Hit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	if (GetHitResultUnderCursorForObjects({ UEngineTypes::ConvertToObjectType(ECC_WorldStatic) }, false, Hit))
	{
		OutWorld = Hit.Location;
		return true;
	}
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		OutWorld = Hit.Location;
		return true;
	}
	return false;
}

void ALegionPlayerController::GetMarqueeScreenRect(FVector2D& OutMin, FVector2D& OutMax) const
{
	OutMin.X = FMath::Min(LeftDragStartScreen.X, LeftDragCurScreen.X);
	OutMin.Y = FMath::Min(LeftDragStartScreen.Y, LeftDragCurScreen.Y);
	OutMax.X = FMath::Max(LeftDragStartScreen.X, LeftDragCurScreen.X);
	OutMax.Y = FMath::Max(LeftDragStartScreen.Y, LeftDragCurScreen.Y);
}

void ALegionPlayerController::OnLeftPressed()
{
	float MX = 0.0f, MY = 0.0f;
	if (GetMousePosition(MX, MY))
	{
		// 点击左上角小地图直接跳转镜头
		int32 SX = 0, SY = 0;
		GetViewportSize(SX, SY);
		if (SY > 0)
		{
			float MapX = 0.0f, MapY = 0.0f, MapSize = 0.0f;
			ALegionHUD::GetMinimapScreenRect(SY, MapX, MapY, MapSize);
			if (MX >= MapX && MX <= MapX + MapSize && MY >= MapY && MY <= MapY + MapSize)
			{
				if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
				{
					const FVector2D UV((MX - MapX) / MapSize, (MY - MapY) / MapSize);
					const FVector WorldPt = GM->MinimapUVToWorld(UV);
					if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
					{
						Cam->EnterFreeMode();
						Cam->FocusOn(WorldPt);
					}
					return;
				}
			}
		}

		bLeftDown = true;
		bMarqueeActive = false;
		LeftDragStartScreen = FVector2D(MX, MY);
		LeftDragCurScreen = LeftDragStartScreen;
	}
}

void ALegionPlayerController::OnLeftReleased()
{
	const bool bWasMarquee = bMarqueeActive;
	bLeftDown = false;
	bMarqueeActive = false;

	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

	if (bWasMarquee)
	{
		FVector2D RMin, RMax;
		GetMarqueeScreenRect(RMin, RMax);
		if (!bShift) { ClearSelection(); }

		if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			for (ALegionUnit* L : GM->GetPlayerLegions())
			{
				if (!L || L->IsWiped()) { continue; }
				FVector2D ScreenPos;
				if (ProjectWorldLocationToScreen(L->GetActorLocation(), ScreenPos, true))
				{
					if (ScreenPos.X >= RMin.X && ScreenPos.X <= RMax.X &&
						ScreenPos.Y >= RMin.Y && ScreenPos.Y <= RMax.Y)
					{
						if (!IsLegionSelected(L))
						{
							L->SetSelected(true);
							SelectedLegions.Add(L);
						}
					}
				}
			}
		}
		return;
	}

	FHitResult Hit;
	ALegionUnit* HitLegion = PickLegion(Hit);
	if (HitLegion && HitLegion->GetTeam() == ELegionTeam::Player && !HitLegion->IsWiped())
	{
		if (bShift) { ToggleSelection(HitLegion); }
		else        { SetSingleSelection(HitLegion); }
		return;
	}
	if (HitLegion && HitLegion->GetTeam() == ELegionTeam::Enemy && SelectedLegions.Num() > 0)
	{
		OrderAttack(HitLegion);
	}
}

void ALegionPlayerController::OnRightPressed()
{
	PruneInvalidSelections();
	if (SelectedLegions.Num() == 0) { return; }

	FHitResult Hit;
	ALegionUnit* HitLegion = PickLegion(Hit);
	if (HitLegion && HitLegion->GetTeam() == ELegionTeam::Enemy)
	{
		OrderAttack(HitLegion);
		return;
	}

	FVector GroundPos;
	if (TraceGroundUnderCursor(GroundPos))
	{
		bRightDragging = true;
		RightDragStartWorld = GroundPos;
		RightDragCurrentWorld = GroundPos;

		// 拖线定朝向时自动进入绝境北地式战术时缓
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			if (GM->GetTacticalTimeMode() == ELegionTimeMode::Normal)
			{
				GM->SetTacticalTimeMode(ELegionTimeMode::SlowMo);
				bAutoSlowMoByDrag = true;
			}
		}
	}
}

void ALegionPlayerController::OnRightReleased()
{
	if (!bRightDragging) { return; }
	bRightDragging = false;

	if (bAutoSlowMoByDrag)
	{
		bAutoSlowMoByDrag = false;
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			if (GM->GetTacticalTimeMode() == ELegionTimeMode::SlowMo)
			{
				GM->SetTacticalTimeMode(ELegionTimeMode::Normal);
			}
		}
	}

	PruneInvalidSelections();
	if (SelectedLegions.Num() == 0) { return; }

	const FVector DragDelta = RightDragCurrentWorld - RightDragStartWorld;
	if (DragDelta.Size2D() > 350.0f)
	{
		OrderFormationMove(RightDragStartWorld, DragDelta.GetSafeNormal2D(), true);
	}
	else
	{
		FVector AvgPos = FVector::ZeroVector;
		int32 Count = 0;
		for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
		{
			if (ALegionUnit* L = Ptr.Get()) { AvgPos += L->GetActorLocation(); ++Count; }
		}
		if (Count > 0) { AvgPos /= (float)Count; }
		const FVector ApproachDir = (RightDragStartWorld - AvgPos).GetSafeNormal2D();
		OrderFormationMove(RightDragStartWorld, ApproachDir.IsNearlyZero() ? FVector::ForwardVector : ApproachDir, false);
	}
}

void ALegionPlayerController::ComputeFormationSlots(const FVector& Center, const FVector& FacingDir, TArray<FVector>& OutSlots) const
{
	OutSlots.Reset();
	TArray<ALegionUnit*> Valid;
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get())
		{
			if (!L->IsWiped()) { Valid.Add(L); }
		}
	}
	const int32 N = Valid.Num();
	if (N == 0) { return; }

	const FVector Fwd = FacingDir.IsNearlyZero() ? FVector::ForwardVector : FacingDir.GetSafeNormal2D();
	const FVector Right(-Fwd.Y, Fwd.X, 0.0f);
	const float SlotSpacing = 1250.0f;

	float MinX = -MAX_FLT, MaxX = MAX_FLT, MinY = -MAX_FLT, MaxY = MAX_FLT;
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		const FVector C = GM->GetDeployCenter();
		const float E = GM->GetBattlefieldHalfExtent();
		MinX = C.X - E; MaxX = C.X + E;
		MinY = C.Y - E; MaxY = C.Y + E;
	}

	const float StartOffset = -0.5f * (N - 1) * SlotSpacing;
	for (int32 i = 0; i < N; ++i)
	{
		FVector Slot = Center + Right * (StartOffset + i * SlotSpacing);
		Slot.X = FMath::Clamp(Slot.X, MinX, MaxX);
		Slot.Y = FMath::Clamp(Slot.Y, MinY, MaxY);
		OutSlots.Add(Slot);
	}
}

void ALegionPlayerController::OrderFormationMove(const FVector& Center, const FVector& FacingDir, bool bUseCustomFacing)
{
	TArray<FVector> Slots;
	ComputeFormationSlots(Center, FacingDir, Slots);
	const float Yaw = FacingDir.Rotation().Yaw;

	int32 SlotIdx = 0;
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		ALegionUnit* L = Ptr.Get();
		if (!L || L->IsWiped()) { continue; }
		const FVector Dest = Slots.IsValidIndex(SlotIdx) ? Slots[SlotIdx] : Center;
		L->CommandMoveTo(Dest, bUseCustomFacing ? Yaw : 1000.0f);
		++SlotIdx;
	}
}

void ALegionPlayerController::OrderAttack(ALegionUnit* Target)
{
	if (!Target || Target->IsWiped()) { return; }
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* Sel = Ptr.Get())
		{
			if (!Sel->IsWiped() && Sel != Target && Target->GetTeam() != Sel->GetTeam())
			{
				Sel->CommandAttack(Target);
			}
		}
	}
}

void ALegionPlayerController::OnKeySelect(int32 Index)
{
	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM) { return; }
	const TArray<TObjectPtr<ALegionUnit>>& Ls = GM->GetPlayerLegions();
	if (Ls.IsValidIndex(Index) && Ls[Index] && !Ls[Index]->IsWiped())
	{
		SetSingleSelection(Ls[Index]);
		if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(GetPawn()))
		{
			Cam->FocusOn(Ls[Index]->GetActorLocation());
		}
	}
}

void ALegionPlayerController::OnKeyX()
{
	PruneInvalidSelections();
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get()) { L->CommandHalt(); }
	}
}

void ALegionPlayerController::OnKeyG()
{
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->AllOutAssault(ELegionTeam::Player);
	}
}

void ALegionPlayerController::OnKeyTab()
{
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld()->GetAuthGameMode()))
	{
		const ELegionTimeMode Cur = GM->GetTacticalTimeMode();
		GM->SetTacticalTimeMode(Cur == ELegionTimeMode::SlowMo ? ELegionTimeMode::Normal : ELegionTimeMode::SlowMo);
	}
}

void ALegionPlayerController::OnKeySpace()
{
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld()->GetAuthGameMode()))
	{
		const ELegionTimeMode Cur = GM->GetTacticalTimeMode();
		GM->SetTacticalTimeMode(Cur == ELegionTimeMode::Paused ? ELegionTimeMode::Normal : ELegionTimeMode::Paused);
	}
}

void ALegionPlayerController::OnEscape()
{
	ClearSelection();
}

void ALegionPlayerController::OnKeyRestart()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}
