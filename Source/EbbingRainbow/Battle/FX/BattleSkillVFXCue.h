// Copyright Ebbing Rainbow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Battle/FX/BattleVFXTypes.h"
#include "BattleSkillVFXCue.generated.h"

class UNiagaraComponent;

/**
 * 技能特效演出蓝图扩展基类 (可选):
 * 绝大多数技能由 UBattleVFXSubsystem + Niagara 参数驱动即可;
 * 若某个招牌技能需要多段 Timeline、自定义网格动画、特殊音效等，可新建蓝图继承本类并填入
 * UBattleSkillData::VFX.CustomCueClass，系统将在对应阶段自动生成并触发 OnPlaySkillCue。
 */
UCLASS(Blueprintable, Abstract)
class EBBINGRAINBOW_API ABattleSkillVFXCue : public AActor
{
	GENERATED_BODY()

public:
	ABattleSkillVFXCue();

	/** 由 UBattleVFXSubsystem 调用，分发到蓝图事件 */
	void PlayCue(const FBattleVFXSpawnContext& InContext);

	/** 蓝图实现入口 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|VFX", meta = (DisplayName = "On Play Skill Cue"))
	void OnPlaySkillCue(const FBattleVFXSpawnContext& Context);

	/** 辅助: 统一把主题色 / 起止点 / 半径注入到蓝图内的 Niagara 组件 */
	UFUNCTION(BlueprintCallable, Category = "Battle|VFX")
	static void ApplyContextToNiagara(UNiagaraComponent* NiagaraComp, const FBattleVFXSpawnContext& Context,
		FName ColorParam = TEXT("User.ThemeColor"),
		FName StartParam = TEXT("User.StartPos"),
		FName EndParam = TEXT("User.TargetPos"),
		FName RadiusParam = TEXT("User.Radius"));

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Battle|VFX")
	FBattleVFXSpawnContext CachedContext;
};
