// Copyright Ebbing Rainbow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/FX/BattleVFXTypes.h"
#include "BattleVFXSubsystem.generated.h"

class ABattleUnit;
class UBattleSkillData;
class UNiagaraComponent;
class UNiagaraSystem;
struct FBattleHitResult;

/**
 * 技能特效统一调度子系统 (纯观察者):
 * 订阅 UBattleEventHub 广播的施法/区域/连锁/命中事件，按“技能 & 战棋主题色”分派特效：
 *   1) 若配置了 CustomCueClass，生成自定义蓝图演出 Actor；
 *   2) 若配置了对应的 Niagara 资产，生成 UNiagaraComponent 并注入主题色/起止点/半径 User 参数；
 *   3) 若均留空，则生成同色系的 ABattleProceduralVFXActor 兜底光效，保证零资产即可完整联调。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleVFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleVFXSubsystem* Get(const UWorld* World);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** 解析技能特效主题色 (保证同战棋与同系技能色调一致) */
	static FLinearColor ResolveSkillThemeColor(const ABattleUnit* Caster, const UBattleSkillData* Skill);

	/** 自动或显式推导技能表现形态 */
	static EVFXDeliveryPattern ResolveDeliveryPattern(const UBattleSkillData* Skill);

	/** 供外部或蓝图直接触发一段规范化特效 */
	void PlayCue(const FBattleVFXSpawnContext& Context);

private:
	void HandleSkillCastStarted(ABattleUnit* Caster, const UBattleSkillData* Skill, ABattleUnit* Target,
		const FVector& TargetLocation, float HitDelay);
	void HandleSkillAreaTriggered(ABattleUnit* Caster, const UBattleSkillData* Skill,
		const FVector& TargetLocation, float Radius);
	void HandleChainArcTriggered(ABattleUnit* Caster, ABattleUnit* FromUnit, ABattleUnit* ToUnit,
		const UBattleSkillData* Skill, int32 JumpIndex, float Delay);
	void HandleHitResolved(const FBattleHitResult& Hit);

	void ScheduleCue(const FBattleVFXSpawnContext& Context, float DelaySeconds);
	FVector GetUnitAnchorLocation(const ABattleUnit* Unit, float HeightFraction = 0.25f) const;
	const TSoftObjectPtr<UNiagaraSystem>* SelectNiagaraSlot(const FBattleSkillVFXConfig& Config,
		EBattleVFXCueStage Stage, EVFXDeliveryPattern Pattern) const;

	FDelegateHandle CastHandle;
	FDelegateHandle AreaHandle;
	FDelegateHandle ChainHandle;
	FDelegateHandle HitHandle;

	int32 SeedCounter = 0;
};
