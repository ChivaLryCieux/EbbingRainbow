// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "Battle/Skills/BattleRandomStream.h"
#include "BattleResolver.generated.h"

class ABattleUnit;
class UBattleSkillData;

/**
 * 结算管线 (顶层设计 §5) —— 全部战斗数值在这里变成事实, 并以事件广播。
 *
 * 管线顺序 (每目标每段):
 *   命中 → 暴击 → 浮动 → 地形(高低差/掩体) → 反应表(倍率/连锁) →
 *   抗性 → 护盾 → 完整度 → 状态施加 → 事件 → 死亡
 *
 * 纪律: 一切随机走 GetRng (战斗随机流); 任何模块不得自建 FRandomStream。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleResolver : public UObject
{
	GENERATED_BODY()

public:
	// ---------------- 随机源 ----------------

	/** 全战斗唯一随机流 (来自 BattleManager; 无管理器时用兜底流) */
	static FBattleRandomStream* GetRng(const UWorld* World);

	// ---------------- 技能结算 ----------------

	/**
	 * 整技能结算 (动画命中时刻由 BattleUnit 调用)。
	 * 处理: 自身效果/状态 → 目标收集 (单体/AoE) → 逐目标逐段命中结算 → 反应/连锁。
	 */
	static void ResolveSkill(ABattleUnit* Caster, ABattleUnit* Target, const FVector& TargetLocation,
		UBattleSkillData* Skill, int32 SkillLevel, const FBattleSkillExecutionContext& Context);

	// ---------------- 原子结算 ----------------

	/** 直接伤害 (状态跳动/连锁溅射): 无命中/暴击/浮动判定, 仍过抗性/护盾 */
	static float ApplyDirectDamage(ABattleUnit* Source, ABattleUnit* Target, EDamageType DamageType, float Amount);

	/** 瞬间属性修改 (只允许当前值: 完整度/护盾/AP; 派生属性被拒绝) */
	static void ApplyInstantEffects(ABattleUnit* Receiver, ABattleUnit* Source,
		const TArray<FAttributeModifier>& Mods, EDamageType DamageType);
};