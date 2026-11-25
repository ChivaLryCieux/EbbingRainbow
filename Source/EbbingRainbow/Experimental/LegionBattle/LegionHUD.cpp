// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionHUD.h"

#include "Blueprint/UserWidget.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionHUDWidget.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "GameFramework/PlayerController.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"

void ALegionHUD::BeginPlay()
{
	Super::BeginPlay();
	LoadPlaceholderLogos();
	if (APlayerController* PC = GetOwningPlayerController())
	{
		Widget = CreateWidget<ULegionHUDWidget>(PC, ULegionHUDWidget::StaticClass());
		if (Widget)
		{
			Widget->AddToViewport(0);
		}
	}
}

void ALegionHUD::LoadPlaceholderLogos()
{
	AllyClassLogos.Reset();
	EnemyClassLogos.Reset();
	static const ELegionClass Classes[4] = {
		ELegionClass::Shieldbearer, ELegionClass::Pikeman, ELegionClass::Vanguard, ELegionClass::Marksman
	};
	for (ELegionClass Cls : Classes)
	{
		if (UTexture2D* AllyTex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetClassLogoAssetPath(Cls, true)))
		{
			AllyClassLogos.Add(Cls, AllyTex);
		}
		if (UTexture2D* EnemyTex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetClassLogoAssetPath(Cls, false)))
		{
			EnemyClassLogos.Add(Cls, EnemyTex);
		}
	}
}

UTexture2D* ALegionHUD::GetClassLogoTexture(ELegionClass Archetype, bool bAlly) const
{
	const TMap<ELegionClass, TObjectPtr<UTexture2D>>& Map = bAlly ? AllyClassLogos : EnemyClassLogos;
	if (const TObjectPtr<UTexture2D>* Found = Map.Find(Archetype))
	{
		return Found->Get();
	}
	return nullptr;
}

void ALegionHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ALegionHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }

	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetOwningPlayerController());
	if (!GM || !PC) { return; }

	// 等高线地图视角: 全屏地图接管整个画面
	if (PC->IsStrategicMapOpen())
	{
		EnemySkyBannerEntries.Reset();
		DrawStrategicMap(GM, PC);
		return;
	}

	EnemySkyBannerEntries.Reset();

	// 1. 远程齐射弹道 / 技能冲击线
	for (const FLegionTracerBeam& B : GM->GetTracerBeams())
	{
		const float Alpha = FMath::Clamp(B.Remaining / FMath::Max(0.01f, B.Duration), 0.0f, 1.0f);
		FLinearColor C = B.Color;
		C.A *= Alpha;
		DrawProjectedLine(B.Start, B.End, C, 2.2f);
	}

	// 2. 敌军增援空降点预警
	const float Now = GM->GetBattleElapsed();
	for (const FPendingEnemyWave& PW : GM->GetPendingWaves())
	{
		if (PW.bSpawned) { continue; }
		const float Rem = FMath::Max(0.0f, PW.Spec.DelaySeconds - Now);
		const FString Label = FString::Printf(TEXT("增援空降 %s ×%d"),
			*ERGalacticMeta::GetClassCode(PW.Spec.Archetype), PW.Spec.SoldierCount);
		DrawDropZoneWarning(PW.DropLocation, Label, Rem);
	}

	// 3. 右键拖拽阵型预览与朝向箭头
	if (PC->IsRightDragActive())
	{
		FVector DragStart, DragEnd;
		PC->GetRightDragWorld(DragStart, DragEnd);
		const FVector Delta = DragEnd - DragStart;
		const FVector FacingDir = (Delta.Size2D() > 150.0f) ? Delta.GetSafeNormal2D() : FVector::ForwardVector;

		TArray<FVector> Slots;
		PC->ComputeFormationSlots(DragStart, FacingDir, Slots);

		const FLinearColor Gold(0.96f, 0.56f, 0.04f, 0.92f);
		const FVector Right(-FacingDir.Y, FacingDir.X, 0.0f);
		for (const FVector& Slot : Slots)
		{
			const FVector P1 = Slot - Right * 460.0f + FVector(0, 0, 50);
			const FVector P2 = Slot + Right * 460.0f + FVector(0, 0, 50);
			DrawProjectedLine(P1, P2, Gold, 3.0f);
			DrawProjectedLine(Slot + FVector(0, 0, 50), Slot + FacingDir * 520.0f + FVector(0, 0, 50), Gold, 2.5f);
		}
		if (Delta.Size2D() > 150.0f)
		{
			DrawProjectedLine(DragStart + FVector(0, 0, 55), DragEnd + FVector(0, 0, 55), FLinearColor::White, 2.0f);
		}
	}

	// 3.5 我方兵团的命令反馈: 地面路线 / 终点环 / 朝向箭头 / 集火连线
	DrawOrderOverlays(GM, PC);

	// 4. 我方兵团地面战术旗标 + 敌方兵团直插天空部队 Logo (按距离由远到近绘制, 近处优先叠层)
	for (ALegionUnit* L : GM->GetPlayerLegions())
	{
		if (L && !L->IsWiped())
		{
			DrawUnitBadge(L, PC->IsLegionSelected(L));
		}
	}

	// 收集当前我方已下达集火的敌方目标集合 (用于在天空 Logo 上亮起集火锁定标记)
	TSet<const ALegionUnit*> FocusedEnemies;
	const bool bHasSelected = PC->GetSelectedLegions().Num() > 0;
	for (ALegionUnit* Ally : GM->GetPlayerLegions())
	{
		if (!Ally || Ally->IsWiped()) { continue; }
		if (bHasSelected && !PC->IsLegionSelected(Ally)) { continue; }
		if (const ALegionUnit* Tgt = Ally->GetAttackTarget())
		{
			if (!Tgt->IsWiped()) { FocusedEnemies.Add(Tgt); }
		}
	}

	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	TArray<ALegionUnit*> SortedEnemies;
	for (ALegionUnit* L : GM->GetEnemyLegions())
	{
		if (L && !L->IsWiped()) { SortedEnemies.Add(L); }
	}
	SortedEnemies.Sort([&CamLoc](const ALegionUnit& A, const ALegionUnit& B)
	{
		return FVector::DistSquared(A.GetSkyBannerWorldLocation(), CamLoc)
			> FVector::DistSquared(B.GetSkyBannerWorldLocation(), CamLoc);
	});
	for (ALegionUnit* L : SortedEnemies)
	{
		DrawUnitBadge(L, false);
		DrawEnemySkyBanner(L, PC, PC->GetAimedLegion() == L, FocusedEnemies.Contains(L));
	}

	// 5. 左键框选矩形
	if (PC->IsMarqueeActive())
	{
		FVector2D MinP, MaxP;
		PC->GetMarqueeScreenRect(MinP, MaxP);
		const float W = MaxP.X - MinP.X;
		const float H = MaxP.Y - MinP.Y;
		if (W > 2.0f && H > 2.0f)
		{
			DrawRect(FLinearColor(0.05f, 0.56f, 0.94f, 0.14f), MinP.X, MinP.Y, W, H);
			const FLinearColor Border(0.96f, 0.56f, 0.04f, 0.92f);
			DrawLine(MinP.X, MinP.Y, MaxP.X, MinP.Y, Border, 1.5f);
			DrawLine(MaxP.X, MinP.Y, MaxP.X, MaxP.Y, Border, 1.5f);
			DrawLine(MaxP.X, MaxP.Y, MinP.X, MaxP.Y, Border, 1.5f);
			DrawLine(MinP.X, MaxP.Y, MinP.X, MinP.Y, Border, 1.5f);
		}
	}

	// 6. 左上角黑白等高线小地图 (点击/按 M 进入等高线地图视角)
	DrawMinimap(GM, PC);

	// 7. 第一人称准星 (光标呼出时不画)
	DrawCrosshair(PC);
}

void ALegionHUD::DrawOrderOverlays(ALegionGameMode* GM, ALegionPlayerController* PC)
{
	const UERUITheme* T = ERUI::Theme();
	const FLinearColor Gold(0.96f, 0.56f, 0.04f, 1.0f);
	const FLinearColor Hostile(1.0f, 0.25f, 0.18f, 1.0f);
	const FVector Up(0.0f, 0.0f, 55.0f);

	auto Ring = [&](const FVector& C, float R, const FLinearColor& Col, float Th)
	{
		constexpr int32 Segs = 20;
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = (float)s / Segs * 2.0f * PI;
			const float A1 = (float)(s + 1) / Segs * 2.0f * PI;
			DrawProjectedLine(C + FVector(FMath::Cos(A0) * R, FMath::Sin(A0) * R, 0.0f),
				C + FVector(FMath::Cos(A1) * R, FMath::Sin(A1) * R, 0.0f), Col, Th);
		}
	};
	auto Arrow = [&](const FVector& From, const FVector& To, const FLinearColor& Col, float Head, float Th)
	{
		DrawProjectedLine(From, To, Col, Th);
		const FVector D = (To - From).GetSafeNormal2D();
		const FVector N(-D.Y, D.X, 0.0f);
		DrawProjectedLine(To, To - D * Head + N * Head * 0.55f, Col, Th);
		DrawProjectedLine(To, To - D * Head - N * Head * 0.55f, Col, Th);
	};

	for (ALegionUnit* L : GM->GetPlayerLegions())
	{
		if (!L || L->IsWiped()) { continue; }
		const bool bSel = PC->IsLegionSelected(L);
		const float Th = bSel ? 2.4f : 1.3f;

		if (ALegionUnit* Tgt = L->GetAttackTarget())
		{
			if (Tgt->IsWiped()) { continue; }
			FLinearColor C = Hostile;
			C.A = bSel ? 0.9f : 0.4f;
			const FVector A = L->GetActorLocation() + Up;
			const FVector B = Tgt->GetActorLocation() + Up;
			Arrow(A, B, C, 260.0f, Th);
			Ring(Tgt->GetActorLocation() + Up, 420.0f, C, Th);
		}
		else if (L->HasMoveOrder())
		{
			FLinearColor C = bSel ? Gold : T->AllyMark;
			C.A = bSel ? 0.95f : 0.45f;

			TArray<FVector> Path;
			L->GetRemainingPath(Path);
			FVector Prev = L->GetActorLocation() + Up;
			FVector BeforeLast = Prev;
			FVector Last = Prev;
			for (const FVector& W : Path)
			{
				const FVector P = W + Up;
				DrawProjectedLine(Prev, P, C, Th);
				BeforeLast = Prev;
				Last = P;
				Prev = P;
			}

			Ring(Last, 260.0f, C, Th);
			FVector Face = (Last - BeforeLast).GetSafeNormal2D();
			if (L->HasCustomFacing()) { Face = FRotator(0.0f, L->GetDesiredFacingYaw(), 0.0f).Vector(); }
			if (!Face.IsNearlyZero()) { Arrow(Last + Face * 260.0f, Last + Face * 900.0f, C, 220.0f, Th + 0.4f); }
		}
	}
}

void ALegionHUD::DrawCrosshair(ALegionPlayerController* PC)
{
	if (!Canvas || !PC || PC->bShowMouseCursor) { return; }

	const UERUITheme* T = ERUI::Theme();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	ALegionUnit* Aim = PC->GetAimedLegion();
	FLinearColor Col(0.95f, 0.95f, 0.92f, 0.75f);
	if (Aim && !Aim->IsWiped())
	{
		const bool bEnemy = Aim->GetTeam() == ELegionTeam::Enemy;
		Col = bEnemy ? (PC->GetSelectedLegions().Num() > 0 ? FLinearColor(1.0f, 0.25f, 0.18f, 1.0f) : T->EnemyMark) : T->AllyMark;
	}

	const float Gap = Aim ? 7.0f : 5.0f;
	const float Len = Aim ? 11.0f : 8.0f;
	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, Col, 1.6f);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, Col, 1.6f);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, Col, 1.6f);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, Col, 1.6f);
	DrawRect(Col, CX - 1.0f, CY - 1.0f, 2.0f, 2.0f);

	if (Aim && !Aim->IsWiped())
	{
		const bool bEnemy = Aim->GetTeam() == ELegionTeam::Enemy;
		UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

		if (bEnemy)
		{
			// 瞄准天空 Logo 时把提示放在铭牌下方并配暗曜石底衬, 不与 Logo / 番号文字重叠
			float BannerBotY = CY + 62.0f;
			if (GetEnemySkyBannerBottomY(Aim, BannerBotY))
			{
				BannerBotY = FMath::Max(BannerBotY + 12.0f, CY + 54.0f);
			}

			const bool bHasSel = PC->GetSelectedLegions().Num() > 0;
			const FString Hint = bHasSel
				? FString::Printf(TEXT("右键 · 集火  %s"), *Aim->GetCallsign())
				: FString::Printf(TEXT("%s  ·  先选中我方部队"), *Aim->GetCallsign());
			float HW = 0.0f, HH = 0.0f;
			Canvas->TextSize(Font, Hint, HW, HH, 1.0f, 1.0f);

			const float PadX = 10.0f;
			const float PadY = 4.0f;
			const float PillW = HW + PadX * 2.0f + 4.0f;
			const float PillH = HH + PadY * 2.0f;
			const float PillX = CX - PillW * 0.5f;
			const float PillY = BannerBotY;
			const FLinearColor BarCol = bHasSel ? FLinearColor(1.0f, 0.28f, 0.16f, 1.0f) : T->BoneSecondary;

			DrawRect(FLinearColor(0.012f, 0.014f, 0.020f, 0.88f), PillX, PillY, PillW, PillH);
			DrawRect(BarCol, PillX, PillY, 4.0f, PillH);
			DrawText(Hint, bHasSel ? T->Bone : T->BoneSecondary, PillX + 4.0f + PadX, PillY + PadY, Font, 1.0f, false);
		}
		else
		{
			const FString Label = FString::Printf(TEXT("%s  %d/%d"), *Aim->GetCallsign(), Aim->GetAliveCount(), Aim->GetSoldierCount());
			float TW = 0.0f, TH = 0.0f;
			Canvas->TextSize(Font, Label, TW, TH, 1.0f, 1.0f);
			DrawText(Label, Col, CX - TW * 0.5f, CY + 26.0f, Font, 1.0f, false);
		}
	}
}

void ALegionHUD::GetMinimapScreenRect(int32 ViewportSizeY, float& OutX, float& OutY, float& OutSize)
{
	const float Scale = FMath::Max(0.5f, (float)ViewportSizeY / 1080.0f);
	OutX = 48.0f * Scale;
	OutY = 108.0f * Scale;
	OutSize = 204.0f * Scale;
}

void ALegionHUD::DrawMinimap(ALegionGameMode* GM, ALegionPlayerController* PC)
{
	if (!Canvas || !GM) { return; }
	UTexture2D* MapTex = GM->GetMinimapTexture();
	if (!MapTex || !MapTex->GetResource()) { return; }

	float MX = 0.0f, MY = 0.0f, MS = 0.0f;
	GetMinimapScreenRect(Canvas->SizeY, MX, MY, MS);

	// 衬底与外框
	DrawRect(FLinearColor(0.01f, 0.01f, 0.015f, 0.92f), MX - 2.0f, MY - 2.0f, MS + 4.0f, MS + 4.0f);

	FCanvasTileItem Tile(FVector2D(MX, MY), MapTex->GetResource(), FVector2D(MS, MS), FLinearColor::White);
	Tile.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Tile);

	const FLinearColor BorderCol(0.92f, 0.92f, 0.90f, 0.42f);
	DrawLine(MX, MY, MX + MS, MY, BorderCol, 1.0f);
	DrawLine(MX + MS, MY, MX + MS, MY + MS, BorderCol, 1.0f);
	DrawLine(MX + MS, MY + MS, MX, MY + MS, BorderCol, 1.0f);
	DrawLine(MX, MY + MS, MX, MY, BorderCol, 1.0f);

	// 摄像机焦点准星
	if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
	{
		FVector2D CamUV;
		if (GM->WorldToMinimapUV(Pawn->GetActorLocation(), CamUV))
		{
			const float CX = MX + CamUV.X * MS;
			const float CY = MY + CamUV.Y * MS;
			const FLinearColor CamCol(0.95f, 0.95f, 0.92f, 0.55f);
			DrawLine(CX - 9.0f, CY, CX + 9.0f, CY, CamCol, 1.0f);
			DrawLine(CX, CY - 9.0f, CX, CY + 9.0f, CamCol, 1.0f);
		}
	}

	// 待降敌军空降预警点 (空心红框)
	for (const FPendingEnemyWave& PW : GM->GetPendingWaves())
	{
		if (PW.bSpawned) { continue; }
		FVector2D UV;
		if (GM->WorldToMinimapUV(PW.DropLocation, UV))
		{
			const float DX = MX + UV.X * MS;
			const float DY = MY + UV.Y * MS;
			const FLinearColor DropCol(0.96f, 0.20f, 0.16f, 0.85f);
			DrawLine(DX - 4.0f, DY - 4.0f, DX + 4.0f, DY - 4.0f, DropCol, 1.2f);
			DrawLine(DX + 4.0f, DY - 4.0f, DX + 4.0f, DY + 4.0f, DropCol, 1.2f);
			DrawLine(DX + 4.0f, DY + 4.0f, DX - 4.0f, DY + 4.0f, DropCol, 1.2f);
			DrawLine(DX - 4.0f, DY + 4.0f, DX - 4.0f, DY - 4.0f, DropCol, 1.2f);
		}
	}

	// 敌方位置 (红点)
	const FLinearColor RedDot(0.94f, 0.14f, 0.12f, 1.0f);
	for (ALegionUnit* E : GM->GetEnemyLegions())
	{
		if (!E || E->IsWiped()) { continue; }
		FVector2D UV;
		if (GM->WorldToMinimapUV(E->GetActorLocation(), UV))
		{
			const float PX = MX + UV.X * MS;
			const float PY = MY + UV.Y * MS;
			DrawRect(FLinearColor::Black, PX - 4.5f, PY - 4.5f, 9.0f, 9.0f);
			DrawRect(RedDot, PX - 3.5f, PY - 3.5f, 7.0f, 7.0f);
		}
	}

	// 我方位置 (蓝点, 选中态加金圈)
	const FLinearColor BlueDot(0.08f, 0.62f, 0.98f, 1.0f);
	const FLinearColor GoldSel(0.96f, 0.56f, 0.04f, 1.0f);
	for (ALegionUnit* P : GM->GetPlayerLegions())
	{
		if (!P || P->IsWiped()) { continue; }
		FVector2D UV;
		if (GM->WorldToMinimapUV(P->GetActorLocation(), UV))
		{
			const float PX = MX + UV.X * MS;
			const float PY = MY + UV.Y * MS;
			if (PC && PC->IsLegionSelected(P))
			{
				DrawRect(GoldSel, PX - 6.0f, PY - 6.0f, 12.0f, 12.0f);
			}
			DrawRect(FLinearColor::Black, PX - 4.5f, PY - 4.5f, 9.0f, 9.0f);
			DrawRect(BlueDot, PX - 3.5f, PY - 3.5f, 7.0f, 7.0f);
		}
	}

}

void ALegionHUD::NotifyStrategicMapOpen(bool bMapOpen)
{
	if (Widget)
	{
		Widget->SetStrategicMapMode(bMapOpen);
	}
}

FBox2D ALegionHUD::GetMapReturnButtonRect(int32 ViewportSizeX, int32 ViewportSizeY)
{
	const float Scale = FMath::Max(0.75f, (float)ViewportSizeY / 1080.0f);
	const float MarginX = 48.0f * Scale;
	const float W = 224.0f * Scale;
	const float H = 48.0f * Scale;
	const float X = (float)ViewportSizeX - MarginX - W;
	const float Y = 34.0f * Scale;
	return FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
}

ALegionUnit* ALegionHUD::PickStrategicBadge(const FVector2D& ScreenPos) const
{
	// 逆序命中: 后绘制 (我方) 的番号牌在上层, 重叠时优先
	for (int32 i = StrategicBadgeRects.Num() - 1; i >= 0; --i)
	{
		if (StrategicBadgeRects[i].Rect.IsInside(ScreenPos))
		{
			return StrategicBadgeRects[i].Unit.Get();
		}
	}
	return nullptr;
}

bool ALegionHUD::GetStrategicBadgeCenter(const ALegionUnit* Unit, FVector2D& OutCenter) const
{
	for (const FStrategicBadgeRect& B : StrategicBadgeRects)
	{
		if (B.Unit.Get() == Unit)
		{
			OutCenter = B.Rect.GetCenter();
			return true;
		}
	}
	return false;
}

ALegionUnit* ALegionHUD::PickEnemySkyBanner(const FVector2D& ScreenPos) const
{
	ALegionUnit* Best = nullptr;
	double BestDistSq = MAX_dbl;
	for (const FEnemySkyBannerEntry& E : EnemySkyBannerEntries)
	{
		ALegionUnit* U = E.Unit.Get();
		if (!U || U->IsWiped()) { continue; }
		if (!E.HitRect.IsInside(ScreenPos)) { continue; }
		const double D2 = FVector2D::DistSquared(ScreenPos, E.Center);
		if (D2 < BestDistSq)
		{
			BestDistSq = D2;
			Best = U;
		}
	}
	return Best;
}

bool ALegionHUD::GetEnemySkyBannerScreenCenter(const ALegionUnit* Unit, FVector2D& OutCenter) const
{
	for (const FEnemySkyBannerEntry& E : EnemySkyBannerEntries)
	{
		if (E.Unit.Get() == Unit)
		{
			OutCenter = E.Center;
			return true;
		}
	}
	return false;
}

bool ALegionHUD::GetEnemySkyBannerBottomY(const ALegionUnit* Unit, float& OutBottomY) const
{
	for (const FEnemySkyBannerEntry& E : EnemySkyBannerEntries)
	{
		if (E.Unit.Get() == Unit)
		{
			OutBottomY = (float)E.HitRect.Max.Y - 16.0f;
			return true;
		}
	}
	return false;
}

void ALegionHUD::DrawStrategicMap(ALegionGameMode* GM, ALegionPlayerController* PC)
{
	StrategicBadgeRects.Reset();
	if (!Canvas || !GM || !PC) { return; }

	const UERUITheme* T = ERUI::Theme();
	UFont* Font = GEngine->GetSmallFont();
	const int32 SX = Canvas->SizeX;
	const int32 SY = Canvas->SizeY;
	const FLegionMapView& View = PC->GetMapView();
	const float Side = View.Side(SX, SY);

	// 全屏深色衬底 (地图小于视口的轴向留出的边)
	DrawRect(FLinearColor(0.007f, 0.008f, 0.012f, 1.0f), 0.0f, 0.0f, (float)SX, (float)SY);

	// 等高线底图: 放大到高精度纹理阈值后切换 3072px 细节图, 避免拉伸发糊
	UTexture2D* MapTex = GM->GetStrategicMapTexture();
	if (UTexture2D* Detail = GM->GetDetailMapTexture())
	{
		if (Side > 1700.0f && Detail->GetResource()) { MapTex = Detail; }
	}
	const FVector2D MapMin = View.UVToScreen(FVector2D(0.0, 0.0), SX, SY);
	if (MapTex && MapTex->GetResource())
	{
		FCanvasTileItem Tile(MapMin, MapTex->GetResource(), FVector2D(Side, Side), FLinearColor::White);
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Tile);
	}
	const FLinearColor EdgeCol(0.92f, 0.92f, 0.90f, 0.55f);
	DrawLine(MapMin.X, MapMin.Y, MapMin.X + Side, MapMin.Y, EdgeCol, 1.5f);
	DrawLine(MapMin.X + Side, MapMin.Y, MapMin.X + Side, MapMin.Y + Side, EdgeCol, 1.5f);
	DrawLine(MapMin.X + Side, MapMin.Y + Side, MapMin.X, MapMin.Y + Side, EdgeCol, 1.5f);
	DrawLine(MapMin.X, MapMin.Y + Side, MapMin.X, MapMin.Y, EdgeCol, 1.5f);

	auto ToScreen = [&](const FVector& World, FVector2D& Out) -> bool
	{
		FVector2D UV;
		if (!GM->WorldToMinimapUV(World, UV)) { return false; }
		Out = View.UVToScreen(UV, SX, SY);
		return true;
	};
	auto OnScreen = [&](const FVector2D& P, float Margin) -> bool
	{
		return P.X >= -Margin && P.X <= SX + Margin && P.Y >= -Margin && P.Y <= SY + Margin;
	};

	// 指挥官标记: 三角 + 视野扇面 (世界 +X = 屏幕左, 世界 +Y = 屏幕上)
	if (APawn* Pawn = PC->GetPawn())
	{
		FVector2D P;
		if (ToScreen(Pawn->GetActorLocation(), P))
		{
			const float YawRad = FMath::DegreesToRadians(PC->GetControlRotation().Yaw);
			const FVector2D D(-FMath::Cos(YawRad), -FMath::Sin(YawRad));
			const FVector2D N(-D.Y, D.X);
			const FLinearColor Col(0.97f, 0.97f, 0.94f, 0.95f);
			const FLinearColor Cone(0.97f, 0.97f, 0.94f, 0.30f);
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const float A = FMath::DegreesToRadians(44.0f * Sgn);
				const FVector2D Dir = D * FMath::Cos(A) + N * FMath::Sin(A);
				DrawLine(P.X, P.Y, P.X + Dir.X * 70.0f, P.Y + Dir.Y * 70.0f, Cone, 1.0f);
			}
			const FVector2D Tip = P + D * 13.0f;
			const FVector2D L = P - D * 8.0f + N * 8.0f;
			const FVector2D R = P - D * 8.0f - N * 8.0f;
			DrawLine(Tip.X, Tip.Y, L.X, L.Y, Col, 2.0f);
			DrawLine(L.X, L.Y, R.X, R.Y, Col, 2.0f);
			DrawLine(R.X, R.Y, Tip.X, Tip.Y, Col, 2.0f);
		}
	}

	// 待降波次预警
	for (const FPendingEnemyWave& PW : GM->GetPendingWaves())
	{
		if (PW.bSpawned) { continue; }
		FVector2D P;
		if (ToScreen(PW.DropLocation, P) && OnScreen(P, 40.0f))
		{
			const float R = 9.0f;
			const FLinearColor DropCol(0.96f, 0.20f, 0.16f, 0.85f);
			DrawLine(P.X, P.Y - R, P.X + R, P.Y, DropCol, 1.4f);
			DrawLine(P.X + R, P.Y, P.X, P.Y + R, DropCol, 1.4f);
			DrawLine(P.X, P.Y + R, P.X - R, P.Y, DropCol, 1.4f);
			DrawLine(P.X - R, P.Y, P.X, P.Y - R, DropCol, 1.4f);
			DrawText(FString::Printf(TEXT("增援空降 %s ×%d  %.0f 秒后"),
					*ERGalacticMeta::GetClassCode(PW.Spec.Archetype), PW.Spec.SoldierCount,
					FMath::Max(0.0f, PW.Spec.DelaySeconds - GM->GetBattleElapsed())),
				DropCol, P.X + R + 4.0f, P.Y - 7.0f, Font, 0.85f, false);
		}
	}

	// 命令反馈: 我方兵团的移动路线 (虚线) + 终点靶标 + 朝向箭头, 集火连线 + 目标靶标
	{
		const FLinearColor Gold(0.96f, 0.56f, 0.04f, 1.0f);
		const FLinearColor Hostile(1.0f, 0.25f, 0.18f, 1.0f);

		auto DrawDashed = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& C, float Th)
		{
			const FVector2D D = B - A;
			const float Len = (float)D.Size();
			if (Len < 1.0f) { return; }
			const FVector2D Dir = D / Len;
			for (float S = 0.0f; S < Len; S += 17.0f)
			{
				const float E = FMath::Min(Len, S + 10.0f);
				DrawLine(A.X + Dir.X * S, A.Y + Dir.Y * S, A.X + Dir.X * E, A.Y + Dir.Y * E, C, Th);
			}
		};
		auto DrawArrowHead = [&](const FVector2D& From, const FVector2D& Tip, const FLinearColor& C, float Size, float Th)
		{
			const FVector2D D = (Tip - From).GetSafeNormal();
			if (D.IsNearlyZero()) { return; }
			const FVector2D N(-D.Y, D.X);
			const FVector2D L = Tip - D * Size + N * Size * 0.55;
			const FVector2D R = Tip - D * Size - N * Size * 0.55;
			DrawLine(Tip.X, Tip.Y, L.X, L.Y, C, Th);
			DrawLine(Tip.X, Tip.Y, R.X, R.Y, C, Th);
		};
		auto DrawRing = [&](const FVector2D& Ctr, float R, const FLinearColor& C, float Th)
		{
			constexpr int32 Segs = 20;
			for (int32 s = 0; s < Segs; ++s)
			{
				const float A0 = (float)s / Segs * 2.0f * PI;
				const float A1 = (float)(s + 1) / Segs * 2.0f * PI;
				DrawLine(Ctr.X + FMath::Cos(A0) * R, Ctr.Y + FMath::Sin(A0) * R,
					Ctr.X + FMath::Cos(A1) * R, Ctr.Y + FMath::Sin(A1) * R, C, Th);
			}
		};

		for (ALegionUnit* L : GM->GetPlayerLegions())
		{
			if (!L || L->IsWiped()) { continue; }
			const bool bSel = PC->IsLegionSelected(L);
			FVector2D PL;
			if (!ToScreen(L->GetActorLocation(), PL)) { continue; }

			if (ALegionUnit* Tgt = L->GetAttackTarget())
			{
				FVector2D PT;
				if (!Tgt->IsWiped() && ToScreen(Tgt->GetActorLocation(), PT))
				{
					FLinearColor C = Hostile;
					C.A = bSel ? 0.95f : 0.45f;
					const float Th = bSel ? 2.2f : 1.2f;
					DrawDashed(PL, PT, C, Th);
					DrawArrowHead(PL, PT, C, bSel ? 14.0f : 10.0f, Th);
					DrawRing(PT, bSel ? 17.0f : 13.0f, C, Th);
					DrawLine(PT.X - 22.0f, PT.Y, PT.X - 9.0f, PT.Y, C, Th);
					DrawLine(PT.X + 9.0f, PT.Y, PT.X + 22.0f, PT.Y, C, Th);
					DrawLine(PT.X, PT.Y - 22.0f, PT.X, PT.Y - 9.0f, C, Th);
					DrawLine(PT.X, PT.Y + 9.0f, PT.X, PT.Y + 22.0f, C, Th);
				}
			}
			else if (L->HasMoveOrder())
			{
				FLinearColor C = bSel ? Gold : T->AllyMark;
				C.A = bSel ? 0.95f : 0.5f;
				const float Th = bSel ? 2.2f : 1.3f;

				TArray<FVector> Path;
				L->GetRemainingPath(Path);
				FVector2D Prev = PL;
				FVector2D Last = PL;
				FVector2D BeforeLast = PL;
				for (const FVector& W : Path)
				{
					FVector2D PW;
					if (!ToScreen(W, PW)) { continue; }
					DrawDashed(Prev, PW, C, Th);
					BeforeLast = Prev;
					Last = PW;
					Prev = PW;
				}

				// 终点靶标 + 到达朝向箭头
				const float R = bSel ? 15.0f : 11.0f;
				DrawRing(Last, R, C, Th);
				DrawRect(C, Last.X - 2.5f, Last.Y - 2.5f, 5.0f, 5.0f);
				DrawArrowHead(BeforeLast, Last - (Last - BeforeLast).GetSafeNormal() * R, C, bSel ? 12.0f : 9.0f, Th);

				FVector2D FaceDir = (Last - BeforeLast).GetSafeNormal();
				if (L->HasCustomFacing())
				{
					const float Y = FMath::DegreesToRadians(L->GetDesiredFacingYaw());
					FaceDir = FVector2D(-FMath::Cos(Y), -FMath::Sin(Y)); // 世界 +X = 屏幕左, 世界 +Y = 屏幕上
				}
				if (!FaceDir.IsNearlyZero())
				{
					const float Len = bSel ? 44.0f : 32.0f;
					const FVector2D Tip = Last + FaceDir * (R + Len);
					DrawLine(Last.X + FaceDir.X * R, Last.Y + FaceDir.Y * R, Tip.X, Tip.Y, C, Th + 0.6f);
					DrawArrowHead(Last, Tip, C, bSel ? 11.0f : 8.0f, Th + 0.6f);
				}

				const FString& Tag = L->GetCallsign();
				DrawText(Tag, C, Last.X + R + 4.0f, Last.Y - 18.0f, Font, 0.85f, false);
			}
		}
	}

	// 番号牌: 大号 Logo + 两行紧凑卡片 + 聚类外扩/迭代排斥防碰撞 + 边缘精准引线
	{
		struct FMapBadgeItem
		{
			ALegionUnit* Unit = nullptr;
			bool bAlly = false;
			bool bSel = false;
			FLinearColor MarkCol = FLinearColor::White;
			FString Name;
			FString Sub;
			FVector2D Anchor = FVector2D::ZeroVector;
			FVector2D Pos = FVector2D::ZeroVector; // 卡片中心
			FVector2D Size = FVector2D::ZeroVector;
			float IconSz = 36.0f;
			float NameH = 0.0f;
			float SubH = 0.0f;
		};

		TArray<FMapBadgeItem> Items;
		auto CollectUnit = [&](ALegionUnit* L)
		{
			if (!L || L->IsWiped()) { return; }
			FVector2D Center;
			if (!ToScreen(L->GetActorLocation(), Center) || !OnScreen(Center, 80.0f)) { return; }

			FMapBadgeItem It;
			It.Unit = L;
			It.bAlly = (L->GetTeam() == ELegionTeam::Player);
			It.bSel = PC->IsLegionSelected(L);
			It.MarkCol = It.bSel ? T->Accent : (It.bAlly ? T->AllyMark : T->EnemyMark);
			It.Name = L->GetCallsign();
			if (It.Name.IsEmpty())
			{
				It.Name = It.bAlly ? ERGalacticMeta::GetBrigadeName(1) : ERGalacticMeta::GetEnemyBrigadeName(1);
			}
			It.Sub = FString::Printf(TEXT("%d / %d 兵"), L->GetAliveCount(), L->GetSoldierCount());
			if (It.bAlly && PC->IsLegionFollowing(L)) { It.Sub += TEXT(" · 跟随"); }

			It.IconSz = It.bSel ? 42.0f : 38.0f;
			float NameW = 0.0f, SubW = 0.0f;
			Canvas->TextSize(Font, It.Name, NameW, It.NameH, 1.32f, 1.32f);
			Canvas->TextSize(Font, It.Sub, SubW, It.SubH, 1.02f, 1.02f);

			constexpr float BarW = 4.0f;
			constexpr float PadX = 9.0f;
			constexpr float PadY = 7.0f;
			constexpr float IconGap = 9.0f;
			const float TextColW = FMath::Max(NameW, SubW);
			const float TextColH = It.NameH + It.SubH + 7.0f;
			const float W = BarW + PadX + (It.IconSz + 4.0f) + IconGap + TextColW + PadX;
			const float H = FMath::Max(It.IconSz + PadY * 2.0f + 4.0f, TextColH + PadY * 2.0f + 4.0f);

			It.Anchor = Center;
			It.Pos = Center;
			It.Size = FVector2D(W, H);
			Items.Add(MoveTemp(It));
		};

		for (ALegionUnit* E : GM->GetEnemyLegions()) { CollectUnit(E); }
		for (ALegionUnit* P : GM->GetPlayerLegions())
		{
			if (P && !PC->IsLegionSelected(P)) { CollectUnit(P); }
		}
		for (ALegionUnit* P : GM->GetPlayerLegions())
		{
			if (P && PC->IsLegionSelected(P)) { CollectUnit(P); }
		}

		// 安全排布区 & 固定 UI 禁区 (避开左上标题、左下兵力卡、右上返回按钮与缩放读数)
		const float Scale = FMath::Max(0.75f, (float)SY / 1080.0f);
		const FBox2D SafeRect(FVector2D(16.0f, 16.0f), FVector2D((float)SX - 16.0f, (float)SY - 16.0f));
		const FBox2D RetRect = GetMapReturnButtonRect(SX, SY);
		const FBox2D Exclusions[] = {
			FBox2D(FVector2D(RetRect.Min.X - 40.0f, RetRect.Min.Y - 12.0f), FVector2D(RetRect.Max.X + 16.0f, RetRect.Max.Y + 42.0f * Scale)),
			FBox2D(FVector2D(0.0f, 0.0f), FVector2D(360.0f * Scale, 170.0f * Scale)),
			FBox2D(FVector2D(0.0f, (float)SY - 150.0f * Scale), FVector2D(310.0f * Scale, (float)SY)),
		};

		auto ClampPos = [&](FMapBadgeItem& It)
		{
			const FVector2D Half = It.Size * 0.5f;
			It.Pos.X = FMath::Clamp(It.Pos.X, SafeRect.Min.X + Half.X, FMath::Max(SafeRect.Min.X + Half.X, SafeRect.Max.X - Half.X));
			It.Pos.Y = FMath::Clamp(It.Pos.Y, SafeRect.Min.Y + Half.Y, FMath::Max(SafeRect.Min.Y + Half.Y, SafeRect.Max.Y - Half.Y));
		};

		// 1. 聚类径向初散: 扎堆兵团先沿相对质心向外轻推, 减少引线交叉
		const int32 N = Items.Num();
		for (int32 i = 0; i < N; ++i)
		{
			FVector2D ClusterSum = FVector2D::ZeroVector;
			int32 ClusterCnt = 0;
			for (int32 j = 0; j < N; ++j)
			{
				if (FVector2D::DistSquared(Items[i].Anchor, Items[j].Anchor) < 180.0 * 180.0)
				{
					ClusterSum += Items[j].Anchor;
					++ClusterCnt;
				}
			}
			if (ClusterCnt > 1)
			{
				const FVector2D Centroid = ClusterSum / (double)ClusterCnt;
				FVector2D Out = Items[i].Anchor - Centroid;
				if (Out.SizeSquared() < 4.0)
				{
					const float Ang = (float)i * 2.39996f;
					Out = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang));
				}
				else
				{
					Out.Normalize();
				}
				Items[i].Pos = Items[i].Anchor + Out * 22.0f;
				ClampPos(Items[i]);
			}
		}

		// 2. 迭代 AABB 排斥松弛 + 柔和回拉弹簧 (保证高密度集群自动优雅散开、零重叠)
		constexpr float GapX = 12.0f;
		constexpr float GapY = 10.0f;
		constexpr int32 TotalIters = 28;
		for (int32 Iter = 0; Iter < TotalIters; ++Iter)
		{
			for (int32 i = 0; i < N; ++i)
			{
				for (int32 j = i + 1; j < N; ++j)
				{
					const double SumW = (Items[i].Size.X + Items[j].Size.X) * 0.5 + GapX;
					const double SumH = (Items[i].Size.Y + Items[j].Size.Y) * 0.5 + GapY;
					const double DX = Items[j].Pos.X - Items[i].Pos.X;
					const double DY = Items[j].Pos.Y - Items[i].Pos.Y;
					const double PenX = SumW - FMath::Abs(DX);
					const double PenY = SumH - FMath::Abs(DY);
					if (PenX > 0.0 && PenY > 0.0)
					{
						// 优先沿穿透更浅的轴分开 (轻微偏好水平/垂直自然扇开)
						if (PenX < PenY * 1.15)
						{
							double Sgn = (FMath::Abs(DX) > 0.5) ? (DX > 0.0 ? 1.0 : -1.0)
								: ((Items[j].Anchor.X >= Items[i].Anchor.X) ? 1.0 : -1.0);
							const double Push = PenX * 0.52;
							Items[i].Pos.X -= Sgn * Push;
							Items[j].Pos.X += Sgn * Push;
						}
						else
						{
							double Sgn = (FMath::Abs(DY) > 0.5) ? (DY > 0.0 ? 1.0 : -1.0)
								: ((Items[j].Anchor.Y >= Items[i].Anchor.Y) ? 1.0 : -1.0);
							const double Push = PenY * 0.52;
							Items[i].Pos.Y -= Sgn * Push;
							Items[j].Pos.Y += Sgn * Push;
						}
					}
				}
			}

			// 避开固定 UI 禁区 + 前段柔和拉回锚点
			const double Spring = (Iter < 18) ? 0.07 : 0.0;
			for (int32 i = 0; i < N; ++i)
			{
				if (Spring > 0.0)
				{
					Items[i].Pos = FMath::Lerp(Items[i].Pos, Items[i].Anchor, Spring);
				}
				const FVector2D Half = Items[i].Size * 0.5f;
				for (const FBox2D& Ex : Exclusions)
				{
					const FVector2D ExC = Ex.GetCenter();
					const FVector2D ExH = Ex.GetExtent();
					const double DX = Items[i].Pos.X - ExC.X;
					const double DY = Items[i].Pos.Y - ExC.Y;
					const double PenX = (Half.X + ExH.X + 8.0) - FMath::Abs(DX);
					const double PenY = (Half.Y + ExH.Y + 8.0) - FMath::Abs(DY);
					if (PenX > 0.0 && PenY > 0.0)
					{
						if (PenX < PenY) { Items[i].Pos.X += (DX >= 0.0 ? 1.0 : -1.0) * PenX; }
						else             { Items[i].Pos.Y += (DY >= 0.0 ? 1.0 : -1.0) * PenY; }
					}
				}
				ClampPos(Items[i]);
			}
		}

		// 3. 零重叠硬兜底: 若极端密集仍有残余重叠, 按螺旋环寻找最近无碰撞空位
		TArray<FBox2D> FinalRects;
		FinalRects.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			auto MakeRect = [&](const FVector2D& CenterPos) -> FBox2D
			{
				return FBox2D(CenterPos - Items[i].Size * 0.5f, CenterPos + Items[i].Size * 0.5f);
			};
			auto HasCollision = [&](const FBox2D& R) -> bool
			{
				const FBox2D PadR(R.Min - FVector2D(4.0, 4.0), R.Max + FVector2D(4.0, 4.0));
				for (const FBox2D& Prev : FinalRects)
				{
					if (PadR.Intersect(Prev)) { return true; }
				}
				for (const FBox2D& Ex : Exclusions)
				{
					if (PadR.Intersect(Ex)) { return true; }
				}
				return false;
			};

			FBox2D Chosen = MakeRect(Items[i].Pos);
			if (HasCollision(Chosen))
			{
				const FVector2D BaseDir = (Items[i].Pos - Items[i].Anchor).GetSafeNormal();
				const float BaseAng = BaseDir.IsNearlyZero() ? 0.0f : FMath::Atan2((float)BaseDir.Y, (float)BaseDir.X);
				bool bFound = false;
				for (float Rad = 30.0f; Rad <= 390.0f && !bFound; Rad += 28.0f)
				{
					for (int32 a = 0; a < 24; ++a)
					{
						const float Ang = BaseAng + ((float)a / 24.0f) * 2.0f * PI;
						FMapBadgeItem Probe = Items[i];
						Probe.Pos = Items[i].Anchor + FVector2D(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad);
						ClampPos(Probe);
						const FBox2D Cand = MakeRect(Probe.Pos);
						if (!HasCollision(Cand))
						{
							Items[i].Pos = Probe.Pos;
							Chosen = Cand;
							bFound = true;
							break;
						}
					}
				}
			}
			FinalRects.Add(Chosen);
		}

		// 4. 分层绘制: 先画真实地图锚点 + 贴边引导线, 最后叠画所有大号 Logo 番号卡
		for (int32 i = 0; i < N; ++i)
		{
			const FMapBadgeItem& It = Items[i];
			const FBox2D& Rect = FinalRects[i];

			// 真实部队锚点 (黑边彩心方块; 选中加外框)
			if (It.bSel)
			{
				DrawRect(It.MarkCol, It.Anchor.X - 6.0f, It.Anchor.Y - 6.0f, 12.0f, 12.0f);
			}
			DrawRect(FLinearColor::Black, It.Anchor.X - 4.5f, It.Anchor.Y - 4.5f, 9.0f, 9.0f);
			DrawRect(It.MarkCol, It.Anchor.X - 3.5f, It.Anchor.Y - 3.5f, 7.0f, 7.0f);

			// 引线从锚点连至卡片最近边缘点 (绝不穿过卡片内部)
			if (!Rect.IsInside(It.Anchor))
			{
				const FVector2D EdgePt(
					FMath::Clamp(It.Anchor.X, Rect.Min.X, Rect.Max.X),
					FMath::Clamp(It.Anchor.Y, Rect.Min.Y, Rect.Max.Y));
				if (FVector2D::DistSquared(EdgePt, It.Anchor) > 16.0)
				{
					FLinearColor LeadShadow(0.01f, 0.01f, 0.02f, 0.72f);
					DrawLine(EdgePt.X, EdgePt.Y, It.Anchor.X, It.Anchor.Y, LeadShadow, 3.2f);
					FLinearColor LeadCol = It.MarkCol;
					LeadCol.A = It.bSel ? 0.95f : 0.78f;
					DrawLine(EdgePt.X, EdgePt.Y, It.Anchor.X, It.Anchor.Y, LeadCol, It.bSel ? 1.8f : 1.3f);
				}
			}
		}

		for (int32 i = 0; i < N; ++i)
		{
			const FMapBadgeItem& It = Items[i];
			const FBox2D& Rect = FinalRects[i];
			const FVector2D Sz = Rect.GetSize();

			constexpr float BarW = 4.0f;
			constexpr float PadX = 9.0f;
			constexpr float IconGap = 9.0f;

			DrawBonePlate(Rect.Min.X, Rect.Min.Y, Sz.X, Sz.Y, It.MarkCol, false, It.bSel, false);

			// 大号部队 Logo (暗曜石内框 + 技能图标占位)
			const float IX = Rect.Min.X + BarW + PadX + 2.0f;
			const float IY = Rect.Min.Y + (Sz.Y - It.IconSz) * 0.5f;
			DrawRect(FLinearColor(0.012f, 0.013f, 0.018f, 0.94f), IX - 2.0f, IY - 2.0f, It.IconSz + 4.0f, It.IconSz + 4.0f);
			if (UTexture2D* LogoTex = GetClassLogoTexture(It.Unit->GetArchetype(), It.bAlly))
			{
				if (LogoTex->GetResource())
				{
					FCanvasTileItem Tile(FVector2D(IX, IY), LogoTex->GetResource(), FVector2D(It.IconSz, It.IconSz), FLinearColor::White);
					Tile.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(Tile);
				}
			}

			// 右侧两行: 部队名称 + 兵员数量 + 贴底细血条
			const float TextX = IX + It.IconSz + 2.0f + IconGap;
			const float TextBlockH = It.NameH + It.SubH + 3.0f;
			const float TextTopY = Rect.Min.Y + (Sz.Y - 5.0f - TextBlockH) * 0.5f;
			DrawText(It.Name, ERUI::InkText(It.bAlly ? 1.0f : 0.88f), TextX, TextTopY, Font, 1.32f, false);
			DrawText(It.Sub, ERUI::InkText(0.68f), TextX, TextTopY + It.NameH + 3.0f, Font, 1.02f, false);

			const float HPRatio = FMath::Clamp(It.Unit->GetLegionHP() / FMath::Max(1.0f, It.Unit->GetLegionMaxHP()), 0.0f, 1.0f);
			const float BarX = TextX;
			const float BarWMax = FMath::Max(16.0f, Rect.Max.X - PadX - BarX);
			DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.22f), BarX, Rect.Max.Y - 6.0f, BarWMax, 3.0f);
			DrawRect(It.MarkCol, BarX, Rect.Max.Y - 6.0f, BarWMax * HPRatio, 3.0f);

			StrategicBadgeRects.Add({ It.Unit, Rect });
		}
	}

	// 右上角「返回战场」按钮 + 缩放读数 (右对齐, 彻底让出地图顶部中央视野)
	{
		const FBox2D BR = GetMapReturnButtonRect(SX, SY);
		float MX = 0.0f, MY = 0.0f;
		const bool bHover = PC->GetMousePosition(MX, MY) && BR.IsInside(FVector2D(MX, MY));
		const FVector2D Sz = BR.GetSize();
		DrawBonePlate(BR.Min.X, BR.Min.Y, Sz.X, Sz.Y, T->Accent, true, false, bHover);

		const FString Txt = TEXT("返回战场");
		const float BtnScale = 1.85f;
		float TW = 0.0f, TH = 0.0f;
		Canvas->TextSize(Font, Txt, TW, TH, BtnScale, BtnScale);
		DrawText(Txt, ERUI::InkText(),
			BR.Min.X + 4.0f + (Sz.X - 4.0f - TW) * 0.5f, BR.Min.Y + (Sz.Y - TH) * 0.5f, Font, BtnScale, false);

		const bool bPaused = GM->GetTacticalTimeMode() == ELegionTimeMode::Paused;
		const FString ZoomTxt = FString::Printf(TEXT("缩放 %.1f 倍  ·  %s"), View.Zoom,
			bPaused ? TEXT("战场已暂停 · 命令在返回战场后执行") : TEXT("兵团正在执行命令"));
		float ZW = 0.0f, ZH = 0.0f;
		Canvas->TextSize(Font, ZoomTxt, ZW, ZH, 1.05f, 1.05f);
		DrawText(ZoomTxt, T->BoneSecondary, BR.Max.X - ZW, BR.Max.Y + 8.0f, Font, 1.05f, false);
	}
}

void ALegionHUD::DrawBonePlate(float X, float Y, float W, float H, const FLinearColor& Accent,
	bool bPrism, bool bEmphasis, bool bHover)
{
	const UERUITheme* T = ERUI::Theme();
	DrawRect(bHover ? FLinearColor(1.0f, 1.0f, 0.98f, 1.0f) : FLinearColor(0.93f, 0.93f, 0.91f, 0.96f), X, Y, W, H);

	const float SW = 4.0f;
	if (bPrism)
	{
		const float S3 = H / 3.0f;
		DrawRect(T->PrismStart, X, Y, SW, S3 + 1.0f);
		DrawRect(T->PrismMid, X, Y + S3, SW, S3 + 1.0f);
		DrawRect(T->PrismEnd, X, Y + 2.0f * S3, SW, H - 2.0f * S3);
	}
	else
	{
		DrawRect(Accent, X, Y, SW, H);
	}

	const FLinearColor Ink(0.02f, 0.02f, 0.03f, bEmphasis ? 0.92f : (bHover ? 0.8f : 0.40f));
	const float Th = bEmphasis ? 1.8f : 1.0f;
	DrawLine(X, Y, X + W, Y, Ink, Th);
	DrawLine(X + W, Y, X + W, Y + H, Ink, Th);
	DrawLine(X + W, Y + H, X, Y + H, Ink, Th);
	DrawLine(X, Y + H, X, Y, Ink, Th);
}

void ALegionHUD::DrawProjectedLine(const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness)
{
	const FVector PA = Canvas->Project(A, false);
	const FVector PB = Canvas->Project(B, false);
	if (PA.Z <= 0.0f || PB.Z <= 0.0f) { return; }
	DrawLine(PA.X, PA.Y, PB.X, PB.Y, Color, Thickness);
}

void ALegionHUD::DrawClippedWorldLine(const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas) { return; }

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector CamFwd = CamRot.Vector();

	constexpr float NearDist = 30.0f;
	const float DepthA = FVector::DotProduct(A - CamLoc, CamFwd);
	const float DepthB = FVector::DotProduct(B - CamLoc, CamFwd);
	if (DepthA < NearDist && DepthB < NearDist) { return; }

	FVector CA = A;
	FVector CB = B;
	if (DepthA < NearDist)
	{
		const float Alpha = (NearDist - DepthA) / FMath::Max(KINDA_SMALL_NUMBER, DepthB - DepthA);
		CA = FMath::Lerp(A, B, Alpha);
	}
	else if (DepthB < NearDist)
	{
		const float Alpha = (NearDist - DepthB) / FMath::Max(KINDA_SMALL_NUMBER, DepthA - DepthB);
		CB = FMath::Lerp(B, A, Alpha);
	}

	const FVector PA = Canvas->Project(CA, false);
	const FVector PB = Canvas->Project(CB, false);
	if (PA.Z <= 0.0f || PB.Z <= 0.0f) { return; }
	DrawLine(PA.X, PA.Y, PB.X, PB.Y, Color, Thickness);
}

void ALegionHUD::DrawEnemySkyBanner(ALegionUnit* Unit, ALegionPlayerController* PC, bool bHovered, bool bFocusedTarget)
{
	if (!Canvas || !Unit || !PC || Unit->IsWiped()) { return; }

	const UERUITheme* T = ERUI::Theme();
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector BaseWorld = Unit->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
	const FVector SkyWorld = Unit->GetSkyBannerWorldLocation();
	const FVector ZenithWorld = SkyWorld + FVector(0.0f, 0.0f, 2400.0f);

	const FLinearColor AccentCol = bHovered
		? FLinearColor(1.0f, 0.28f, 0.16f, 1.0f)
		: (bFocusedTarget ? FLinearColor(0.98f, 0.22f, 0.14f, 1.0f) : T->EnemyMark);
	const FLinearColor CoreCol = bHovered
		? FLinearColor(1.0f, 0.88f, 0.78f, 1.0f)
		: FLinearColor(0.98f, 0.42f, 0.34f, 1.0f);

	// 1. 直插天空的连续通天立柱竖线 (整段近平面裁剪, 绝对平滑无分段断口)
	{
		const float Boost = bHovered ? 1.25f : (bFocusedTarget ? 1.12f : 1.0f);
		const FVector MidWorld = FMath::Lerp(BaseWorld, SkyWorld, 0.52f);

		// 柔和外光晕 (整段 + 上半段叠亮)
		FLinearColor GlowLow = AccentCol;
		GlowLow.A = FMath::Clamp(0.20f * Boost, 0.0f, 0.55f);
		DrawClippedWorldLine(BaseWorld, SkyWorld, GlowLow, bHovered ? 7.0f : 4.6f);

		FLinearColor GlowHigh = AccentCol;
		GlowHigh.A = FMath::Clamp(0.18f * Boost, 0.0f, 0.50f);
		DrawClippedWorldLine(MidWorld, SkyWorld, GlowHigh, bHovered ? 5.0f : 3.2f);

		// 锐利主芯线 (整段连续, 零断缝)
		FLinearColor CoreMain = CoreCol;
		CoreMain.A = FMath::Clamp(0.82f * Boost, 0.0f, 1.0f);
		DrawClippedWorldLine(BaseWorld, SkyWorld, CoreMain, bHovered ? 2.4f : 1.6f);

		// Logo 上方继续向天穹延伸的细尖峰线 ("直插天空" 的庄严纵深感)
		const FVector SpireMid = FMath::Lerp(SkyWorld, ZenithWorld, 0.45f);
		FLinearColor SpireLow = CoreCol;
		SpireLow.A = bHovered ? 0.65f : 0.40f;
		DrawClippedWorldLine(SkyWorld, SpireMid, SpireLow, bHovered ? 1.6f : 1.15f);

		FLinearColor SpireTip = CoreCol;
		SpireTip.A = bHovered ? 0.25f : 0.14f;
		DrawClippedWorldLine(SpireMid, ZenithWorld, SpireTip, 1.0f);
	}

	// 地面起点的极简菱形锚点 (无文字、不挡视线, 仅标明立柱源自哪支敌团)
	{
		const FVector PBase = Canvas->Project(BaseWorld, false);
		if (PBase.Z > 0.0f && PBase.X >= -40.0f && PBase.X <= Canvas->SizeX + 40.0f
			&& PBase.Y >= -40.0f && PBase.Y <= Canvas->SizeY + 40.0f)
		{
			const float R = bHovered ? 7.0f : 5.0f;
			FLinearColor AnchorCol = AccentCol;
			AnchorCol.A = bHovered ? 0.95f : 0.72f;
			DrawLine(PBase.X, PBase.Y - R, PBase.X + R, PBase.Y, AnchorCol, 1.6f);
			DrawLine(PBase.X + R, PBase.Y, PBase.X, PBase.Y + R, AnchorCol, 1.6f);
			DrawLine(PBase.X, PBase.Y + R, PBase.X - R, PBase.Y, AnchorCol, 1.6f);
			DrawLine(PBase.X - R, PBase.Y, PBase.X, PBase.Y - R, AnchorCol, 1.6f);
		}
	}

	// 2. 天空高处的部队 Logo 与番号铭牌
	const FVector Proj = Canvas->Project(SkyWorld, false);
	if (Proj.Z <= 0.0f) { return; }
	if (Proj.X < -140.0f || Proj.X > Canvas->SizeX + 140.0f || Proj.Y < -140.0f || Proj.Y > Canvas->SizeY + 140.0f)
	{
		return;
	}

	const float Dist = FVector::Dist(CamLoc, SkyWorld);
	float IconSize = FMath::Clamp(680000.0f / FMath::Max(2500.0f, Dist), 52.0f, 84.0f);
	if (bHovered) { IconSize *= 1.14f; }

	const float Pad = 6.0f;
	const float BarW = 4.0f;
	const float StripH = 26.0f;
	const float LogoAreaH = IconSize + Pad * 2.0f;
	const FString Name = Unit->GetCallsign().IsEmpty() ? ERGalacticMeta::GetEnemyBrigadeName(1) : Unit->GetCallsign();
	const FString Label = FString::Printf(TEXT("%s · %d/%d"), *Name, Unit->GetAliveCount(), Unit->GetSoldierCount());
	float LabelW = 0.0f, LabelH = 0.0f;
	Canvas->TextSize(Font, Label, LabelW, LabelH, 1.0f, 1.0f);

	const float BoxW = FMath::Max(IconSize + Pad * 2.0f + BarW + 4.0f, LabelW + BarW + 18.0f);
	const float TotalH = LogoAreaH + StripH;
	const float X = Proj.X - BoxW * 0.5f;
	const float Y = Proj.Y - LogoAreaH * 0.5f;

	// 立柱下方几道精致的刻度短横线 (机能光学标尺感)
	for (int32 k = 1; k <= 2; ++k)
	{
		const float TickY = Y + TotalH + k * 12.0f;
		const float HalfTick = (k == 1 ? 8.0f : 5.0f);
		FLinearColor TickCol = CoreCol;
		TickCol.A = bHovered ? 0.85f : 0.45f;
		DrawLine(Proj.X - HalfTick, TickY, Proj.X + HalfTick, TickY, TickCol, 1.2f);
	}

	// 白底墨字彩条外框 + 左侧红条
	DrawBonePlate(X, Y, BoxW, TotalH, AccentCol, false, bHovered || bFocusedTarget, bHovered);

	// Logo 区域暗曜石内框 (在亮蓝天空与白云背景下确保红色部队图标高对比锐利)
	const float InnerX = X + BarW + (BoxW - BarW - IconSize) * 0.5f;
	const float InnerY = Y + Pad;
	DrawRect(FLinearColor(0.012f, 0.013f, 0.018f, 0.92f), InnerX - 2.0f, InnerY - 2.0f, IconSize + 4.0f, IconSize + 4.0f);

	if (UTexture2D* LogoTex = GetClassLogoTexture(Unit->GetArchetype(), false))
	{
		if (LogoTex->GetResource())
		{
			FCanvasTileItem Tile(FVector2D(InnerX, InnerY), LogoTex->GetResource(),
				FVector2D(IconSize, IconSize), FLinearColor::White);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
	}

	// 底部番号与存活兵力 + 血条
	const float TextX = X + BarW + (BoxW - BarW - LabelW) * 0.5f;
	const float TextY = Y + LogoAreaH + 2.0f;
	DrawText(Label, ERUI::InkText(), TextX, TextY, Font, 1.0f, false);

	const float HPRatio = FMath::Clamp(Unit->GetLegionHP() / FMath::Max(1.0f, Unit->GetLegionMaxHP()), 0.0f, 1.0f);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.22f), X + BarW + 4.0f, Y + TotalH - 5.0f, BoxW - BarW - 8.0f, 3.0f);
	DrawRect(AccentCol, X + BarW + 4.0f, Y + TotalH - 5.0f, (BoxW - BarW - 8.0f) * HPRatio, 3.0f);

	// 瞄准悬停 / 集火锁定光学角括弧
	if (bHovered || bFocusedTarget)
	{
		const float Outset = bHovered ? 7.0f : 4.0f;
		const float Arm = bHovered ? 14.0f : 10.0f;
		const float Th = bHovered ? 2.5f : 1.8f;
		const FLinearColor LockCol = bHovered ? T->Accent : FLinearColor(1.0f, 0.22f, 0.14f, 0.96f);
		const float L = X - Outset;
		const float R = X + BoxW + Outset;
		const float Top = Y - Outset;
		const float Bot = Y + TotalH + Outset;

		DrawLine(L, Top, L + Arm, Top, LockCol, Th);
		DrawLine(L, Top, L, Top + Arm, LockCol, Th);
		DrawLine(R, Top, R - Arm, Top, LockCol, Th);
		DrawLine(R, Top, R, Top + Arm, LockCol, Th);
		DrawLine(L, Bot, L + Arm, Bot, LockCol, Th);
		DrawLine(L, Bot, L, Bot - Arm, LockCol, Th);
		DrawLine(R, Bot, R - Arm, Bot, LockCol, Th);
		DrawLine(R, Bot, R, Bot - Arm, LockCol, Th);
	}

	// 顶部集火状态标 (配暗曜石微底衬以在亮天白云下保持锐利)
	if (bFocusedTarget)
	{
		const FString Tag = TEXT("集火目标");
		float TagW = 0.0f, TagH = 0.0f;
		Canvas->TextSize(Font, Tag, TagW, TagH, 0.95f, 0.95f);
		const float PW = TagW + 16.0f;
		const float PH = TagH + 6.0f;
		const float PX = Proj.X - PW * 0.5f;
		const float PY = Y - PH - (bHovered ? 12.0f : 8.0f);
		DrawRect(FLinearColor(0.012f, 0.014f, 0.020f, 0.88f), PX, PY, PW, PH);
		DrawRect(FLinearColor(1.0f, 0.24f, 0.14f, 1.0f), PX, PY, 3.0f, PH);
		DrawText(Tag, T->Bone, PX + 9.0f, PY + 3.0f, Font, 0.95f, false);
	}

	// 记录可点击/可瞄准命中区域 (适度留边, 抬头右键锁定手感顺畅)
	constexpr float HitPad = 16.0f;
	FEnemySkyBannerEntry Entry;
	Entry.Unit = Unit;
	Entry.HitRect = FBox2D(FVector2D(X - HitPad, Y - HitPad), FVector2D(X + BoxW + HitPad, Y + TotalH + HitPad));
	Entry.Center = FVector2D(Proj.X, Y + TotalH * 0.5f);
	Entry.DistSq = Dist * Dist;
	EnemySkyBannerEntries.Add(Entry);
}

void ALegionHUD::DrawDropZoneWarning(const FVector& WorldPos, const FString& Label, float RemainingSec)
{
	const FVector P = Canvas->Project(WorldPos + FVector(0, 0, 180.0f), false);
	if (P.Z <= 0.0f) { return; }

	const float R = 18.0f;
	const FLinearColor Red(0.92f, 0.14f, 0.10f, 0.92f);
	DrawLine(P.X, P.Y - R, P.X + R, P.Y, Red, 2.0f);
	DrawLine(P.X + R, P.Y, P.X, P.Y + R, Red, 2.0f);
	DrawLine(P.X, P.Y + R, P.X - R, P.Y, Red, 2.0f);
	DrawLine(P.X - R, P.Y, P.X, P.Y - R, Red, 2.0f);

	const FString Txt = FString::Printf(TEXT("%s  %.0f 秒后"), *Label, RemainingSec);
	DrawText(Txt, Red, P.X - 42.0f, P.Y - 36.0f, nullptr, 0.9f, false);
}

void ALegionHUD::DrawUnitBadge(ALegionUnit* Unit, bool bSelected)
{
	const UERUITheme* T = ERUI::Theme();
	const bool bAlly = (Unit->GetTeam() == ELegionTeam::Player);
	const FLinearColor MarkCol = bSelected ? T->Accent : (bAlly ? T->AllyMark : T->EnemyMark);

	// 地面投影细环与朝向角标 (贴合自然散阵, 取代生硬矩形盒)
	{
		const FVector Center = Unit->GetActorLocation() + FVector(0.0f, 0.0f, 28.0f);
		const FVector Fwd = Unit->GetFacingVector();
		const FVector Right(-Fwd.Y, Fwd.X, 0.0f);
		const float RingR = FMath::Max(260.0f, (Unit->GetFormationHalfExtent().X + Unit->GetFormationHalfExtent().Y) * 0.45f);

		// 16 段轻细战术地面圈
		FLinearColor RingCol = MarkCol;
		RingCol.A = bSelected ? 0.72f : 0.24f;
		constexpr int32 Segs = 16;
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = ((float)s / Segs) * 2.0f * PI;
			const float A1 = ((float)(s + 1) / Segs) * 2.0f * PI;
			const FVector P0 = Center + FVector(FMath::Cos(A0) * RingR, FMath::Sin(A0) * RingR, 0.0f);
			const FVector P1 = Center + FVector(FMath::Cos(A1) * RingR, FMath::Sin(A1) * RingR, 0.0f);
			DrawProjectedLine(P0, P1, RingCol, bSelected ? 1.8f : 1.0f);
		}

		// 前锋朝向小角标
		const float TipDist = RingR + 120.0f;
		const FVector Tip = Center + Fwd * TipDist;
		const FVector WingL = Center + Fwd * (TipDist - 110.0f) - Right * 150.0f;
		const FVector WingR = Center + Fwd * (TipDist - 110.0f) + Right * 150.0f;
		FLinearColor ChevronCol = MarkCol;
		ChevronCol.A = bSelected ? 0.95f : 0.55f;
		DrawProjectedLine(WingL, Tip, ChevronCol, bSelected ? 2.4f : 1.5f);
		DrawProjectedLine(WingR, Tip, ChevronCol, bSelected ? 2.4f : 1.5f);
	}

	// 敌军不再在头顶显示番号牌 (改由 DrawEnemySkyBanner 在天空高处直插显示部队 Logo)
	if (!bAlly) { return; }

	const FVector Proj = Canvas->Project(Unit->GetActorLocation() + FVector(0.0f, 0.0f, 340.0f), false);
	if (Proj.Z <= 0.0f) { return; }
	if (Proj.X < -80.0f || Proj.X > Canvas->SizeX + 80.0f || Proj.Y < -80.0f || Proj.Y > Canvas->SizeY + 80.0f)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const FString Name = Unit->GetCallsign().IsEmpty() ? ERGalacticMeta::GetBrigadeName(1) : Unit->GetCallsign();
	const FString Label = FString::Printf(TEXT("%s · %d/%d"), *Name, Unit->GetAliveCount(), Unit->GetSoldierCount());
	float TW = 0.0f, TH = 0.0f;
	Canvas->TextSize(Font, Label, TW, TH, 1.0f, 1.0f);

	const float BarW = 4.0f;
	const float IconSz = bSelected ? 24.0f : 22.0f;
	const float W = BarW + 8.0f + IconSz + 7.0f + TW + 10.0f;
	const float H = bSelected ? 36.0f : 34.0f;
	const float X = Proj.X - W * 0.5f;
	const float Y = Proj.Y - H;

	// 白底墨字彩条底板 + 左侧阵营色条 + 选中加粗描边
	DrawBonePlate(X, Y, W, H, MarkCol, false, bSelected, false);

	// 我方部队占位 Logo (蓝色技能图标 + 暗曜石底框)
	const float IX = X + BarW + 6.0f;
	const float IY = Y + (H - 5.0f - IconSz) * 0.5f;
	DrawRect(FLinearColor(0.012f, 0.013f, 0.018f, 0.92f), IX - 1.0f, IY - 1.0f, IconSz + 2.0f, IconSz + 2.0f);
	if (UTexture2D* LogoTex = GetClassLogoTexture(Unit->GetArchetype(), true))
	{
		if (LogoTex->GetResource())
		{
			FCanvasTileItem Tile(FVector2D(IX, IY), LogoTex->GetResource(), FVector2D(IconSz, IconSz), FLinearColor::White);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
	}

	// 血条/兵力比例条 (贴底 3px, 墨色底 + 阵营色填充)
	const float HPRatio = FMath::Clamp(Unit->GetLegionHP() / FMath::Max(1.0f, Unit->GetLegionMaxHP()), 0.0f, 1.0f);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.22f), X + BarW + 4.0f, Y + H - 5.0f, W - BarW - 8.0f, 3.0f);
	DrawRect(MarkCol, X + BarW + 4.0f, Y + H - 5.0f, (W - BarW - 8.0f) * HPRatio, 3.0f);

	// 部队名称 + 存活人数
	DrawText(Label, ERUI::InkText(), IX + IconSz + 7.0f, Y + (H - 5.0f - TH) * 0.5f, Font, 1.0f, false);
}
