// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "ERTechPrimitives.generated.h"

class SERTechFrameLeaf;
class SERTechDividerLeaf;
class SERPrismBarLeaf;
class SERCornerReticleLeaf;
class SERControlSharePieLeaf;

/** 切角机能面板的点睛色条位置 */
UENUM(BlueprintType)
enum class EERAccentEdge : uint8
{
	None   UMETA(DisplayName = "无"),
	Left   UMETA(DisplayName = "左侧竖条"),
	Top    UMETA(DisplayName = "顶部横条"),
	Bottom UMETA(DisplayName = "底部横条"),
	Right  UMETA(DisplayName = "右侧竖条")
};

/**
 * 机能风切角建筑底板 (UMG Palette: "ER UI|Tech"):
 *  - 纯 C++ Slate GPU 三角扇填充真 45° 切角多边形 + 1px 锐利轮廓线 (零模糊九切片)
 *  - 可选四角精密对准 L 括号 ([ ])、单边点睛色条/虹彩窄条、对角线微扫描高光
 *  - 内置动效状态 (RevealAlpha / HoverAlpha / SweepAlpha), 由 C++ 弹簧或蓝图直接驱动
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Tech Frame"))
class EBBINGRAINBOW_API UERTechFrame : public UWidget
{
	GENERATED_BODY()

public:
	/** 四角 45° 切角像素 (TL, TR, BR, BL) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	FVector4 Chamfer = FVector4(12.0f, 0.0f, 12.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	FLinearColor FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	FLinearColor BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	float BorderThickness = 1.0f;

	/** 是否绘制四角 L 形对准刻度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	bool bDrawCornerBrackets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame", meta = (EditCondition = "bDrawCornerBrackets"))
	float BracketLength = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame", meta = (EditCondition = "bDrawCornerBrackets"))
	float BracketThickness = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame", meta = (EditCondition = "bDrawCornerBrackets"))
	FLinearColor BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.72f);

	/** 点睛色边位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	EERAccentEdge AccentEdge = EERAccentEdge::None;

	/** true 时该边绘制《虹不我与》三段窄光谱条, false 时使用单点睛色 AccentColor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	bool bUsePrismAccent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	float AccentThickness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	FLinearColor AccentColor = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);

	/** 顶部/边缘极淡内高光 (增加材质厚度感) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	bool bTopHairlineSheen = true;

	/** 展开进度 0..1 (驱动面板水平/纵向锐利展开) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RevealAlpha = 1.0f;

	/** 悬停/聚焦强度 0..1 (提亮边线与角标) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoverAlpha = 0.0f;

	/** 扫描光进度 0..1 (0 或 1 时隐藏, 中间值划过一道极克制斜向高光) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SweepAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechFrame")
	FVector2D DesiredFrameSize = FVector2D(64.0f, 32.0f);

	UFUNCTION(BlueprintCallable, Category = "ERUI|TechFrame")
	void SetFrameColors(FLinearColor InFill, FLinearColor InBorder, FLinearColor InBracket, FLinearColor InAccent);

	UFUNCTION(BlueprintCallable, Category = "ERUI|TechFrame")
	void SetMotionState(float InRevealAlpha, float InHoverAlpha, float InSweepAlpha);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Tech"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SERTechFrameLeaf> MyLeaf;
};

/**
 * 机能风建筑分割线 (UMG Palette: "ER UI|Tech"):
 * 起点实心锚块 + 1px 发丝线 + 末端微菱形, 支持 RevealAlpha 从左向右划出动画。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Tech Divider"))
class EBBINGRAINBOW_API UERTechDivider : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechDivider")
	FLinearColor LineColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechDivider")
	FLinearColor AnchorColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechDivider")
	float AnchorLength = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechDivider")
	float LineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|TechDivider")
	bool bDrawEndDiamond = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RevealAlpha = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "ERUI|TechDivider")
	void SetRevealAlpha(float InAlpha);

	UFUNCTION(BlueprintCallable, Category = "ERUI|TechDivider")
	void SetDividerColors(FLinearColor InLine, FLinearColor InAnchor);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Tech"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SERTechDividerLeaf> MyLeaf;
};

/**
 * 《虹不我与》招牌窄光谱条 (UMG Palette: "ER UI|Tech"):
 * 黑白主调中唯一一缕克制的三段折射彩光 (冷青 -> 折射暖金 -> 控制朱红),
 * 纯 GPU 顶点色插值, 零贴图依赖, 支持纵/横摆放与静谧微呼吸。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Prism Bar"))
class EBBINGRAINBOW_API UERPrismBar : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PrismBar")
	bool bVertical = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PrismBar")
	FVector2D BarSize = FVector2D(3.0f, 44.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PrismBar")
	bool bAnimateShimmer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RevealAlpha = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "ERUI|PrismBar")
	void SetRevealAlpha(float InAlpha);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Tech"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SERPrismBarLeaf> MyLeaf;
};

/**
 * 全屏/画框静谧建筑四角标尺 (UMG Palette: "ER UI|Tech"):
 * 在视口四角绘制极细 L 形光学对准角标与微小遥测编号, 95%+ 画面保持绝对通透留白。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Corner Reticle"))
class EBBINGRAINBOW_API UERCornerReticle : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	float Inset = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	float ArmLength = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	FLinearColor ReticleColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	FString TopRightCode = TEXT("ER // 58.01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	FString BottomRightCode = TEXT("SYS.NOMINAL");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Reticle")
	bool bDrawCenterTicks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RevealAlpha = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "ERUI|Reticle")
	void SetRevealAlpha(float InAlpha);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Reticle")
	void SetCodes(const FString& InTopRight, const FString& InBottomRight);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Tech"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SERCornerReticleLeaf> MyLeaf;
};

struct FERControlShareSlice
{
	float Share = 0.0f;
	FLinearColor Fill = FLinearColor::Black;
};

UCLASS()
class EBBINGRAINBOW_API UERControlSharePie : public UWidget
{
	GENERATED_BODY()

public:
	void SetSlices(TArray<FERControlShareSlice> InSlices);
	void SetLightOnDark(bool bInLightOnDark);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Chart")
	bool bLightOnDark = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TArray<FERControlShareSlice> Slices;
	TSharedPtr<SERControlSharePieLeaf> MyLeaf;
};
