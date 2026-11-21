// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionHUD.h"

#include "Blueprint/UserWidget.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
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
		const FString Label = FString::Printf(TEXT("DROP %s x%d"),
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

	// 6. 左上角黑白等高线小地图 (蓝点=我方, 红点=敌方)
	DrawMinimap(GM, PC);
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

	DrawText(TEXT("TOPO // MINIMAP"), FLinearColor(0.92f, 0.92f, 0.90f, 0.65f),
		MX + 6.0f, MY + MS - 16.0f, nullptr, 0.78f, false);
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

	const FString Txt = FString::Printf(TEXT("%s [%.0fs]"), *Label, RemainingSec);
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

	const float W = bSelected ? 92.0f : 78.0f;
	const float H = 26.0f;
	const float X = Proj.X - W * 0.5f;
	const float Y = Proj.Y - H;

	// 黑曜石底板 + 左侧阵营色条 + 选中金框
	DrawRect(FLinearColor(0.01f, 0.012f, 0.018f, 0.86f), X, Y, W, H);
	DrawRect(MarkCol, X, Y, 3.5f, H);
	if (bSelected)
	{
		DrawLine(X, Y, X + W, Y, T->Accent, 1.5f);
		DrawLine(X + W, Y, X + W, Y + H, T->Accent, 1.5f);
		DrawLine(X + W, Y + H, X, Y + H, T->Accent, 1.5f);
		DrawLine(X, Y + H, X, Y, T->Accent, 1.5f);
	}

	// 血条/兵力比例条 (贴底 3px)
	const float HPRatio = FMath::Clamp(Unit->GetLegionHP() / FMath::Max(1.0f, Unit->GetLegionMaxHP()), 0.0f, 1.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.14f), X + 4.0f, Y + H - 4.0f, W - 5.0f, 3.0f);
	DrawRect(MarkCol, X + 4.0f, Y + H - 4.0f, (W - 5.0f) * HPRatio, 3.0f);

	// 兵种代号 + 存活人数
	const FString Code = ERGalacticMeta::GetClassCode(Unit->GetArchetype());
	const FString Label = FString::Printf(TEXT("%s  %d/%d"), *Code, Unit->GetAliveCount(), Unit->GetSoldierCount());
	DrawText(Label, T->Bone, X + 8.0f, Y + 4.0f, nullptr, 0.92f, false);

	// 若正在释放主动协议技, 在上方显示金色高亮标
	if (Unit->IsSkillActive())
	{
		DrawText(ERGalacticMeta::GetClassSkillName(Unit->GetArchetype()),
			T->Accent, X + 4.0f, Y - 16.0f, nullptr, 0.85f, false);
	}
}
