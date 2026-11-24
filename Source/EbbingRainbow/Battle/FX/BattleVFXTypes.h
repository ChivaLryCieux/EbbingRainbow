// Copyright Ebbing Rainbow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleVFXTypes.generated.h"

class ABattleSkillVFXCue;
class ABattleUnit;
class UBattleSkillData;
class UNiagaraSystem;

/** 技能特效表现形态 (Auto 会按 RangeProfile / AoE / Chain / TargetFilter 自动推导) */
UENUM(BlueprintType)
enum class EVFXDeliveryPattern : uint8
{
	Auto          UMETA(DisplayName = "自动推导"),
	MeleeSlash    UMETA(DisplayName = "近战斩击弧"),
	RangedBeam    UMETA(DisplayName = "远程射线/电弧"),
	ProjectileArc UMETA(DisplayName = "抛射弹道球"),
	SelfNova      UMETA(DisplayName = "自身扩散新星 (AoE)"),
	GroundAoE     UMETA(DisplayName = "落点范围轰炸 (AoE)"),
	ChainArc      UMETA(DisplayName = "连锁跳跃电弧"),
	SelfBuff      UMETA(DisplayName = "自身增益/护盾光环")
};

/** 技能特效生命周期阶段 */
UENUM(BlueprintType)
enum class EBattleVFXCueStage : uint8
{
	Cast,       // 起手聚能 / 挥砍 / 弹道发射
	Area,       // 范围扩散 (SelfNova / GroundAoE)
	ChainJump,  // 连锁弹跳段 (From -> Next)
	Impact,     // 命中爆点 (含暴击强化)
	Reaction    // 元素反应附加爆发
};

/**
 * 挂在 UBattleSkillData 上的特效表现配置:
 * - 优先走 Niagara (按需填入 NS_* 资产，自动注入主题色/起止点/半径 User 参数)
 * - 可填 CustomCueClass (继承 ABattleSkillVFXCue 的蓝图)，实现完全自定义多段演出
 * - 若都留空，则由 UBattleVFXSubsystem 自动驱动同色系程序化光效兜底，保证零资产即可验证
 */
USTRUCT(BlueprintType)
struct FBattleSkillVFXConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	EVFXDeliveryPattern Pattern = EVFXDeliveryPattern::Auto;

	/** 默认随施法者战棋/技能主题色；勾选后强制使用 ColorOverride */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	bool bOverrideColor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (EditCondition = "bOverrideColor"))
	FLinearColor ColorOverride = FLinearColor(0.20f, 0.45f, 0.95f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ScaleMultiplier = 1.0f;

	/** 可选蓝图演出覆盖: 若指定，则生成该 Actor 并触发 OnPlaySkillCue */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Blueprint")
	TSubclassOf<ABattleSkillVFXCue> CustomCueClass;

	/** 起手/施法身侧 Niagara 资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Niagara")
	TSoftObjectPtr<UNiagaraSystem> CastNiagara;

	/** 光束/电弧/弹道拖尾 Niagara 资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Niagara")
	TSoftObjectPtr<UNiagaraSystem> BeamOrTrailNiagara;

	/** 范围冲击波/毒雾/新星 Niagara 资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Niagara")
	TSoftObjectPtr<UNiagaraSystem> AreaNiagara;

	/** 命中溅射/暴击爆发 Niagara 资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Niagara")
	TSoftObjectPtr<UNiagaraSystem> ImpactNiagara;

	// ---- Niagara User Parameter 统一命名约定 ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|NiagaraParams")
	FName ColorParamName = TEXT("User.ThemeColor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|NiagaraParams")
	FName StartParamName = TEXT("User.StartPos");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|NiagaraParams")
	FName EndParamName = TEXT("User.TargetPos");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|NiagaraParams")
	FName RadiusParamName = TEXT("User.Radius");
};

/** 传递给 VFX 系统与可选蓝图 Cue 的完整上下文 */
USTRUCT(BlueprintType)
struct FBattleVFXSpawnContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	EBattleVFXCueStage Stage = EBattleVFXCueStage::Cast;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	EVFXDeliveryPattern ResolvedPattern = EVFXDeliveryPattern::MeleeSlash;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TObjectPtr<ABattleUnit> Caster = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TObjectPtr<ABattleUnit> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TObjectPtr<const UBattleSkillData> Skill = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FLinearColor ThemeColor = FLinearColor(0.20f, 0.45f, 0.95f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	float Duration = 0.35f;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	float ScaleMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	bool bCrit = false;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	bool bReaction = false;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	bool bJaggedArc = false;
};
