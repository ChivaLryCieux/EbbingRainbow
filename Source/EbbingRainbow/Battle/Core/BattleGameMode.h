// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Battle/Core/BattleTypes.h"
#include "WFC/WFCGenerationTypes.h"
#include "BattleGameMode.generated.h"

class ABattleArena;
class ABattleUnit;
class UBattleManager;
class UWFC3DStyleTable;
class URunSubsystem;

/** 回合制战斗 GameMode: 负责在开局创建竞技场、按出生点生成双方单位并初始化战斗管理器 */
UCLASS()
class EBBINGRAINBOW_API ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattleGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** 竞技场类 (默认 ABattleArena,可在蓝图中替换以调整参数) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABattleArena> ArenaClass;

	/** 单位类 (默认 ABattleUnit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABattleUnit> UnitClass;

	/** 竞技场原点 (WFC 网格 (0,0,0) 所在位置) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FVector ArenaOrigin = FVector::ZeroVector;

	/** 当前进度对应的建筑风格（后续由 RunState / Act 映射写入） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	EWFCBuildingStyle ArenaStyle = EWFCBuildingStyle::Style0;

	/** 本场战斗的 WFC 规模（X/Y/Z 格数）。Arch Atrium 使用 19x19x2（约 76m x 76m，8m 十字中庭 + 4m 夹层） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FIntVector ArenaScale = FIntVector(19, 19, 2);

	/** 风格表：把 ArenaStyle 映射到具体 WFC 生成器类。为空时回退到 Arch Atrium */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TObjectPtr<UWFC3DStyleTable> WFCStyleTable = nullptr;

	/** 我方 / 敌方名单 (每个条目对应一个出生点,按顺序) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TArray<FBattleUnitStats> PlayerRoster;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TArray<FBattleUnitStats> EnemyRoster;

	UFUNCTION(BlueprintPure, Category = "Battle")
	ABattleArena* GetArena() const { return Arena; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	UBattleManager* GetBattleManager() const { return BattleManager; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetPlayerUnits() const { return PlayerUnits; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetEnemyUnits() const { return EnemyUnits; }

	/** 竞技场与单位全部就绪 */
	DECLARE_MULTICAST_DELEGATE(FOnBattleSetupComplete);
	FOnBattleSetupComplete OnBattleSetupComplete;

	bool IsBattleSetupComplete() const { return bSetupComplete; }

	/** 默认我方名单（Run 系统复用同一份数值，避免两处漂移）。 */
	static const TArray<FBattleUnitStats>& GetDefaultPlayerRoster();

	/** 战斗结束回调：写回 Run 状态并把结果交给结算面板（跳转由玩家点按钮触发）。 */
	UFUNCTION()
	void HandleBattleFinished(bool bPlayerWon);

	/** 结算面板的「返回」按钮：航线图战斗胜利 -> 回航线图，其余 -> 回大厅。 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ReturnFromBattle();

	/** 调试作弊: 秒杀全部敌人 (战斗界面右上角的 DEBUG 按钮) */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void DebugKillAllEnemies();

	/** 本场战斗是否来自航线图节点 */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsRunBattle() const { return bRunBattle; }

	/** 本场是否已经结算完 (结算面板据此决定要不要弹出) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsBattleResolved() const { return bBattleResultHandled; }

	/** 玩家是否获胜 (未结算时为 false) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool DidPlayerWin() const { return bBattleResultHandled && bLastBattleWon; }

	/** 结算按钮是否应该回航线图 */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool ShouldReturnToRoute() const { return bRunBattle && bBattleResultHandled && bLastBattleWon; }

	/** 本场对应的航线图节点 (非航线图战斗为 INDEX_NONE) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetPendingNodeId() const { return PendingNodeId; }

private:
	UFUNCTION()
	void HandleArenaGenerated(bool bSuccess);

	void SpawnTeam(const TArray<FBattleUnitStats>& Roster, const TArray<FVector>& Points, EBattleTeam Team, TArray<ABattleUnit*>& OutUnits);

	URunSubsystem* GetRunSubsystem() const;
	void ReturnToRoute();
	void ReturnToHall();

	UPROPERTY(Transient)
	TObjectPtr<ABattleArena> Arena = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleManager> BattleManager = nullptr;

	TArray<ABattleUnit*> PlayerUnits;
	TArray<ABattleUnit*> EnemyUnits;
	bool bSetupComplete = false;

	// ---------- 航线图战斗衔接 ----------

	/** 本场战斗是否来自航线图节点 */
	bool bRunBattle = false;
	/** 是否已经处理过战斗结果（防止重复广播导致二次推进） */
	bool bBattleResultHandled = false;
	/** 本场战斗玩家是否获胜 (结算面板读取) */
	bool bLastBattleWon = false;
	/** 本场对应的路线节点 / 遭遇种子 */
	int32 PendingNodeId = INDEX_NONE;
	int32 PendingEncounterSeed = 0;
	/** -1=默认生成器; 2=自然岛 (BattleNature 节点) */
	int32 PendingGeneratorChoice = -1;
	/** 战斗单位 -> 队伍下标的映射（阵亡成员不生成，所以不能按下标硬对） */
	TArray<int32> PlayerPartyIndices;
	FTimerHandle AutoWinTimerHandle;
	FTimerHandle AutoReturnTimerHandle;
	FTimerHandle AutoShotTimerHandle;

	/** 调试截图用的临时俯瞰相机 */
	UPROPERTY()
	TObjectPtr<class ACameraActor> ShotCamera = nullptr;
};
