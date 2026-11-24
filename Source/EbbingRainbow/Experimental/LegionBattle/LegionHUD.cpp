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
	if (APlayerController* PC = GetOwningPlayerController())
	{
		Widget = CreateWidget<ULegionHUDWidget>(PC, ULegionHUDWidget::StaticClass());
		if (Widget)
		{
			Widget->AddToViewport(0);
		}
	}
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
		DrawStrategicMap(GM, PC);
		return;
	}

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

	// 4. 双方兵团屏幕投影战术旗标 (解决大地形拉远看不清单位的问题)
	for (ALegionUnit* L : GM->GetPlayerLegions())
	{
		if (L && !L->IsWiped())
		{
			DrawUnitBadge(L, PC->IsLegionSelected(L));
		}
	}
	for (ALegionUnit* L : GM->GetEnemyLegions())
	{
		if (L && !L->IsWiped())
		{
			DrawUnitBadge(L, false);
		}
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
		const FString Label = FString::Printf(TEXT("%s  %d/%d"), *Aim->GetCallsign(), Aim->GetAliveCount(), Aim->GetSoldierCount());
		DrawText(Label, Col, CX - 70.0f, CY + 28.0f, nullptr, 0.9f, false);
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

	DrawText(TEXT("按 M 或按住 Alt 点击展开"), FLinearColor(0.92f, 0.92f, 0.90f, 0.78f),
		MX + 6.0f, MY + MS - 16.0f, nullptr, 0.78f, false);
}

FBox2D ALegionHUD::GetMapReturnButtonRect(int32 ViewportSizeX, int32 ViewportSizeY)
{
	const float Scale = FMath::Max(0.75f, (float)ViewportSizeY / 1080.0f);
	const float W = 240.0f * Scale;
	const float H = 44.0f * Scale;
	const float X = (float)ViewportSizeX * 0.5f - W * 0.5f;
	const float Y = 18.0f * Scale;
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

				FString Tag = ERGalacticMeta::GetClassCode(L->GetArchetype());
				DrawText(Tag, C, Last.X + R + 4.0f, Last.Y - 18.0f, Font, 0.85f, false);
			}
		}
	}

	// 番号牌 (敌方先画, 我方后画上叠层; 选中金框; 重叠时错开并引回真实位置)
	TArray<FBox2D> PlacedRects;
	const FBox2D Viewport(FVector2D(4.0f, 72.0f), FVector2D((float)SX - 4.0f, (float)SY - 4.0f));
	auto DrawBadge = [&](ALegionUnit* L)
	{
		FVector2D Center;
		if (!ToScreen(L->GetActorLocation(), Center) || !OnScreen(Center, 60.0f)) { return; }

		const bool bAlly = (L->GetTeam() == ELegionTeam::Player);
		const bool bSel = PC->IsLegionSelected(L);
		const FLinearColor MarkCol = bSel ? T->Accent : (bAlly ? T->AllyMark : T->EnemyMark);

		FString Call = L->GetCallsign();
		int32 Sep = INDEX_NONE;
		if (Call.FindChar(TEXT('/'), Sep)) { Call = Call.Left(Sep).TrimStartAndEnd(); }
		if (Call.IsEmpty()) { Call = ERGalacticMeta::GetClassCode(L->GetArchetype()); }

		FString Label = FString::Printf(TEXT("%s · %s · %d 兵"),
			*ERGalacticMeta::GetClassCode(L->GetArchetype()), *Call, L->GetAliveCount());
		if (bAlly && PC->IsLegionFollowing(L)) { Label += TEXT(" · 跟随中"); }

		float TW = 0.0f, TH = 0.0f;
		Canvas->TextSize(Font, Label, TW, TH, 1.3f, 1.3f);

		const float BarW = 4.0f, PadX = 10.0f, PadY = 6.0f;
		const FVector2D BSize(BarW + PadX * 2.0f + TW, PadY * 2.0f + TH);
		const float StepX = BSize.X + 8.0f;
		const float StepY = BSize.Y + 5.0f;

		// 候选错开位 (原位 -> 上下 -> 更远 + 左右), 取第一个不与已放置牌碰撞的位置
		const FVector2D Offsets[] = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, -StepY), FVector2D(0.0f, StepY),
			FVector2D(0.0f, -2.0f * StepY), FVector2D(0.0f, 2.0f * StepY),
			FVector2D(-StepX, 0.0f), FVector2D(StepX, 0.0f),
			FVector2D(-StepX, -StepY), FVector2D(StepX, -StepY),
			FVector2D(-StepX, StepY), FVector2D(StepX, StepY),
		};
		FBox2D Chosen(ForceInit);
		for (const FVector2D& Off : Offsets)
		{
			FVector2D BMin = Center + Off - BSize * 0.5f;
			BMin.X = FMath::Clamp(BMin.X, Viewport.Min.X, FMath::Max(Viewport.Min.X, Viewport.Max.X - BSize.X));
			BMin.Y = FMath::Clamp(BMin.Y, Viewport.Min.Y, FMath::Max(Viewport.Min.Y, Viewport.Max.Y - BSize.Y));
			const FBox2D Cand(BMin, BMin + BSize);
			bool bHit = false;
			for (const FBox2D& R : PlacedRects)
			{
				if (Cand.Intersect(R)) { bHit = true; break; }
			}
			if (!bHit) { Chosen = Cand; break; }
		}
		if (!Chosen.bIsValid)
		{
			const FVector2D BMin = Center - BSize * 0.5f;
			Chosen = FBox2D(BMin, BMin + BSize);
		}
		PlacedRects.Add(Chosen);

		const FVector2D BCenter = Chosen.GetCenter();
		DrawRect(MarkCol, Center.X - 3.0f, Center.Y - 3.0f, 6.0f, 6.0f);
		if ((BCenter - Center).Size() > 6.0f)
		{
			FLinearColor LeadCol = MarkCol;
			LeadCol.A = 0.5f;
			DrawLine(BCenter.X, BCenter.Y, Center.X, Center.Y, LeadCol, 1.0f);
		}

		const FVector2D Sz = Chosen.GetSize();
		DrawBonePlate(Chosen.Min.X, Chosen.Min.Y, Sz.X, Sz.Y, MarkCol, false, bSel, false);
		DrawText(Label, ERUI::InkText(bAlly ? 1.0f : 0.78f),
			Chosen.Min.X + BarW + PadX, Chosen.Min.Y + PadY - 1.0f, Font, 1.3f, false);

		StrategicBadgeRects.Add({ L, Chosen });
	};

	for (ALegionUnit* E : GM->GetEnemyLegions())
	{
		if (E && !E->IsWiped()) { DrawBadge(E); }
	}
	for (ALegionUnit* P : GM->GetPlayerLegions())
	{
		if (P && !P->IsWiped()) { DrawBadge(P); }
	}

	// 顶部「返回战场」按钮 + 缩放读数
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
			bPaused ? TEXT("战场已暂停 · 命令在返回战场后执行 · E 键切换 0.2 倍观战") : TEXT("0.2 倍时缓中 · 兵团正在执行命令 · E 键暂停"));
		float ZW = 0.0f, ZH = 0.0f;
		Canvas->TextSize(Font, ZoomTxt, ZW, ZH, 1.05f, 1.05f);
		DrawText(ZoomTxt, T->BoneSecondary, (float)SX * 0.5f - ZW * 0.5f, BR.Max.Y + 6.0f, Font, 1.05f, false);
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

	const FVector Proj = Canvas->Project(Unit->GetActorLocation() + FVector(0.0f, 0.0f, 340.0f), false);
	if (Proj.Z <= 0.0f) { return; }
	if (Proj.X < -80.0f || Proj.X > Canvas->SizeX + 80.0f || Proj.Y < -80.0f || Proj.Y > Canvas->SizeY + 80.0f)
	{
		return;
	}

	const float W = bSelected ? 104.0f : 94.0f;
	const float H = 30.0f;
	const float X = Proj.X - W * 0.5f;
	const float Y = Proj.Y - H;

	// 白底墨字彩条底板 + 左侧阵营色条 + 选中加粗描边
	DrawBonePlate(X, Y, W, H, MarkCol, false, bSelected, false);

	// 血条/兵力比例条 (贴底 3px, 墨色底 + 阵营色填充)
	const float HPRatio = FMath::Clamp(Unit->GetLegionHP() / FMath::Max(1.0f, Unit->GetLegionMaxHP()), 0.0f, 1.0f);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.22f), X + 6.0f, Y + H - 5.0f, W - 8.0f, 3.0f);
	DrawRect(MarkCol, X + 6.0f, Y + H - 5.0f, (W - 8.0f) * HPRatio, 3.0f);

	// 兵种简称 + 存活人数
	const FString Code = ERGalacticMeta::GetClassCode(Unit->GetArchetype());
	const FString Label = FString::Printf(TEXT("%s  %d/%d"), *Code, Unit->GetAliveCount(), Unit->GetSoldierCount());
	DrawText(Label, ERUI::InkText(), X + 10.0f, Y + 3.0f, nullptr, 1.0f, false);

	// 若正在释放主动协议技, 在上方显示金色高亮标
	if (Unit->IsSkillActive())
	{
		DrawText(ERGalacticMeta::GetClassSkillName(Unit->GetArchetype()),
			T->Accent, X + 4.0f, Y - 16.0f, nullptr, 0.85f, false);
	}
}
