// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Experimental/LegionBattle/LegionTerrainGrid.h"
#include "Experimental/LegionBattle/LegionTypes.h"
#include "LegionGameMode.generated.h"

class ALegionForest;
class ALegionUnit;
class UAnimationAsset;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/** 战场时间流速档位 */
UENUM(BlueprintType)
enum class ELegionTimeMode : uint8
{
	Normal UMETA(DisplayName = "1.0x 常速"),
	SlowMo UMETA(DisplayName = "0.2x 战术时缓"),
	Paused UMETA(DisplayName = "0.0x 战术暂停")
};

/** 尚未落地的敌军增援波次 */
struct FPendingEnemyWave
{
	FEnemyWaveLegionSpec Spec;
	FVector DropLocation = FVector::ZeroVector;
	bool bSpawned = false;
};

/**
 * 虹膜基金会大兵团战场 GameMode (Lvl_Legion):
 *  - 接管 UGalacticCampaignSubsystem 派发的阵营委托或直接进入演武测试;
 *  - 构建 FLegionTerrainGrid 坡度寻路网格与高地视野判定;
 *  - 调度敌军开局战线 + 延迟空降增援波次;
 *  - 管理战术时缓/暂停、弹道轨迹线池、战果结算回写战役子系统。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALegionGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------------- 编排参数 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	int32 LegionsPerSide = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	int32 SoldiersPerLegion = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	float LegionSpacing = 2200.0f;

	/** 两军开局纵深间距 (cm): 远大于敌方索敌视野, 确保开局不立即交战 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	float LineDistance = 9800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	float CameraBoundExtent = 14000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	float BattlefieldHalfExtent = 11500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Camera")
	float InitialArmLength = 11200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Camera")
	float InitialYawDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Camera")
	float MaxZoomArmLength = 16000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Camera")
	float EdgeScrollMarginPx = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Camera")
	float CameraPanSpeed = 3800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	bool bEnemiesAutoCharge = true;

	/** 敌方有限寻敌半径 (cm): 仅在进入此战术视野后接敌 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion", meta = (EditCondition = "bEnemiesAutoCharge"))
	float EnemyAggroRange = 3000.0f;

	/** 我方静止驻守时的近身自卫反击半径 (cm): 不会远距离主动出击 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	float PlayerAutoDefenseRange = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion")
	TSubclassOf<ALegionUnit> LegionClass;

	// ---------------- 林地 (树木障碍 + 远程掩体) ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Forest")
	bool bSpawnForest = true;

	/** 无合同时的林地种子; 有合同则用合同 TerrainSeed; 命令行 -ForestSeed=N 优先 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Forest")
	int32 DefaultForestSeed = 20261;

	/** 各部署带两侧清空的半纵深 (cm), 保证开局阵地与空降点无树 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Forest")
	float ForestClearHalfDepth = 2800.0f;

	// ---------------- 士兵资产 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UStaticMesh> AllyBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UStaticMesh> AllyHeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<USkeletalMesh> AllySkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UAnimationAsset> AllyAnimIdle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UAnimationAsset> AllyAnimMove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TArray<TSoftObjectPtr<UAnimationAsset>> AllyAnimAttacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TArray<TSoftObjectPtr<UAnimationAsset>> AllyAnimDeaths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	int32 MaxAnimatedSoldiersPerSide = 160;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UStaticMesh> EnemyBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UMaterialInterface> AllyMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Assets")
	TSoftObjectPtr<UMaterialInterface> EnemyMaterial;

	// ---------------- 查询与命令 ----------------

	const TArray<TObjectPtr<ALegionUnit>>& GetPlayerLegions() const { return PlayerLegions; }
	const TArray<TObjectPtr<ALegionUnit>>& GetEnemyLegions() const { return EnemyLegions; }
	const FLegionTerrainGrid& GetTerrainGrid() const { return TerrainGrid; }
	UTexture2D* GetMinimapTexture() const { return MinimapTexture; }
	/** 等高线地图视角用高分辨率纹理 (无则退回小地图纹理) */
	UTexture2D* GetStrategicMapTexture() const { return StrategicMapTexture ? StrategicMapTexture : MinimapTexture; }
	/** 地图放大后切换的超高精度纹理 (可能为空) */
	UTexture2D* GetDetailMapTexture() const { return DetailMapTexture; }
	bool WorldToMinimapUV(const FVector& WorldPos, FVector2D& OutUV) const;
	FVector MinimapUVToWorld(const FVector2D& UV) const;
	const TArray<FPendingEnemyWave>& GetPendingWaves() const { return PendingWaves; }
	const TArray<FLegionTracerBeam>& GetTracerBeams() const { return TracerBeams; }

	/** 真实弹道模拟: 士兵开火生成的轻量子弹 (HUD 据以绘制曳光) */
	const TArray<FLegionBullet>& GetBullets() const { return Bullets; }

	/** 士兵开火: 在 MuzzlePos 处以 Vel 速度生成一发实体弹道子弹 (阵营取自 Source) */
	void SpawnBullet(const FVector& MuzzlePos, const FVector& Vel, ALegionUnit* Source);

	void AddTracerBeam(const FVector& Start, const FVector& End, const FLinearColor& Color, float Duration = 0.25f);

	float GetBattlefieldHalfExtent() const
	{
		return EffectiveBattlefieldHalfExtent > 0.0f ? EffectiveBattlefieldHalfExtent : BattlefieldHalfExtent;
	}

	FVector GetDeployCenter() const { return DeployCenter; }
	float GetBattleElapsed() const { return Elapsed; }

	int32 CountAliveLegions(ELegionTeam Team) const;
	int32 CountAliveSoldiers(ELegionTeam Team) const;
	int32 CountPendingEnemyWaves() const;

	UFUNCTION(BlueprintCallable, Category = "Legion")
	void AllOutAssault(ELegionTeam Team);

	UFUNCTION(BlueprintCallable, Category = "Legion")
	void SetTacticalTimeMode(ELegionTimeMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Legion")
	ELegionTimeMode GetTacticalTimeMode() const { return TimeMode; }

	bool IsBattleOver() const { return bBattleOver; }
	bool DidPlayerWin() const { return bPlayerWon; }

	/** 战斗是否已打响: 接驳加载屏驻留期间为 false —— Elapsed/索敌/增援波次全部冻结,
	 *  敌军不会趁玩家还在加载画面里白嫖机动。自动化截图链路直接为 true。 */
	bool IsBattleStarted() const { return bBattleStarted; }

	/** 玩家点击「进入战场」后由加载屏调用: 解冻战斗时钟与双方接敌逻辑 */
	void BeginBattle();

	FString GetBattleTitleText() const { return BattleTitleText; }
	bool IsCampaignBattle() const { return BattleContractId != INDEX_NONE; }
	FString GetBattleSubText() const { return BattleSubText; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLegionBattleEnd, bool);
	FOnLegionBattleEnd OnLegionBattleEnd;

protected:
	virtual void BeginPlay() override;

private:
	ALegionUnit* SpawnLegion(ELegionTeam Team, ELegionClass Archetype, int32 SoldierCount,
		const FVector& Location, float YawDeg, int32 CampaignId, const FString& Callsign, int32 SyncLevel);
	ALegionUnit* FindNearestEnemy(const ALegionUnit* From) const;
	void LoadRuntimeAssets();
	void SpawnForest();
	void SpawnWaveLegion(const FPendingEnemyWave& Wave);
	void CheckBattleEnd();
	void HandleAutoShot();

	FLegionTerrainGrid TerrainGrid;

	UPROPERTY(Transient)
	TObjectPtr<ALegionForest> Forest;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALegionUnit>> PlayerLegions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALegionUnit>> EnemyLegions;

	TArray<FPendingEnemyWave> PendingWaves;
	TArray<FLegionTracerBeam> TracerBeams;
	TArray<FLegionBullet> Bullets;

	/** 逐帧推进全部在飞子弹: 线段扫描敌兵胶囊/地形静态物, 命中即结算, 脱靶即湮灭 */
	void SimulateBullets(float DeltaSeconds);

	// 运行时缓存资产 (供延迟增援波次使用)
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedAllyBody;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedEnemyBody;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedGearCube;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CachedAllyMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CachedEnemyMat;
	UPROPERTY(Transient) TObjectPtr<USkeletalMesh> CachedAllySkel;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> CachedAnimIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> CachedAnimMove;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimationAsset>> CachedAnimAttacks;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimationAsset>> CachedAnimDeaths;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> MinimapTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> StrategicMapTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> DetailMapTexture;
	bool bUseAnimatedAlly = true;
	bool bUseAnimatedEnemy = true;
	FLinearColor EnemySkeletalTint = FLinearColor(0.92f, 0.14f, 0.12f, 1.0f);

	// 记录开局各兵团初始人数, 用于结算阵亡总数
	TMap<int32, int32> InitialPlayerCounts;
	int32 BattleContractId = INDEX_NONE;
	int32 InitialEnemySoldiersTotal = 0;

	FString BattleTitleText = TEXT("军团演武");
	FString BattleSubText = TEXT("虹膜基金会  ·  多兵团战术演武");

	ELegionTimeMode TimeMode = ELegionTimeMode::Normal;
	float Elapsed = 0.0f;
	float EndCheckTimer = 0.0f;
	float AggroScanTimer = 0.0f;
	bool bBattleOver = false;
	bool bPlayerWon = false;
	/** 见 IsBattleStarted: StartPlay 末尾按自动化参数决断, 接驳屏点击「进入战场」后置 true */
	bool bBattleStarted = false;

	bool bAutoShotRequested = false;
	bool bSkyShotRequested = false;
	bool bMapShotRequested = false;
	/** -DeployShot: 不跳过接驳屏, 自动截 加载中/就绪/进入后战场 三态 (接驳屏自身到点自动点击进入) */
	bool bDeployShotRequested = false;
	/** -LegionCloseup: 战斗打响后把玩家传送到我方第一旅旁连拍, 验证持枪/开火动画/曳光近景 */
	bool bCloseupShotRequested = false;
	int32 AutoShotStage = 0;
	float AutoShotTimer = 0.0f;
	bool bOpeningShotDone = false;

	FVector DeployCenter = FVector::ZeroVector;
	float EffectiveBattlefieldHalfExtent = 0.0f;
};
