// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/ERLog.h"
#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "CollisionQueryParams.h"
#include "Containers/Ticker.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	ALegionGameMode* GetLegionGM(const UWorld* World)
	{
		return World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	}
}

ALegionPlayerController::ALegionPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
	// 等高线地图视角下战场暂停, 但平移/悬停仍需逐帧更新
	bShouldPerformFullTickWhenPaused = true;

	// FP 模板输入映射 (指挥官化身的移动/视角/开火由 Enhanced Input 驱动)
	DefaultMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Input/IMC_Default.IMC_Default")));
	MouseLookMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")));
	WeaponsMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Variant_Shooter/Input/IMC_Weapons.IMC_Weapons")));
}

void ALegionPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -82.0f;
		PlayerCameraManager->ViewPitchMax = 82.0f;
	}
	if (ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD))
	{
		HUD->NotifyStrategicMapOpen(bMapOpen);
	}
	ApplyInputMode();
}

void ALegionPlayerController::SetDeploymentInputLocked(bool bLocked)
{
	bDeploymentInputLocked = bLocked;
	ApplyInputMode();
}

void ALegionPlayerController::ApplyInputMode()
{
	// 接驳加载屏驻留期间: 一切战场输入让给 UI, 光标常驻
	if (bDeploymentInputLocked)
	{
		bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		return;
	}

	const bool bCursor = bMapOpen || bAltCursor;
	bShowMouseCursor = bCursor;
	if (bCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ALegionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) { return; }

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALegionPlayerController::OnLeftPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ALegionPlayerController::OnLeftReleased).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALegionPlayerController::OnRightPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ALegionPlayerController::OnRightReleased).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ALegionPlayerController::OnMidPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &ALegionPlayerController::OnMidReleased).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ALegionPlayerController::OnWheelUp).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ALegionPlayerController::OnWheelDown).bExecuteWhenPaused = true;

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ALegionPlayerController::OnKey1).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ALegionPlayerController::OnKey2).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ALegionPlayerController::OnKey3).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ALegionPlayerController::OnKey4).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ALegionPlayerController::OnKey5).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ALegionPlayerController::OnKey6).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ALegionPlayerController::OnKey7).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ALegionPlayerController::OnKey8).bExecuteWhenPaused = true;

	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ALegionPlayerController::OnKeyX).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALegionPlayerController::OnKeyG).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ALegionPlayerController::OnKeyF).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ALegionPlayerController::OnKeyMapToggle).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ALegionPlayerController::OnKeyRestart).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALegionPlayerController::OnEscape).bExecuteWhenPaused = true;

	// FP 模板 Enhanced Input 映射 (移动/跳跃/视角/开火由指挥官化身自绑)
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (const TSoftObjectPtr<UInputMappingContext>& Ctx :
				{ DefaultMappingContext, MouseLookMappingContext, WeaponsMappingContext })
			{
				if (UInputMappingContext* MC = Ctx.LoadSynchronous())
				{
					Subsystem->AddMappingContext(MC, 0);
				}
				else
				{
					UE_LOG(LogERLegion, Warning, TEXT("[Legion] InputMappingContext failed to load: %s"), *Ctx.ToString());
				}
			}
		}
	}
}

// ---------------- Tick ----------------

void ALegionPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	PruneInvalidSelections();

	ALegionGameMode* GM = GetLegionGM(GetWorld());
	if (bMapOpen && GM && GM->IsBattleOver())
	{
		SetStrategicMapOpen(false);
	}

	// 战场视角: 按住 Alt 临时呼出光标 (点小地图 / 兵团牌 / 按钮)
	const bool bAlt = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
	if (!bMapOpen && bAlt != bAltCursor)
	{
		bAltCursor = bAlt;
		ApplyInputMode();
	}

	if (bMapOpen)
	{
		TickMap();
	}
	else
	{
		TickGround(DeltaTime);
		TickFollow(DeltaTime);
	}
}

void ALegionPlayerController::TickGround(float DeltaTime)
{
	// 右键拖拽阵型朝向: 以准星 (或光标) 指向的地面点作为拖拽终点
	if (bRightDragging)
	{
		FVector G;
		if (TraceGroundAim(G)) { RightDragCurrentWorld = G; }
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

void ALegionPlayerController::TickMap()
{
	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	if (SX <= 0 || SY <= 0) { return; }

	float MX = 0.0f, MY = 0.0f;
	if (!GetMousePosition(MX, MY)) { return; }
	const FVector2D M(MX, MY);

	if (UERAICommandPanel::IsAnyPanelHoveredOrFocused())
	{
		bLeftDown = false;
		bMapDragged = false;
		bMidDown = false;
		CurrentMouseCursor = EMouseCursor::Default;
		return;
	}

	if (bLeftDown && !bMapDragged && FVector2D::Distance(M, LeftDragStartScreen) > 8.0f)
	{
		bMapDragged = true;
		MapLastMouse = M;
	}
	if ((bLeftDown && bMapDragged) || bMidDown)
	{
		MapView.CenterUV -= (M - MapLastMouse) / (double)FMath::Max(1.0f, MapView.Side(SX, SY));
	}
	MapLastMouse = M;
	MapView.Clamp(SX, SY);

	EMouseCursor::Type Cursor = EMouseCursor::Default;
	if (ALegionHUD::GetMapReturnButtonRect(SX, SY).IsInside(M))
	{
		Cursor = EMouseCursor::Hand;
	}
	else if (ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD))
	{
		ALegionUnit* B = HUD->PickStrategicBadge(M);
		if (B && B->GetTeam() == ELegionTeam::Enemy && SelectedLegions.Num() > 0) { Cursor = EMouseCursor::Crosshairs; }
		else if (B) { Cursor = EMouseCursor::Hand; }
		else if (bLeftDown && bMapDragged) { Cursor = EMouseCursor::GrabHandClosed; }
	}
	CurrentMouseCursor = Cursor;
}

void ALegionPlayerController::TickFollow(float DeltaTime)
{
	if (FollowLegions.Num() == 0) { return; }
	FollowTimer -= DeltaTime;
	if (FollowTimer > 0.0f) { return; }
	FollowTimer = 0.7f;

	APawn* P = GetPawn();
	if (!P) { return; }

	TArray<ALegionUnit*> Followers;
	for (int32 i = FollowLegions.Num() - 1; i >= 0; --i)
	{
		ALegionUnit* L = FollowLegions[i].Get();
		if (!L || L->IsWiped()) { FollowLegions.RemoveAt(i); continue; }
		Followers.Insert(L, 0);
	}
	if (Followers.Num() == 0) { return; }

	const float Yaw = GetControlRotation().Yaw;
	const FVector Fwd = FRotator(0.0f, Yaw, 0.0f).Vector();
	const FVector Center = P->GetActorLocation() - Fwd * FollowDistance;

	TArray<FVector> Slots;
	ComputeSlotsFor(Followers, Center, Fwd, Slots);
	for (int32 i = 0; i < Followers.Num(); ++i)
	{
		ALegionUnit* L = Followers[i];
		// 接敌中的兵团先打完, 不被跟随指令拽走
		if (L->GetAttackTarget() || L->GetLegionState() == ELegionState::Engaged) { continue; }
		if (Slots.IsValidIndex(i) && FVector::Dist2D(L->GetActorLocation(), Slots[i]) > 380.0f)
		{
			L->CommandMoveTo(Slots[i], Yaw);
		}
	}
}

// ---------------- 选择 ----------------

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

bool ALegionPlayerController::IsLegionFollowing(const ALegionUnit* Legion) const
{
	if (!Legion) { return false; }
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : FollowLegions)
	{
		if (Ptr.Get() == Legion) { return true; }
	}
	return false;
}

void ALegionPlayerController::StopFollow(const ALegionUnit* Legion)
{
	FollowLegions.RemoveAll([Legion](const TWeakObjectPtr<ALegionUnit>& P) { return P.Get() == Legion || !P.IsValid(); });
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

void ALegionPlayerController::SetSelectedLegions(const TArray<ALegionUnit*>& Legions)
{
	ClearSelection();
	for (ALegionUnit* L : Legions)
	{
		if (L && !L->IsWiped() && L->GetTeam() == ELegionTeam::Player && !IsLegionSelected(L))
		{
			L->SetSelected(true);
			SelectedLegions.Add(L);
		}
	}
}

void ALegionPlayerController::SetLegionsFollowing(const TArray<ALegionUnit*>& Legions, bool bFollow)
{
	for (ALegionUnit* L : Legions)
	{
		if (!L || L->IsWiped() || L->GetTeam() != ELegionTeam::Player) { continue; }
		if (bFollow)
		{
			if (!IsLegionFollowing(L)) { FollowLegions.Add(L); }
		}
		else
		{
			StopFollow(L);
		}
	}
	if (bFollow) { FollowTimer = 0.0f; }
}

void ALegionPlayerController::ComputeSlotsForLegions(const TArray<ALegionUnit*>& Legions, const FVector& Center,
	const FVector& FacingDir, TArray<FVector>& OutSlots) const
{
	ComputeSlotsFor(Legions, Center, FacingDir, OutSlots);
}

// ---------------- 拾取 ----------------

ALegionUnit* ALegionPlayerController::PickLegion(FHitResult& OutHit) const
{
	// 1. 优先命中天空中的敌军部队 Logo (抬头瞄天才能锁定敌军集火, 彻底避免地面移动误触)
	if (const ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD))
	{
		FVector2D AimScreen = FVector2D::ZeroVector;
		bool bHasAimScreen = false;
		if (bShowMouseCursor)
		{
			float MX = 0.0f, MY = 0.0f;
			if (GetMousePosition(MX, MY))
			{
				AimScreen = FVector2D(MX, MY);
				bHasAimScreen = true;
			}
		}
		else
		{
			int32 SX = 0, SY = 0;
			GetViewportSize(SX, SY);
			if (SX > 0 && SY > 0)
			{
				AimScreen = FVector2D(SX * 0.5, SY * 0.5);
				bHasAimScreen = true;
			}
		}

		if (bHasAimScreen)
		{
			if (ALegionUnit* SkyEnemy = HUD->PickEnemySkyBanner(AimScreen))
			{
				return SkyEnemy;
			}
		}
	}

	// 2. 地面射线只拾取我方兵团 (敌军地面 PickBox 已忽略 Visibility, 这里再做一次防误触兜底)
	ALegionUnit* GroundUnit = nullptr;
	if (bShowMouseCursor)
	{
		if (GetHitResultUnderCursor(ECC_Visibility, false, OutHit))
		{
			GroundUnit = Cast<ALegionUnit>(OutHit.GetActor());
		}
	}
	else
	{
		FVector Loc;
		FRotator Rot;
		GetPlayerViewPoint(Loc, Rot);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LegionCrosshairPick), false);
		Q.AddIgnoredActor(GetPawn());
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(OutHit, Loc, Loc + Rot.Vector() * 80000.0f, ECC_Visibility, Q))
		{
			GroundUnit = Cast<ALegionUnit>(OutHit.GetActor());
		}
	}

	if (GroundUnit && GroundUnit->GetTeam() == ELegionTeam::Enemy)
	{
		return nullptr;
	}
	return GroundUnit;
}

bool ALegionPlayerController::TraceGroundAim(FVector& OutWorld) const
{
	FHitResult Hit;
	if (bShowMouseCursor)
	{
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

	FVector Loc;
	FRotator Rot;
	GetPlayerViewPoint(Loc, Rot);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LegionGroundAim), false);
	Q.AddIgnoredActor(GetPawn());
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, Loc, Loc + Rot.Vector() * 90000.0f, ObjParams, Q))
	{
		OutWorld = Hit.Location;
		return true;
	}
	return false;
}

// ---------------- 鼠标 ----------------

void ALegionPlayerController::OnLeftPressed()
{
	float MX = 0.0f, MY = 0.0f;
	const bool bHasMouse = GetMousePosition(MX, MY);

	if (bMapOpen)
	{
		if (UERAICommandPanel::IsAnyPanelHoveredOrFocused()) { return; }
		if (!bHasMouse) { return; }
		bLeftDown = true;
		bMapDragged = false;
		LeftDragStartScreen = FVector2D(MX, MY);
		MapLastMouse = LeftDragStartScreen;
		return;
	}

	// 光标呼出时点击左上角小地图 → 进入等高线地图视角
	if (bShowMouseCursor && bHasMouse)
	{
		int32 SX = 0, SY = 0;
		GetViewportSize(SX, SY);
		if (SY > 0)
		{
			float MapX = 0.0f, MapY = 0.0f, MapSize = 0.0f;
			ALegionHUD::GetMinimapScreenRect(SY, MapX, MapY, MapSize);
			if (MX >= MapX && MX <= MapX + MapSize && MY >= MapY && MY <= MapY + MapSize)
			{
				SetStrategicMapOpen(true);
				return;
			}
		}
	}

	// 战场视角的左键 = 纯开火 (IA_Shoot), 指挥全部由右键承担
}

void ALegionPlayerController::OnLeftReleased()
{
	if (!bMapOpen)
	{
		bLeftDown = false;
		return;
	}

	const bool bWasClick = bLeftDown && !bMapDragged;
	bLeftDown = false;
	bMapDragged = false;
	if (bWasClick)
	{
		float MX = 0.0f, MY = 0.0f;
		if (GetMousePosition(MX, MY)) { HandleMapClick(FVector2D(MX, MY), false); }
	}
}

void ALegionPlayerController::OnMidPressed()
{
	if (!bMapOpen || UERAICommandPanel::IsAnyPanelHoveredOrFocused()) { return; }
	bMidDown = true;
	float MX = 0.0f, MY = 0.0f;
	if (GetMousePosition(MX, MY)) { MapLastMouse = FVector2D(MX, MY); }
}

void ALegionPlayerController::OnRightPressed()
{
	if (bMapOpen)
	{
		if (UERAICommandPanel::IsAnyPanelHoveredOrFocused()) { return; }
		float MX = 0.0f, MY = 0.0f;
		if (GetMousePosition(MX, MY)) { HandleMapClick(FVector2D(MX, MY), true); }
		return;
	}

	PruneInvalidSelections();

	FHitResult Hit;
	ALegionUnit* HitLegion = PickLegion(Hit);
	if (HitLegion && !HitLegion->IsWiped())
	{
		if (HitLegion->GetTeam() == ELegionTeam::Player)
		{
			// 点我方兵团: 选中/取消选中 (Ctrl 为累选/减选)
			const bool bMulti = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
			if (bMulti) { ToggleSelection(HitLegion); }
			else if (SelectedLegions.Num() == 1 && SelectedLegions[0].Get() == HitLegion) { ClearSelection(); }
			else { SetSingleSelection(HitLegion); }
			return;
		}
		if (SelectedLegions.Num() > 0)
		{
			// 点敌方兵团/番号: 选中兵团集火
			OrderAttack(HitLegion);
			return;
		}
	}

	if (SelectedLegions.Num() == 0) { return; }

	FVector GroundPos;
	if (TraceGroundAim(GroundPos))
	{
		bRightDragging = true;
		RightDragStartWorld = GroundPos;
		RightDragCurrentWorld = GroundPos;
	}
}

void ALegionPlayerController::OnRightReleased()
{
	if (!bRightDragging) { return; }
	bRightDragging = false;

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

// ---------------- 等高线地图视角 ----------------

void ALegionPlayerController::ResetMapView()
{
	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	if (SX <= 0 || SY <= 0) { return; }

	// 初始铺满整个屏幕 (cover), 并以指挥官所在位置为中心
	MapView.Zoom = (float)FMath::Max(SX, SY) / (float)FMath::Min(SX, SY);
	MapView.CenterUV = FVector2D(0.5, 0.5);
	if (ALegionGameMode* GM = GetLegionGM(GetWorld()))
	{
		FVector2D UV;
		if (GetPawn() && GM->WorldToMinimapUV(GetPawn()->GetActorLocation(), UV)) { MapView.CenterUV = UV; }
	}
	MapView.Clamp(SX, SY);
}

void ALegionPlayerController::SetStrategicMapOpen(bool bOpen)
{
	if (bMapOpen == bOpen) { return; }
	ALegionGameMode* GM = GetLegionGM(GetWorld());

	if (bOpen)
	{
		PrevTimeMode = GM ? GM->GetTacticalTimeMode() : ELegionTimeMode::Normal;
		bRightDragging = false;
		bLeftDown = false;
		bMapDragged = false;
		bMidDown = false;
		bAltCursor = false;
		if (HoveredLegion.IsValid()) { HoveredLegion->SetHovered(false); }
		HoveredLegion = nullptr;
		bMapOpen = true;
		ResetMapView();
		if (GM) { GM->SetTacticalTimeMode(ELegionTimeMode::Paused); }
	}
	else
	{
		bMapOpen = false;
		bLeftDown = false;
		bMapDragged = false;
		bMidDown = false;
		if (GM && !GM->IsBattleOver()) { GM->SetTacticalTimeMode(PrevTimeMode); }
	}
	if (ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD))
	{
		HUD->NotifyStrategicMapOpen(bMapOpen);
	}
	ApplyInputMode();
}

void ALegionPlayerController::OnMapWheel(int32 Dir)
{
	if (!bMapOpen || UERAICommandPanel::IsAnyPanelHoveredOrFocused()) { return; }
	float MX = 0.0f, MY = 0.0f;
	if (GetMousePosition(MX, MY)) { ZoomMapAt(Dir, FVector2D(MX, MY)); }
}

void ALegionPlayerController::ZoomMapAt(int32 Dir, const FVector2D& M)
{
	if (!bMapOpen) { return; }
	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	if (SX <= 0 || SY <= 0) { return; }

	const FVector2D UVUnder = MapView.ScreenToUV(M, SX, SY);
	const float Factor = Dir > 0 ? MapZoomStep : 1.0f / MapZoomStep;
	MapView.Zoom = FMath::Clamp(MapView.Zoom * Factor, FLegionMapView::MinZoom, FLegionMapView::MaxZoom);
	// 保持鼠标下的地图点不动
	MapView.CenterUV = UVUnder - (M - FVector2D(SX * 0.5, SY * 0.5)) / (double)MapView.Side(SX, SY);
	MapView.Clamp(SX, SY);
}

void ALegionPlayerController::HandleMapClick(const FVector2D& ScreenPos, bool bRight)
{
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD);
	if (!GM || !HUD) { SetStrategicMapOpen(false); return; }

	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);

	// 专用「返回战场」按钮
	if (!bRight && ALegionHUD::GetMapReturnButtonRect(SX, SY).IsInside(ScreenPos))
	{
		SetStrategicMapOpen(false);
		return;
	}

	const bool bMulti = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)
		|| IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);

	// 番号牌: 我方 = 选中, 敌方 = 集火
	if (ALegionUnit* Hit = HUD->PickStrategicBadge(ScreenPos))
	{
		if (Hit->IsWiped()) { return; }
		if (Hit->GetTeam() == ELegionTeam::Player)
		{
			if (!bRight)
			{
				if (bMulti) { ToggleSelection(Hit); }
				else        { SetSingleSelection(Hit); }
			}
		}
		else
		{
			PruneInvalidSelections();
			OrderAttack(Hit);
		}
		return;
	}

	// 空地: 有选中则机动
	const FVector2D UV = MapView.ScreenToUV(ScreenPos, SX, SY);
	if (UV.X < 0.0 || UV.X > 1.0 || UV.Y < 0.0 || UV.Y > 1.0) { return; }

	PruneInvalidSelections();
	if (SelectedLegions.Num() == 0) { return; }

	const FVector WorldPt = GM->MinimapUVToWorld(UV);
	FVector AvgPos = FVector::ZeroVector;
	int32 Count = 0;
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get()) { AvgPos += L->GetActorLocation(); ++Count; }
	}
	if (Count > 0) { AvgPos /= (float)Count; }
	const FVector ApproachDir = (WorldPt - AvgPos).GetSafeNormal2D();
	OrderFormationMove(WorldPt, ApproachDir.IsNearlyZero() ? FVector::ForwardVector : ApproachDir, false);
}

void ALegionPlayerController::ERMapSelfTest()
{
	RunMapSelfTest(true);
}

void ALegionPlayerController::ERSkyBannerSelfTest()
{
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	if (!GM || !GetPawn()) { return; }
	if (bMapOpen) { SetStrategicMapOpen(false); }

	ALegionUnit* P0 = nullptr;
	ALegionUnit* E0 = nullptr;
	for (ALegionUnit* L : GM->GetPlayerLegions()) { if (L && !L->IsWiped()) { P0 = L; break; } }
	for (ALegionUnit* L : GM->GetEnemyLegions())  { if (L && !L->IsWiped()) { E0 = L; break; } }
	if (!P0 || !E0)
	{
		UE_LOG(LogERLegion, Warning, TEXT("[SkyBannerSelfTest] FAIL: missing player or enemy legion"));
		return;
	}

	SetSingleSelection(P0);
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);

	// 1. 瞄准敌军地面身体: PickLegion 必须返回 nullptr (不会误触)
	const FRotator GroundAimRot = (E0->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f) - CamLoc).Rotation();
	SetControlRotation(GroundAimRot);

	TWeakObjectPtr<ALegionPlayerController> WeakPC(this);
	TWeakObjectPtr<ALegionUnit> WeakP0(P0);
	TWeakObjectPtr<ALegionUnit> WeakE0(E0);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, WeakP0, WeakE0](float)
	{
		ALegionPlayerController* PC = WeakPC.Get();
		ALegionUnit* P = WeakP0.Get();
		ALegionUnit* E = WeakE0.Get();
		if (!PC || !P || !E) { return false; }

		FHitResult Hit;
		ALegionUnit* GroundPick = PC->PickLegion(Hit);
		const bool bGroundSafe = (GroundPick != E);
		UE_LOG(LogERLegion, Log, TEXT("[SkyBannerSelfTest] %s: ground aim does not mis-pick enemy (picked=%s)"),
			bGroundSafe ? TEXT("PASS") : TEXT("FAIL"), *GetNameSafe(GroundPick));

		// 2. 抬头瞄准敌军天空 Logo: 等待下一帧 HUD 绘制后校验命中并模拟右键集火
		FVector Loc;
		FRotator Rot;
		PC->GetPlayerViewPoint(Loc, Rot);
		PC->SetControlRotation((E->GetSkyBannerWorldLocation() - Loc).Rotation());

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, WeakP0, WeakE0](float)
		{
			ALegionPlayerController* PC2 = WeakPC.Get();
			ALegionUnit* P2 = WeakP0.Get();
			ALegionUnit* E2 = WeakE0.Get();
			if (!PC2 || !P2 || !E2) { return false; }

			FHitResult SkyHit;
			ALegionUnit* SkyPick = PC2->PickLegion(SkyHit);
			const bool bSkyPicked = (SkyPick == E2);
			UE_LOG(LogERLegion, Log, TEXT("[SkyBannerSelfTest] %s: look-up sky banner picks enemy (picked=%s)"),
				bSkyPicked ? TEXT("PASS") : TEXT("FAIL"), *GetNameSafe(SkyPick));

			PC2->OnRightPressed();
			PC2->OnRightReleased();
			const bool bOrdered = (P2->GetAttackTarget() == E2);
			UE_LOG(LogERLegion, Log, TEXT("[SkyBannerSelfTest] %s: right-click on sky banner orders attack"),
				bOrdered ? TEXT("PASS") : TEXT("FAIL"));
			E2->RunExactHitSelfTest();
			return false;
		}), 0.15f);
		return false;
	}), 0.15f);
}

void ALegionPlayerController::RunMapSelfTest(bool bCloseAtEnd)
{
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	ALegionHUD* HUD = Cast<ALegionHUD>(MyHUD);
	if (!GM || !HUD) { return; }
	if (!bMapOpen)
	{
		SetStrategicMapOpen(true);
		UE_LOG(LogERLegion, Log, TEXT("[MapSelfTest] map was closed -> opened it; run the command again once a frame has been drawn"));
		return;
	}

	int32 Fails = 0;
	auto Check = [&Fails](const char* Name, bool bOk)
	{
		UE_LOG(LogERLegion, Log, TEXT("[MapSelfTest] %s: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name));
		if (!bOk) { ++Fails; }
	};

	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);

	ALegionUnit* P0 = nullptr; ALegionUnit* P1 = nullptr; ALegionUnit* E0 = nullptr;
	for (ALegionUnit* L : GM->GetPlayerLegions())
	{
		if (L && !L->IsWiped()) { if (!P0) { P0 = L; } else if (!P1) { P1 = L; } }
	}
	for (ALegionUnit* L : GM->GetEnemyLegions())
	{
		if (L && !L->IsWiped()) { E0 = L; break; }
	}

	Check("battlefield paused while map open", GM->GetTacticalTimeMode() == ELegionTimeMode::Paused);
	Check("have 2 ally legions + 1 enemy", P0 && P1 && E0);
	if (!(P0 && P1 && E0)) { return; }

	FVector2D PC0, EC0;
	Check("ally badge laid out", HUD->GetStrategicBadgeCenter(P0, PC0));
	Check("enemy badge laid out", HUD->GetStrategicBadgeCenter(E0, EC0));

	ClearSelection();
	HandleMapClick(PC0, false);
	Check("click ally badge selects it", IsLegionSelected(P0) && SelectedLegions.Num() == 1);

	HandleMapClick(EC0, false);
	Check("then click enemy badge orders attack", P0->GetAttackTarget() == E0);

	// 空地机动: 找一个没有番号牌且在地图内的屏幕点
	SetSingleSelection(P1);
	FVector2D Empty = FVector2D::ZeroVector;
	bool bFound = false;
	for (int32 iy = 1; iy <= 6 && !bFound; ++iy)
	{
		for (int32 ix = 1; ix <= 6 && !bFound; ++ix)
		{
			const FVector2D Pt(SX * ix / 7.0, SY * iy / 7.0 + 40.0);
			const FVector2D UV = MapView.ScreenToUV(Pt, SX, SY);
			if (!HUD->PickStrategicBadge(Pt) && UV.X > 0.02 && UV.X < 0.98 && UV.Y > 0.02 && UV.Y < 0.98
				&& !ALegionHUD::GetMapReturnButtonRect(SX, SY).IsInside(Pt))
			{
				Empty = Pt; bFound = true;
			}
		}
	}
	Check("found an empty map point", bFound);
	if (bFound)
	{
		TestMover = P1;
		TestMoverStart = P1->GetActorLocation();
		HandleMapClick(Empty, false);
		Check("click empty ground orders move", P1->GetLegionState() == ELegionState::Moving && !P1->GetAttackTarget());
		const FVector Expected = GM->MinimapUVToWorld(MapView.ScreenToUV(Empty, SX, SY));
		Check("move destination is where the map was clicked", P1->HasMoveOrder() && FVector::Dist2D(P1->GetMoveDestination(), Expected) < 3000.0f);
		TArray<FVector> Path;
		P1->GetRemainingPath(Path);
		Check("move order exposes a path for the UI", Path.Num() > 0);
	}

	// 滚轮缩放: 鼠标下的地图点必须不动
	{
		const FVector2D Anchor(SX * 0.7, SY * 0.6);
		const FVector2D Before = MapView.ScreenToUV(Anchor, SX, SY);
		const float Z0 = MapView.Zoom;
		ZoomMapAt(+1, Anchor);
		const FVector2D After = MapView.ScreenToUV(Anchor, SX, SY);
		Check("wheel zoom-in grows zoom", MapView.Zoom > Z0 + 0.01f);
		Check("wheel zoom keeps point under cursor", FVector2D::Distance(Before, After) < 1e-3);
		for (int32 i = 0; i < 40; ++i) { ZoomMapAt(+1, Anchor); }
		Check("zoom clamps at max", FMath::IsNearlyEqual(MapView.Zoom, FLegionMapView::MaxZoom, 1e-3f));
		for (int32 i = 0; i < 60; ++i) { ZoomMapAt(-1, Anchor); }
		Check("zoom clamps at min", FMath::IsNearlyEqual(MapView.Zoom, FLegionMapView::MinZoom, 1e-3f));
		const FVector2D TL = MapView.UVToScreen(FVector2D(0, 0), SX, SY);
		const float Side = MapView.Side(SX, SY);
		Check("min zoom keeps whole map inside viewport", TL.X >= -1.0 && TL.Y >= -1.0 && TL.X + Side <= SX + 1.0 && TL.Y + Side <= SY + 1.0);
	}

	TestFails = Fails;

	if (bCloseAtEnd)
	{
		FinishMapSelfTest();
		return;
	}

	// 保持地图打开供截图: 放大到中等倍率并对准参战各方, 让命令线 / 终点靶标 / 朝向箭头清晰可见
	{
		FVector Mid = P0->GetActorLocation() + P1->GetActorLocation() + E0->GetActorLocation();
		Mid /= 3.0f;
		FVector2D UV;
		if (GM->WorldToMinimapUV(Mid, UV)) { MapView.CenterUV = UV; }
		MapView.Zoom = 2.4f;
		MapView.Clamp(SX, SY);
	}
}

void ALegionPlayerController::FinishMapSelfTest()
{
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	if (!GM || !bMapOpen) { return; }

	int32 Fails = TestFails;
	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	const FBox2D BR = ALegionHUD::GetMapReturnButtonRect(SX, SY);
	HandleMapClick(BR.GetCenter(), false);
	auto Check = [&Fails](const char* Name, bool bOk)
	{
		UE_LOG(LogERLegion, Log, TEXT("[MapSelfTest] %s: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name));
		if (!bOk) { ++Fails; }
	};
	Check("return button closes map", !bMapOpen);
	Check("time mode restored after leaving map", GM->GetTacticalTimeMode() != ELegionTimeMode::Paused);
	UE_LOG(LogERLegion, Log, TEXT("[MapSelfTest] done: %s (%d failed)"), Fails == 0 ? TEXT("ALL PASS") : TEXT("HAS FAILURES"), Fails);

	// 返回战场后 4 秒: 被下令机动的兵团必须真的走出去了
	TWeakObjectPtr<ALegionUnit> Mover = TestMover;
	const FVector Start = TestMoverStart;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Mover, Start](float)
	{
		if (const ALegionUnit* L = Mover.Get())
		{
			const float Moved = FVector::Dist2D(L->GetActorLocation(), Start);
			UE_LOG(LogERLegion, Log, TEXT("[MapSelfTest] %s: legion moved %.0f cm within 4s after returning to the battlefield"),
				Moved > 600.0f ? TEXT("PASS") : TEXT("FAIL"), Moved);
		}
		return false;
	}), 4.0f);
}

// ---------------- 编队 / 命令 ----------------

void ALegionPlayerController::ComputeSlotsFor(const TArray<ALegionUnit*>& Legions, const FVector& Center,
	const FVector& FacingDir, TArray<FVector>& OutSlots) const
{
	OutSlots.Reset();
	const int32 N = Legions.Num();
	if (N == 0) { return; }

	const FVector Fwd = FacingDir.IsNearlyZero() ? FVector::ForwardVector : FacingDir.GetSafeNormal2D();
	const FVector Right(-Fwd.Y, Fwd.X, 0.0f);
	const float SlotSpacing = 1250.0f;

	float MinX = -MAX_FLT, MaxX = MAX_FLT, MinY = -MAX_FLT, MaxY = MAX_FLT;
	if (ALegionGameMode* GM = GetLegionGM(GetWorld()))
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

void ALegionPlayerController::ComputeFormationSlots(const FVector& Center, const FVector& FacingDir, TArray<FVector>& OutSlots) const
{
	TArray<ALegionUnit*> Valid;
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get())
		{
			if (!L->IsWiped()) { Valid.Add(L); }
		}
	}
	ComputeSlotsFor(Valid, Center, FacingDir, OutSlots);
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
		StopFollow(L);
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
				StopFollow(Sel);
				Sel->CommandAttack(Target);
			}
		}
	}
}

// ---------------- 键盘 ----------------

void ALegionPlayerController::OnKeySelect(int32 Index)
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	if (!GM) { return; }
	const TArray<TObjectPtr<ALegionUnit>>& Ls = GM->GetPlayerLegions();
	if (Ls.IsValidIndex(Index) && Ls[Index] && !Ls[Index]->IsWiped())
	{
		SetSingleSelection(Ls[Index]);
		if (bMapOpen)
		{
			int32 SX = 0, SY = 0;
			GetViewportSize(SX, SY);
			FVector2D UV;
			if (GM->WorldToMinimapUV(Ls[Index]->GetActorLocation(), UV))
			{
				MapView.CenterUV = UV;
				MapView.Clamp(SX, SY);
			}
		}
	}
}

void ALegionPlayerController::OnKeyX()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	PruneInvalidSelections();
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get())
		{
			StopFollow(L);
			L->CommandHalt();
		}
	}
}

void ALegionPlayerController::OnKeyG()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	if (ALegionGameMode* GM = GetLegionGM(GetWorld()))
	{
		FollowLegions.Reset();
		GM->AllOutAssault(ELegionTeam::Player);
	}
}

void ALegionPlayerController::OnKeyF()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	ALegionGameMode* GM = GetLegionGM(GetWorld());
	if (!GM) { return; }
	PruneInvalidSelections();

	TArray<ALegionUnit*> Targets;
	for (const TWeakObjectPtr<ALegionUnit>& Ptr : SelectedLegions)
	{
		if (ALegionUnit* L = Ptr.Get()) { Targets.Add(L); }
	}
	if (Targets.Num() == 0)
	{
		for (ALegionUnit* L : GM->GetPlayerLegions())
		{
			if (L && !L->IsWiped()) { Targets.Add(L); }
		}
	}

	bool bAllFollowing = Targets.Num() > 0;
	for (ALegionUnit* L : Targets)
	{
		if (!IsLegionFollowing(L)) { bAllFollowing = false; break; }
	}
	for (ALegionUnit* L : Targets)
	{
		if (bAllFollowing) { StopFollow(L); }
		else if (!IsLegionFollowing(L)) { FollowLegions.Add(L); }
	}
	FollowTimer = 0.0f;
}

void ALegionPlayerController::OnEscape()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	if (bMapOpen)
	{
		SetStrategicMapOpen(false);
		return;
	}
	ClearSelection();
}

void ALegionPlayerController::OnKeyMapToggle()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	SetStrategicMapOpen(!bMapOpen);
}

void ALegionPlayerController::OnKeyRestart()
{
	if (UERAICommandPanel::IsAnyTextInputFocused()) { return; }
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}
