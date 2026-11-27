// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dom/JsonObject.h"
#if ER_WITH_AI_COMMAND
#include "Interfaces/IHttpRequest.h"
#endif
#include "UBattleAICommandService.generated.h"

class ABattlePlayerController;
class ABattleUnit;
class ALegionGameMode;
class ALegionPlayerController;
class ALegionUnit;
class UBattleManager;

UENUM(BlueprintType)
enum class EERAICommandLogKind : uint8
{
	System,
	User,
	Assistant,
	ToolCall,
	ToolResult,
	Error
};

USTRUCT(BlueprintType)
struct FAICommandConfig
{
	GENERATED_BODY()

	/** 接口地址 (支持 OpenAI /v1/chat/completions 与 Anthropic /v1/messages) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString EndpointUrl = TEXT("https://api.deepseek.com/v1/chat/completions");

	/** 模型名称 (如 deepseek-chat / gpt-4o-mini / qwen-plus) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString ModelName = TEXT("deepseek-chat");

	/** API Key (仅保存于本地 Saved/ER_AIConfig.json, 不进版本库) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString ApiKey;

	/** 额外战术指令 (可选追加到内置系统提示词后) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString CustomPromptExtra;

	/** 单条指令最大工具调用轮次 (支持跨多轮集火/敌方回合等待) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 MaxToolRounds = 30;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnERAICommandLog, EERAICommandLogKind /*Kind*/, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnERAICommandStateChanged, bool /*bBusy*/, const FString& /*StatusLabel*/);

/**
 * [实验性毕设模块] 战斗 AI 战术指挥服务:
 * 独立于核心战斗规则之外, 可通过 EbbingRainbow.Build.cs 中 ER_WITH_AI_COMMAND=0 一键剥离。
 *
 * 职责:
 *  - 维护跨指令的多轮战术对话上下文
 *  - 将玩家自然语言指令 + 实时战场快照 + 内置提示词 + Tool Calling 函数表发送至大模型 API
 *  - 驱动 UBattleManager / ABattleUnit 异步完成移动、技能施放、镜头聚焦与回合推进
 *  - 将战术播报同步推送到 AI 面板日志与主战斗 HUD 战报条
 */
UCLASS()
class EBBINGRAINBOW_API UBattleAICommandService : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleAICommandService* Get(const UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------- 配置管理 ----------

	const FAICommandConfig& GetConfig() const { return Config; }
	void SetConfig(const FAICommandConfig& NewConfig, bool bSaveToDisk = true);
	bool LoadConfig();
	bool SaveConfig() const;
	void ApplyPreset(const FString& PresetName);

	/** 查询预设协议/供应商的接口地址与默认模型 (DeepSeek/OpenAI/Qwen/Anthropic/Ollama) */
	static bool GetPresetValues(const FString& PresetName, FString& OutEndpoint, FString& OutModel);
	bool HasApiKey() const;
	bool IsAnthropicProtocol() const;

	// ---------- 指令与上下文 ----------

	/** 提交玩家自然语言指令 (保留多轮对话上下文) */
	bool SendUserCommand(const FString& UserInstruction);

	/** 清空多轮对话历史 (开始新对话) */
	void ClearConversationContext();

	/** 中止当前正在进行的 API 请求或异步动作等待 */
	void CancelActiveCommand();

	bool IsBusy() const { return bBusy; }
	FString GetStatusLabel() const { return CurrentStatusLabel; }
	int32 GetContextTurnCount() const { return ContextTurnCount; }

	/** 当前是否处于大兵团作战模式 (Lvl_Legion) */
	bool IsLegionMode() const;

	/** 离线自检大兵团 AI 工具链 (无需真实 API Key, 模拟大模型返回 tool_calls 验证全部部队集火敌第一旅等命令) */
	bool RunLegionToolSelfTest();

	/** 返回易读的单位序号索引摘要 (供 HUD 与玩家核对编号/旅番号) */
	FString GetUnitIndexSummary() const;

	/** 获取内置系统提示词 */
	FString BuildSystemPrompt() const;

	/** 构建当前战场状态 JSON 快照 */
	TSharedPtr<FJsonObject> BuildBattlefieldSnapshotJson() const;

	FOnERAICommandLog OnLogEntry;
	FOnERAICommandStateChanged OnStateChanged;

private:
	struct FPendingToolCall
	{
		FString Id;
		FString Name;
		FString ArgumentsJson;
	};

	UBattleManager* GetBattleManager() const;
	ABattlePlayerController* GetBattlePC() const;
	ALegionGameMode* GetLegionGameMode() const;
	ALegionPlayerController* GetLegionPC() const;

	ABattleUnit* ResolveUnitRef(const FString& RefStr) const;
	FString GetUnitCode(const ABattleUnit* TargetUnit) const;

	// ---- 大兵团作战模式辅助 ----
	FString GetLegionUnitIndexSummary() const;
	FString BuildLegionSystemPrompt() const;
	TSharedPtr<FJsonObject> BuildLegionSnapshotJson() const;
	ALegionUnit* ResolveLegionUnitRef(const FString& RefStr, bool bPreferEnemy) const;
	void ResolveAllyLegions(const TSharedPtr<FJsonObject>& Args, TArray<ALegionUnit*>& OutAllies) const;
	FString GetLegionCode(const ALegionUnit* Unit) const;

	void SetBusyState(bool bInBusy, const FString& StatusText);
	void EmitLog(EERAICommandLogKind Kind, const FString& Text, bool bBroadcastToBattleHUD = false);
	void BroadcastBattleHUDStatus(const FString& Message);

#if ER_WITH_AI_COMMAND
	void DispatchChatRequest();
	void HandleHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, int32 ReqSessionId);
	void HandleOpenAIResponse(const TSharedPtr<FJsonObject>& RespRoot);
	void HandleAnthropicResponse(const TSharedPtr<FJsonObject>& RespRoot);

	void ExecuteNextToolCall();
	void FinishToolCallAndContinue(const FString& ToolResultJson);
	void FlushAnthropicToolResultsIfReady();

	/** 大兵团作战同步工具实现 (暂停态下直接生效, 地图上实时刷新命令线) */
	bool DispatchLegionToolCall(const FString& ToolName, const TSharedPtr<FJsonObject>& Args, FString& OutResultJson);
	TArray<TSharedPtr<FJsonValue>> BuildLegionToolsJson(bool bAnthropic) const;
	FString Tool_LegionOrderAttack(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderAllAssault();
	FString Tool_LegionOrderMove(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderMoveRelative(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderHalt(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderFollowCommander(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionSelectAllies(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderGuard(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderPatrol(const TSharedPtr<FJsonObject>& Args);
	FString Tool_LegionOrderRecon(const TSharedPtr<FJsonObject>& Args);

	/** 同步工具实现 */
	FString Tool_GetBattleState();
	FString Tool_FocusCamera(const TSharedPtr<FJsonObject>& Args);
	FString Tool_EndTurn(const TSharedPtr<FJsonObject>& Args);

	/** 异步工具实现 (带移动结束/延迟命中/回合自动结算/敌方回合等待) */
	void StartAsyncMoveTowardsUnit(const TSharedPtr<FJsonObject>& Args);
	void StartAsyncMoveToPosition(const TSharedPtr<FJsonObject>& Args);
	void StartAsyncUseSkill(const TSharedPtr<FJsonObject>& Args);
	void StartAsyncAdvanceAndAttack(const TSharedPtr<FJsonObject>& Args);
	void StartAsyncWaitForPlayerTurn();

	/** 辅助: 选出对目标最佳的可用/理想攻击技能与期望接敌距离 */
	int32 PickBestOffensiveSkill(ABattleUnit* Caster, ABattleUnit* Target, bool bRequireUsableNow) const;
	float ComputeDesiredEngageDistance(ABattleUnit* Caster, ABattleUnit* Target) const;
	FVector ChooseSmartMoveDestination(ABattleUnit* Caster, ABattleUnit* Target, float DesiredDistCm, bool bCheckLOS) const;

	/** 动作后若 AP 耗尽, 等 TryAutoEndTurn 推进完再返回工具结果, 防止 AI 误结束下一位回合 */
	void ReturnMoveOrSkillResultAfterAutoEndCheck(ABattleUnit* ActedUnit, const FString& ResultStatus, const FString& ExtraNote = FString());
	void HandleAutoEndSettleCheck();
	void HandleAdvanceAttackMoveFinished(ABattleUnit* Unit);
	void HandleAdvanceAttackSkillSettled();
	void HandleWaitTurnPoll();

	void HandleAsyncWaitTimeout();
	void HandleSkillSettleDone();
	void CleanupAsyncWait();

	TArray<TSharedPtr<FJsonValue>> BuildOpenAIToolsJson() const;
	TArray<TSharedPtr<FJsonValue>> BuildAnthropicToolsJson() const;
	void TrimConversationHistory();
	bool TryParseFallbackToolCallFromContent(const FString& Content, TArray<FPendingToolCall>& OutCalls) const;

	static FString JsonObjectToString(const TSharedPtr<FJsonObject>& Obj);

	TArray<TSharedPtr<FJsonValue>> ConversationMessages;
	TArray<TSharedPtr<FJsonValue>> PendingAnthropicToolResults;
	TArray<FPendingToolCall> PendingToolCalls;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveHttpRequest;
#endif

	UFUNCTION()
	void HandleObservedUnitMoveFinished(ABattleUnit* Unit);

	UFUNCTION()
	void HandleWaitTurnStarted(ABattleUnit* NewActiveUnit);

	UFUNCTION()
	void HandleWaitBattleFinished(bool bPlayerWon);

	static FString GetConfigFilePath();

	FAICommandConfig Config;
	bool bBusy = false;
	FString CurrentStatusLabel = TEXT("待命");
	int32 SessionId = 0;
	int32 CurrentToolRound = 0;
	int32 ContextTurnCount = 0;
	int32 ActiveToolCallIndex = 0;
	bool bLastRequestWasAnthropic = false;

	TWeakObjectPtr<ABattleUnit> ObservedMoveUnit;
	TWeakObjectPtr<ABattleUnit> PendingActionUnit;
	TWeakObjectPtr<ABattleUnit> PendingAdvanceTarget;
	int32 PendingPreferredSkillIndex = INDEX_NONE;
	bool bPendingEndTurnIfDone = false;
	bool bWaitingForPlayerTurn = false;
	FString PendingReturnStatus;
	FString PendingReturnNote;
	FString ActiveAsyncToolName;
	FTimerHandle AsyncWaitTimerHandle;
	FTimerHandle WaitTurnPollTimerHandle;
};
