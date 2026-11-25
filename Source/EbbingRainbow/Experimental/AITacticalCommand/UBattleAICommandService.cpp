// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "Core/ERLog.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Core/UBattleContext.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Terrain/BattleTerrainQuery.h"
#include "Battle/Units/BattleUnit.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionTerrainGrid.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

#if ER_WITH_AI_COMMAND
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#endif

#if ER_WITH_AI_COMMAND
namespace
{
	TSharedPtr<FJsonObject> MakeJsonProp(const FString& Type, const FString& Desc)
	{
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("type"), Type);
		P->SetStringField(TEXT("description"), Desc);
		return P;
	}

	TSharedPtr<FJsonObject> MakeStringArrayProp(const FString& Desc)
	{
		TSharedPtr<FJsonObject> Items = MakeShared<FJsonObject>();
		Items->SetStringField(TEXT("type"), TEXT("string"));

		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("type"), TEXT("array"));
		P->SetObjectField(TEXT("items"), Items);
		P->SetStringField(TEXT("description"), Desc);
		return P;
	}

	TSharedPtr<FJsonObject> MakeParametersSchema(
		const TMap<FString, TSharedPtr<FJsonObject>>& Props,
		const TArray<FString>& Required)
	{
		TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Props)
		{
			PropertiesObj->SetObjectField(Pair.Key, Pair.Value);
		}

		TArray<TSharedPtr<FJsonValue>> ReqArr;
		for (const FString& R : Required)
		{
			ReqArr.Add(MakeShared<FJsonValueString>(R));
		}

		TSharedPtr<FJsonObject> ParamsObj = MakeShared<FJsonObject>();
		ParamsObj->SetStringField(TEXT("type"), TEXT("object"));
		ParamsObj->SetObjectField(TEXT("properties"), PropertiesObj);
		ParamsObj->SetArrayField(TEXT("required"), ReqArr);
		return ParamsObj;
	}

	TSharedPtr<FJsonObject> MakeOpenAIToolDef(
		const FString& Name,
		const FString& Description,
		const TMap<FString, TSharedPtr<FJsonObject>>& Props,
		const TArray<FString>& Required)
	{
		TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
		FuncObj->SetStringField(TEXT("name"), Name);
		FuncObj->SetStringField(TEXT("description"), Description);
		FuncObj->SetObjectField(TEXT("parameters"), MakeParametersSchema(Props, Required));

		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("type"), TEXT("function"));
		ToolObj->SetObjectField(TEXT("function"), FuncObj);
		return ToolObj;
	}

	TSharedPtr<FJsonObject> MakeAnthropicToolDef(
		const FString& Name,
		const FString& Description,
		const TMap<FString, TSharedPtr<FJsonObject>>& Props,
		const TArray<FString>& Required)
	{
		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("name"), Name);
		ToolObj->SetStringField(TEXT("description"), Description);
		ToolObj->SetObjectField(TEXT("input_schema"), MakeParametersSchema(Props, Required));
		return ToolObj;
	}

	FString TargetFilterToString(ESkillTargetFilter Filter)
	{
		switch (Filter)
		{
		case ESkillTargetFilter::EnemyOnly:  return TEXT("EnemyOnly");
		case ESkillTargetFilter::AllyOrSelf: return TEXT("AllyOrSelf");
		case ESkillTargetFilter::SelfOnly:   return TEXT("SelfOnly");
		case ESkillTargetFilter::Any:        return TEXT("Any");
		default:                             return TEXT("Unknown");
		}
	}

	FString CoverToString(ECoverQuality Cover)
	{
		switch (Cover)
		{
		case ECoverQuality::None: return TEXT("None");
		case ECoverQuality::Half: return TEXT("Half");
		case ECoverQuality::Full: return TEXT("Full");
		default:                  return TEXT("None");
		}
	}
}
#endif

UBattleAICommandService* UBattleAICommandService::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleAICommandService>() : nullptr;
}

bool UBattleAICommandService::ShouldCreateSubsystem(UObject* Outer) const
{
#if ER_WITH_AI_COMMAND
	return Super::ShouldCreateSubsystem(Outer);
#else
	return false;
#endif
}

void UBattleAICommandService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();
	CurrentStatusLabel = HasApiKey() ? TEXT("就绪") : TEXT("未配置 Key");
}

void UBattleAICommandService::Deinitialize()
{
	CancelActiveCommand();
	Super::Deinitialize();
}

FString UBattleAICommandService::GetConfigFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ER_AIConfig.json"));
}

bool UBattleAICommandService::LoadConfig()
{
#if ER_WITH_AI_COMMAND
	const FString Path = GetConfigFilePath();
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Root->TryGetStringField(TEXT("endpoint_url"), Config.EndpointUrl);
	Root->TryGetStringField(TEXT("model_name"), Config.ModelName);
	Root->TryGetStringField(TEXT("api_key"), Config.ApiKey);
	Root->TryGetStringField(TEXT("custom_prompt_extra"), Config.CustomPromptExtra);
	int32 MaxRounds = Config.MaxToolRounds;
	if (Root->TryGetNumberField(TEXT("max_tool_rounds"), MaxRounds))
	{
		Config.MaxToolRounds = FMath::Clamp(MaxRounds, 1, 50);
	}
	return true;
#else
	return false;
#endif
}

bool UBattleAICommandService::SaveConfig() const
{
#if ER_WITH_AI_COMMAND
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("endpoint_url"), Config.EndpointUrl);
	Root->SetStringField(TEXT("model_name"), Config.ModelName);
	Root->SetStringField(TEXT("api_key"), Config.ApiKey);
	Root->SetStringField(TEXT("custom_prompt_extra"), Config.CustomPromptExtra);
	Root->SetNumberField(TEXT("max_tool_rounds"), Config.MaxToolRounds);

	const FString OutStr = JsonObjectToString(Root);
	return FFileHelper::SaveStringToFile(OutStr, *GetConfigFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#else
	return false;
#endif
}

void UBattleAICommandService::SetConfig(const FAICommandConfig& NewConfig, bool bSaveToDisk)
{
	Config = NewConfig;
	Config.EndpointUrl.TrimStartAndEndInline();
	Config.ModelName.TrimStartAndEndInline();
	Config.ApiKey.TrimStartAndEndInline();
	Config.MaxToolRounds = FMath::Clamp(Config.MaxToolRounds, 1, 50);

	if (bSaveToDisk)
	{
		SaveConfig();
	}

	if (!bBusy)
	{
		SetBusyState(false, HasApiKey() ? TEXT("就绪") : TEXT("未配置 Key"));
	}
}

bool UBattleAICommandService::GetPresetValues(const FString& PresetName, FString& OutEndpoint, FString& OutModel)
{
	struct FPresetRow { const TCHAR* Name; const TCHAR* Endpoint; const TCHAR* Model; };
	static const FPresetRow Presets[] =
	{
		{ TEXT("DeepSeek"),  TEXT("https://api.deepseek.com/v1/chat/completions"),                        TEXT("deepseek-chat")    },
		{ TEXT("OpenAI"),    TEXT("https://api.openai.com/v1/chat/completions"),                          TEXT("gpt-4o-mini")      },
		{ TEXT("Qwen"),      TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),  TEXT("qwen-plus")        },
		{ TEXT("Anthropic"), TEXT("https://api.anthropic.com/v1/messages"),                               TEXT("claude-sonnet-4-5") },
		{ TEXT("Ollama"),    TEXT("http://localhost:11434/v1/chat/completions"),                          TEXT("qwen2.5:7b")       },
	};
	for (const FPresetRow& Row : Presets)
	{
		if (PresetName.Equals(Row.Name, ESearchCase::IgnoreCase))
		{
			OutEndpoint = Row.Endpoint;
			OutModel = Row.Model;
			return true;
		}
	}
	return false;
}

void UBattleAICommandService::ApplyPreset(const FString& PresetName)
{
	FString Endpoint, Model;
	if (GetPresetValues(PresetName, Endpoint, Model))
	{
		Config.EndpointUrl = Endpoint;
		Config.ModelName = Model;
	}
}

bool UBattleAICommandService::HasApiKey() const
{
	if (!Config.ApiKey.IsEmpty())
	{
		return true;
	}
	return Config.EndpointUrl.Contains(TEXT("localhost")) || Config.EndpointUrl.Contains(TEXT("127.0.0.1"));
}

bool UBattleAICommandService::IsAnthropicProtocol() const
{
	return Config.EndpointUrl.Contains(TEXT("/anthropic")) || Config.EndpointUrl.EndsWith(TEXT("/messages"));
}

void UBattleAICommandService::SetBusyState(bool bInBusy, const FString& StatusText)
{
	bBusy = bInBusy;
	CurrentStatusLabel = StatusText;
	OnStateChanged.Broadcast(bBusy, CurrentStatusLabel);
}

void UBattleAICommandService::EmitLog(EERAICommandLogKind Kind, const FString& Text, bool bBroadcastToBattleHUD)
{
	UE_LOG(LogERBattle, Log, TEXT("[AICommand][%d] %s"), (int32)Kind, *Text);
	OnLogEntry.Broadcast(Kind, Text);
	if (bBroadcastToBattleHUD)
	{
		BroadcastBattleHUDStatus(Text);
	}
}

void UBattleAICommandService::BroadcastBattleHUDStatus(const FString& Message)
{
	if (UBattleManager* Mgr = GetBattleManager())
	{
		const FString SingleLine = Message.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(" "));
		Mgr->OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("[AI 战术官] %s"), *SingleLine.Left(96)));
	}
}

UBattleManager* UBattleAICommandService::GetBattleManager() const
{
	UBattleContext* Ctx = UBattleContext::Get(GetWorld());
	return Ctx ? Ctx->GetManager() : nullptr;
}

ABattlePlayerController* UBattleAICommandService::GetBattlePC() const
{
	UWorld* W = GetWorld();
	return W ? Cast<ABattlePlayerController>(W->GetFirstPlayerController()) : nullptr;
}

ALegionGameMode* UBattleAICommandService::GetLegionGameMode() const
{
	UWorld* W = GetWorld();
	return W ? Cast<ALegionGameMode>(W->GetAuthGameMode()) : nullptr;
}

ALegionPlayerController* UBattleAICommandService::GetLegionPC() const
{
	UWorld* W = GetWorld();
	return W ? Cast<ALegionPlayerController>(W->GetFirstPlayerController()) : nullptr;
}

bool UBattleAICommandService::IsLegionMode() const
{
	return GetLegionGameMode() != nullptr;
}

FString UBattleAICommandService::GetUnitCode(const ABattleUnit* TargetUnit) const
{
	const UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || !TargetUnit)
	{
		return TEXT("?");
	}

	int32 PIdx = 0;
	int32 EIdx = 0;
	for (const ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (!U) continue;
		if (U->Team == EBattleTeam::Player)
		{
			++PIdx;
			if (U == TargetUnit) return FString::Printf(TEXT("P%d"), PIdx);
		}
		else
		{
			++EIdx;
			if (U == TargetUnit) return FString::Printf(TEXT("E%d"), EIdx);
		}
	}
	return TEXT("?");
}

ABattleUnit* UBattleAICommandService::ResolveUnitRef(const FString& RefStr) const
{
	const UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		return nullptr;
	}

	FString Clean = RefStr;
	Clean.TrimStartAndEndInline();
	if (Clean.IsEmpty())
	{
		return nullptr;
	}

	if (Clean.Equals(TEXT("self"), ESearchCase::IgnoreCase)
		|| Clean.Equals(TEXT("active"), ESearchCase::IgnoreCase)
		|| Clean.Equals(TEXT("当前"))
		|| Clean.Equals(TEXT("自身")))
	{
		return Mgr->GetActiveUnit();
	}

	TArray<ABattleUnit*> Players;
	TArray<ABattleUnit*> Enemies;
	for (ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (!U) continue;
		if (U->Team == EBattleTeam::Player) Players.Add(U);
		else Enemies.Add(U);
	}

	const FString Upper = Clean.ToUpper();
	if (Upper.StartsWith(TEXT("P")) && Upper.Mid(1).IsNumeric())
	{
		const int32 Idx = FCString::Atoi(*Upper.Mid(1)) - 1;
		return Players.IsValidIndex(Idx) ? Players[Idx] : nullptr;
	}
	if (Upper.StartsWith(TEXT("E")) && Upper.Mid(1).IsNumeric())
	{
		const int32 Idx = FCString::Atoi(*Upper.Mid(1)) - 1;
		return Enemies.IsValidIndex(Idx) ? Enemies[Idx] : nullptr;
	}

	for (ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (U && U->Stats.DisplayName.Equals(Clean, ESearchCase::IgnoreCase))
		{
			return U;
		}
	}
	for (ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (U && U->Stats.DisplayName.Contains(Clean, ESearchCase::IgnoreCase))
		{
			return U;
		}
	}

	return nullptr;
}

FString UBattleAICommandService::GetUnitIndexSummary() const
{
	if (IsLegionMode())
	{
		return GetLegionUnitIndexSummary();
	}

	const UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		return TEXT("战斗未初始化");
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	const FString ActiveStr = Active
		? FString::Printf(TEXT("%s %s (AP %d)"), *GetUnitCode(Active), *Active->Stats.DisplayName, Active->AP)
		: TEXT("无");

	FString PlayersStr;
	FString EnemiesStr;
	int32 PIdx = 0;
	int32 EIdx = 0;
	for (const ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (!U) continue;
		if (U->Team == EBattleTeam::Player)
		{
			++PIdx;
			if (!PlayersStr.IsEmpty()) PlayersStr += TEXT(" ");
			PlayersStr += FString::Printf(TEXT("P%d:%s%s"),
				PIdx, *U->Stats.DisplayName, U->IsAlive() ? TEXT("") : TEXT("(亡)"));
		}
		else
		{
			++EIdx;
			if (!EnemiesStr.IsEmpty()) EnemiesStr += TEXT(" ");
			EnemiesStr += FString::Printf(TEXT("E%d:%s%s"),
				EIdx, *U->Stats.DisplayName, U->IsAlive() ? TEXT("") : TEXT("(亡)"));
		}
	}

	return FString::Printf(TEXT("当前: %s | 上下文: %d轮\n我方: %s\n敌方: %s"),
		*ActiveStr, ContextTurnCount, *PlayersStr, *EnemiesStr);
}

FString UBattleAICommandService::BuildSystemPrompt() const
{
	if (IsLegionMode())
	{
		return BuildLegionSystemPrompt();
	}

	FString Prompt = TEXT(
		"你要扮演科幻肉鸽战棋游戏《虹不我与 (Ebbing Rainbow)》中的战术 AI 指挥助手。\n"
		"你的职责是接收玩家指挥官的自然语言指令，结合实时战场上下文，自主规划并连续调用引擎函数操控我方棋子完成战术目标。\n\n"
		"【战场与棋子设定】\n"
		"1. 我方通常有 4 名棋子：P1(力学-近战)、P2(热学-高机动/中程+周身AoE)、P3(电学-远程连锁)、P4(化学-中远程酸蚀)；敌方编号为 E1, E2, E3...。\n"
		"2. 敌我双方按先攻值穿插行动（顶部先攻条顺序），同一时刻只有 active_unit 可以行动。\n"
		"   - 若某单位 AP 耗尽，引擎会自动在 0.45 秒后推进到下一位（工具返回值会给出最新 active_unit_code，无需重复调用 end_turn）。\n"
		"   - 即使轮到敌方行动，只要当前指令还没完成（如“全部棋子集火攻击1号敌人”、“直到1号敌人死亡”），**绝对不要半途中断或让玩家重新输入**，请立即调用 `wait_for_player_turn()` 等敌方全部行动完毕后自动续接下一位我方棋子！\n\n"
		"【核心函数工具】\n"
		"1. `advance_and_attack(target_id, skill_index, end_turn_if_done)` 【首选战斗宏函数】：\n"
		"   - 在一次调用中自动为当前 active_unit 完成：检查射程/视线 → 必要时沿导航网格绕掩体靠近 → 到达后若仍有 AP 立即施放最佳攻击技能（可连打至 AP 不足）→ 耗尽或 `end_turn_if_done=true` 时自动稳妥交棒下一位。\n"
		"   - 当玩家下达“攻击/集火/所有棋子打某目标”等指令时，优先对每位轮到的我方棋子直接调用本函数！\n"
		"2. `wait_for_player_turn()` 【跨敌方回合续接】：\n"
		"   - 当 `active_unit_team == 'Enemy'` 时调用，挂起等待所有连续敌方 AI 行动结束，直到下一个我方单位回合或战斗结束，然后自动返回最新战况让你继续指挥。\n"
		"3. `end_turn(unit_id)`：安全结束指定我方单位（传 unit_id 如 'P1'）的当前回合；若该单位已自动结束则安全忽略，绝不会误跳下一个单位。\n"
		"4. 细粒度工具：`move_towards_unit(target_id, desired_distance_cm)`、`move_to_position(x, y, z)`、`use_skill(skill_index, target_id, skill_id)`、`focus_camera(target_id)`、`get_battle_state()`。\n\n"
		"【多单位 / 持续指令执行规范】\n"
		"- 例1：玩家说“全部棋子集火攻击1号敌人”（或“直到1号敌人死亡”）：\n"
		"  循环检查战况：只要目标 E1 仍存活且战斗未结束 —— 若当前是 Enemy 回合就调 `wait_for_player_turn()`；若当前是 Player 回合就调 `advance_and_attack(target_id='E1', end_turn_if_done=true)`；\n"
		"  - 若指令没说“直到死亡”，在所有存活我方棋子（P1~P4）本轮各自推进/攻击一次后即可总结汇报；\n"
		"  - 若指令说了“直到死亡”，则持续多轮直到 E1 阵亡或战斗结束！\n"
		"- 例2：玩家点名特定棋子（如“让P1攻击E1”），若当前轮到 P2 行动且玩家没要求跳过 P2，请直接播报当前是 P2 回合，询问是否由 P2 执行或跳过到 P1。\n\n"
		"【UI 播报规范】\n"
		"- 每次执行或结束时给出简短中文战术播报（如：“已收到指令。正指挥 2 号单位（热学）向 1 号敌人推进并集火攻击。”），严禁虚构未发生在工具回执里的伤害或击杀。"
	);

	if (!Config.CustomPromptExtra.IsEmpty())
	{
		Prompt += TEXT("\n\n【附加战术约束】\n") + Config.CustomPromptExtra;
	}
	return Prompt;
}

TSharedPtr<FJsonObject> UBattleAICommandService::BuildBattlefieldSnapshotJson() const
{
	if (IsLegionMode())
	{
		return BuildLegionSnapshotJson();
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
#if ER_WITH_AI_COMMAND
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		Root->SetStringField(TEXT("error"), TEXT("BattleManager not ready"));
		return Root;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	Root->SetNumberField(TEXT("round"), Mgr->GetRoundIndex());
	Root->SetNumberField(TEXT("battle_state"), (int32)Mgr->GetBattleState());
	Root->SetBoolField(TEXT("battle_over"), Mgr->GetBattleState() == EBattleState::BattleOver);
	Root->SetStringField(TEXT("active_unit_code"), GetUnitCode(Active));
	Root->SetStringField(TEXT("active_unit_name"), Active ? Active->Stats.DisplayName : TEXT("None"));
	Root->SetStringField(TEXT("active_unit_team"), Active ? (Active->Team == EBattleTeam::Player ? TEXT("Player") : TEXT("Enemy")) : TEXT("None"));

	UBattleTerrainQuery* Terrain = UBattleTerrainQuery::Get(GetWorld());

	auto MakeUnitJson = [this, Active, Terrain](ABattleUnit* U, const FString& Code)
	{
		TSharedPtr<FJsonObject> UObj = MakeShared<FJsonObject>();
		UObj->SetStringField(TEXT("id"), Code);
		UObj->SetStringField(TEXT("name"), U->Stats.DisplayName);
		UObj->SetStringField(TEXT("team"), U->Team == EBattleTeam::Player ? TEXT("Player") : TEXT("Enemy"));
		UObj->SetBoolField(TEXT("alive"), U->IsAlive());
		UObj->SetBoolField(TEXT("is_active"), U == Active);

		const UBattleSkillComponent* Comp = U->GetSkillComponent();
		const float MaxHP = Comp ? Comp->GetEffectiveMaxIntegrity() : U->Stats.MaxIntegrity;
		const int32 MaxAP = Comp ? Comp->GetEffectiveMaxAP() : U->Stats.MaxAP;
		const float Init = Comp ? Comp->GetEffectiveInitiative() : (float)U->Stats.Initiative;

		UObj->SetNumberField(TEXT("hp"), FMath::RoundToFloat(U->Integrity * 10.0f) / 10.0f);
		UObj->SetNumberField(TEXT("max_hp"), MaxHP);
		UObj->SetNumberField(TEXT("barrier"), FMath::RoundToFloat(U->Barrier * 10.0f) / 10.0f);
		UObj->SetNumberField(TEXT("ap"), U->AP);
		UObj->SetNumberField(TEXT("max_ap"), MaxAP);
		UObj->SetNumberField(TEXT("initiative"), Init);
		UObj->SetNumberField(TEXT("cm_per_ap"), U->GetCmPerAP());

		const FVector Loc = U->GetActorLocation();
		TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
		PosObj->SetNumberField(TEXT("x"), FMath::RoundToInt(Loc.X));
		PosObj->SetNumberField(TEXT("y"), FMath::RoundToInt(Loc.Y));
		PosObj->SetNumberField(TEXT("z"), FMath::RoundToInt(Loc.Z));
		UObj->SetObjectField(TEXT("position"), PosObj);

		if (Comp)
		{
			const FString StatusSummary = Comp->GetStatusSummary();
			if (!StatusSummary.IsEmpty())
			{
				UObj->SetStringField(TEXT("statuses"), StatusSummary);
			}
		}

		if (Active && U != Active && U->IsAlive())
		{
			const float DistCm = FVector::Dist(Active->GetActorLocation(), Loc);
			UObj->SetNumberField(TEXT("distance_from_active_cm"), FMath::RoundToInt(DistCm));
			if (Terrain)
			{
				const FBattleTerrainContext TC = Terrain->Evaluate(Active, U);
				UObj->SetBoolField(TEXT("has_los_from_active"), TC.bHasLineOfSight);
				UObj->SetStringField(TEXT("cover_from_active"), CoverToString(TC.Cover));
				UObj->SetNumberField(TEXT("height_delta_cm"), FMath::RoundToInt(TC.HeightDelta));
			}
		}

		if (U->Team == EBattleTeam::Player || U == Active)
		{
			TArray<TSharedPtr<FJsonValue>> SkillsArr;
			const TArray<FBattleSkillSpec>& Specs = U->GetSkillSpecs();
			for (int32 i = 0; i < Specs.Num(); ++i)
			{
				const FBattleSkillSpec& Spec = Specs[i];
				const UBattleSkillData* Skill = Spec.SkillData;
				if (!Skill) continue;

				TSharedPtr<FJsonObject> SObj = MakeShared<FJsonObject>();
				SObj->SetNumberField(TEXT("index"), i);
				SObj->SetStringField(TEXT("skill_id"), Skill->SkillId.ToString());
				SObj->SetStringField(TEXT("name"), Skill->DisplayName.ToString());
				SObj->SetNumberField(TEXT("ap_cost"), Skill->APCost);
				SObj->SetNumberField(TEXT("range_cm"), Skill->Range);
				SObj->SetNumberField(TEXT("aoe_radius_cm"), Skill->AoE.Radius);
				SObj->SetNumberField(TEXT("chain_jumps"), Skill->Chain.MaxJumps);
				SObj->SetNumberField(TEXT("est_damage"), Skill->GetEstimatedTotalDamage());
				SObj->SetStringField(TEXT("damage_type"), GetDamageTypeName(Skill->Damage.DamageType));
				SObj->SetStringField(TEXT("target_filter"), TargetFilterToString(Skill->TargetFilter));
				SObj->SetBoolField(TEXT("requires_los"), Skill->bRequiresLineOfSight);
				SObj->SetBoolField(TEXT("on_cooldown"), Spec.IsOnCooldown());
				SObj->SetNumberField(TEXT("cooldown_remaining"), Spec.CooldownRemaining);
				SkillsArr.Add(MakeShared<FJsonValueObject>(SObj));
			}
			UObj->SetArrayField(TEXT("skills"), SkillsArr);
		}

		return UObj;
	};

	TArray<TSharedPtr<FJsonValue>> PlayersArr;
	TArray<TSharedPtr<FJsonValue>> EnemiesArr;
	int32 PIdx = 0;
	int32 EIdx = 0;
	for (ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (!U) continue;
		if (U->Team == EBattleTeam::Player)
		{
			++PIdx;
			PlayersArr.Add(MakeShared<FJsonValueObject>(MakeUnitJson(U, FString::Printf(TEXT("P%d"), PIdx))));
		}
		else
		{
			++EIdx;
			EnemiesArr.Add(MakeShared<FJsonValueObject>(MakeUnitJson(U, FString::Printf(TEXT("E%d"), EIdx))));
		}
	}

	Root->SetArrayField(TEXT("players"), PlayersArr);
	Root->SetArrayField(TEXT("enemies"), EnemiesArr);

	TArray<TSharedPtr<FJsonValue>> QueueArr;
	for (const ABattleUnit* U : Mgr->GetInitiativeQueue())
	{
		if (U && U->IsAlive())
		{
			QueueArr.Add(MakeShared<FJsonValueString>(GetUnitCode(U)));
		}
	}
	Root->SetArrayField(TEXT("initiative_order"), QueueArr);
#endif
	return Root;
}

#if ER_WITH_AI_COMMAND

TArray<TSharedPtr<FJsonValue>> UBattleAICommandService::BuildOpenAIToolsJson() const
{
	if (IsLegionMode())
	{
		return BuildLegionToolsJson(false);
	}

	TArray<TSharedPtr<FJsonValue>> Tools;

	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, E2, P1")));
		Props.Add(TEXT("skill_index"), MakeJsonProp(TEXT("integer"), TEXT("可选：优先使用的技能索引（-1 或不传则自动选最佳进攻技能）")));
		Props.Add(TEXT("end_turn_if_done"), MakeJsonProp(TEXT("boolean"), TEXT("若移动/攻击后仍有剩余 AP 但无法继续攻击目标，是否自动结束该单位回合（集火指令建议 true）")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("advance_and_attack"),
			TEXT("【首选】让当前我方行动单位自动规划靠近目标、绕开遮挡并在射程内立刻施放最佳技能（可连续施法），随后稳妥交棒下一位。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("wait_for_player_turn"),
			TEXT("当当前是敌方回合（active_unit_team=='Enemy'）时调用，自动等待全部连续敌方 AI 行动完毕并返回下一个我方回合的实时战况。"),
			Props, {})));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("get_battle_state"),
			TEXT("获取当前实时战场状态快照，包括当前行动单位、各单位坐标、HP/AP、技能列表及距离/视线信息。"),
			Props, {})));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, E2, P1, Goblin")));
		Props.Add(TEXT("desired_distance_cm"), MakeJsonProp(TEXT("number"), TEXT("期望停在距离目标多少厘米处（近战建议 160，远程建议 500，默认 160）")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("move_towards_unit"),
			TEXT("让当前行动单位沿导航网格向目标移动，并在到达或耗尽本回合移动预算后返回更新后的状态。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("x"), MakeJsonProp(TEXT("number"), TEXT("目标世界坐标 X")));
		Props.Add(TEXT("y"), MakeJsonProp(TEXT("number"), TEXT("目标世界坐标 Y")));
		Props.Add(TEXT("z"), MakeJsonProp(TEXT("number"), TEXT("目标世界坐标 Z")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("move_to_position"),
			TEXT("让当前行动单位移动到指定世界坐标 (x, y, z)。"),
			Props, { TEXT("x"), TEXT("y"), TEXT("z") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("skill_index"), MakeJsonProp(TEXT("integer"), TEXT("技能索引 (0, 1, 2...)，对应单位 skills 列表中的 index")));
		Props.Add(TEXT("skill_id"), MakeJsonProp(TEXT("string"), TEXT("可选：技能 ID 或名称（如 Slash、相位斩击）")));
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, P1, self")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("use_skill"),
			TEXT("让当前行动单位对目标施放技能。若为 SelfOnly 技能可传 target_id='self'。"),
			Props, { TEXT("skill_index"), TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("要聚焦的单位编号或名称，例如 P1, E1")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("focus_camera"),
			TEXT("将战术摄像机平滑聚焦到指定棋子。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("unit_id"), MakeJsonProp(TEXT("string"), TEXT("要结束回合的单位编号（如 P1, P2），强烈建议传入以防该单位已自动结束而误跳过下一个单位")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeOpenAIToolDef(
			TEXT("end_turn"),
			TEXT("结束指定我方行动单位的回合，推进先攻队列到下一位。"),
			Props, {})));
	}

	return Tools;
}

TArray<TSharedPtr<FJsonValue>> UBattleAICommandService::BuildAnthropicToolsJson() const
{
	if (IsLegionMode())
	{
		return BuildLegionToolsJson(true);
	}

	TArray<TSharedPtr<FJsonValue>> Tools;

	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, E2, P1")));
		Props.Add(TEXT("skill_index"), MakeJsonProp(TEXT("integer"), TEXT("可选：优先技能索引")));
		Props.Add(TEXT("end_turn_if_done"), MakeJsonProp(TEXT("boolean"), TEXT("完成后若无法继续攻击是否结束本单位回合")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("advance_and_attack"),
			TEXT("【首选】自动完成当前我方单位靠近目标并施放最佳进攻技能（含连击与稳妥交棒）。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("wait_for_player_turn"),
			TEXT("等待敌方连续 AI 回合结束并回到下一个我方行动回合。"),
			Props, {})));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("get_battle_state"),
			TEXT("获取当前实时战场状态快照。"),
			Props, {})));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, E2, P1")));
		Props.Add(TEXT("desired_distance_cm"), MakeJsonProp(TEXT("number"), TEXT("期望停在距离目标多少厘米处（近战建议 160）")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("move_towards_unit"),
			TEXT("让当前行动单位沿导航网格向目标移动。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("x"), MakeJsonProp(TEXT("number"), TEXT("目标 X")));
		Props.Add(TEXT("y"), MakeJsonProp(TEXT("number"), TEXT("目标 Y")));
		Props.Add(TEXT("z"), MakeJsonProp(TEXT("number"), TEXT("目标 Z")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("move_to_position"),
			TEXT("让当前行动单位移动到指定世界坐标。"),
			Props, { TEXT("x"), TEXT("y"), TEXT("z") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("skill_index"), MakeJsonProp(TEXT("integer"), TEXT("技能索引 (0, 1, 2...)")));
		Props.Add(TEXT("skill_id"), MakeJsonProp(TEXT("string"), TEXT("可选：技能 ID 或名称")));
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标单位编号或名称，例如 E1, self")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("use_skill"),
			TEXT("让当前行动单位对目标施放技能。"),
			Props, { TEXT("skill_index"), TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("要聚焦的单位编号或名称")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("focus_camera"),
			TEXT("将战术摄像机聚焦到指定棋子。"),
			Props, { TEXT("target_id") })));
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("unit_id"), MakeJsonProp(TEXT("string"), TEXT("要结束回合的单位编号（如 P1），防止误跳下一位")));
		Tools.Add(MakeShared<FJsonValueObject>(MakeAnthropicToolDef(
			TEXT("end_turn"),
			TEXT("结束指定我方单位回合。"),
			Props, {})));
	}

	return Tools;
}

void UBattleAICommandService::TrimConversationHistory()
{
	constexpr int32 MaxHistoryItems = 36;
	if (ConversationMessages.Num() <= MaxHistoryItems)
	{
		return;
	}

	int32 CutIndex = ConversationMessages.Num() - MaxHistoryItems;
	while (CutIndex < ConversationMessages.Num())
	{
		const TSharedPtr<FJsonObject>* MsgObj = nullptr;
		if (ConversationMessages[CutIndex]->TryGetObject(MsgObj) && MsgObj && (*MsgObj).IsValid())
		{
			FString Role;
			FString ContentStr;
			if ((*MsgObj)->TryGetStringField(TEXT("role"), Role)
				&& Role == TEXT("user")
				&& (*MsgObj)->TryGetStringField(TEXT("content"), ContentStr))
			{
				break;
			}
		}
		++CutIndex;
	}

	if (CutIndex > 0 && CutIndex < ConversationMessages.Num())
	{
		ConversationMessages.RemoveAt(0, CutIndex);
	}
}

#endif // ER_WITH_AI_COMMAND

bool UBattleAICommandService::SendUserCommand(const FString& UserInstruction)
{
#if ER_WITH_AI_COMMAND
	FString Trimmed = UserInstruction;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	if (bBusy)
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("上一条指令仍在执行中，请稍候或点击中止。"));
		return false;
	}

	if (Config.EndpointUrl.IsEmpty() || Config.ModelName.IsEmpty())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("请先在 API 设置中填写 Endpoint URL 和 Model 名称。"));
		return false;
	}

	if (!HasApiKey())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("尚未配置 API Key，请先在右上角“API 设置”中填入 Key 并保存。"));
		return false;
	}

	if (!GetBattleManager() && !GetLegionGameMode())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("战斗尚未就绪，无法获取战场上下文。"));
		return false;
	}

	const bool bUseAnthropic = IsAnthropicProtocol();
	if (ConversationMessages.Num() > 0 && bLastRequestWasAnthropic != bUseAnthropic)
	{
		ConversationMessages.Reset();
		ContextTurnCount = 0;
	}
	bLastRequestWasAnthropic = bUseAnthropic;

	++SessionId;
	CurrentToolRound = 0;
	++ContextTurnCount;
	PendingToolCalls.Reset();
	PendingAnthropicToolResults.Reset();
	ActiveToolCallIndex = 0;

	EmitLog(EERAICommandLogKind::User, Trimmed);
	TrimConversationHistory();

	{
		const FString SnapshotStr = JsonObjectToString(BuildBattlefieldSnapshotJson());
		const FString FullUserMsg = FString::Printf(
			TEXT("【当前实时战场状态 JSON】\n%s\n\n【玩家指令】\n%s"),
			*SnapshotStr, *Trimmed);

		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), TEXT("user"));
		Msg->SetStringField(TEXT("content"), FullUserMsg);
		ConversationMessages.Add(MakeShared<FJsonValueObject>(Msg));
	}

	SetBusyState(true, TEXT("请求 AI 中..."));
	DispatchChatRequest();
	return true;
#else
	return false;
#endif
}

void UBattleAICommandService::ClearConversationContext()
{
	CancelActiveCommand();
#if ER_WITH_AI_COMMAND
	ConversationMessages.Reset();
	PendingAnthropicToolResults.Reset();
#endif
	ContextTurnCount = 0;
	EmitLog(EERAICommandLogKind::System, TEXT("已开启新对话，旧战术上下文已清空。"));
	SetBusyState(false, HasApiKey() ? TEXT("就绪") : TEXT("未配置 Key"));
}

void UBattleAICommandService::CancelActiveCommand()
{
	++SessionId;
#if ER_WITH_AI_COMMAND
	CleanupAsyncWait();

	if (ActiveHttpRequest.IsValid())
	{
		ActiveHttpRequest->CancelRequest();
		ActiveHttpRequest.Reset();
	}

	PendingToolCalls.Reset();
	PendingAnthropicToolResults.Reset();
	ActiveToolCallIndex = 0;
#endif

	if (bBusy)
	{
		SetBusyState(false, HasApiKey() ? TEXT("已中止") : TEXT("未配置 Key"));
		EmitLog(EERAICommandLogKind::System, TEXT("已中止当前 AI 指令。"));
	}
}

void UBattleAICommandService::HandleObservedUnitMoveFinished(ABattleUnit* Unit)
{
#if ER_WITH_AI_COMMAND
	if (!ObservedMoveUnit.IsValid() || ObservedMoveUnit.Get() != Unit)
	{
		return;
	}

	const FString CurrentTool = ActiveAsyncToolName;
	ABattleUnit* AdvanceTarget = PendingAdvanceTarget.Get();
	const int32 PreferredSkill = PendingPreferredSkillIndex;
	const bool bEndTurnIfDone = bPendingEndTurnIfDone;
	CleanupAsyncWait();

	if (CurrentTool == TEXT("advance_and_attack"))
	{
		PendingAdvanceTarget = AdvanceTarget;
		PendingPreferredSkillIndex = PreferredSkill;
		bPendingEndTurnIfDone = bEndTurnIfDone;
		HandleAdvanceAttackMoveFinished(Unit);
		return;
	}

	ReturnMoveOrSkillResultAfterAutoEndCheck(Unit, TEXT("move_finished"));
#endif
}

void UBattleAICommandService::HandleWaitTurnStarted(ABattleUnit* NewActiveUnit)
{
#if ER_WITH_AI_COMMAND
	HandleWaitTurnPoll();
#endif
}

void UBattleAICommandService::HandleWaitBattleFinished(bool bPlayerWon)
{
#if ER_WITH_AI_COMMAND
	if (!bWaitingForPlayerTurn)
	{
		return;
	}
	CleanupAsyncWait();

	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("status"), TEXT("battle_over"));
	Res->SetBoolField(TEXT("player_won"), bPlayerWon);
	Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
	FinishToolCallAndContinue(JsonObjectToString(Res));
#endif
}

#if ER_WITH_AI_COMMAND

void UBattleAICommandService::DispatchChatRequest()
{
	const int32 EffectiveLimit = FMath::Max(Config.MaxToolRounds, 20);
	if (CurrentToolRound >= EffectiveLimit)
	{
		EmitLog(EERAICommandLogKind::Error, FString::Printf(TEXT("已达到单次指令最大轮次 (%d)，自动停止以防死循环。"), EffectiveLimit));
		SetBusyState(false, TEXT("已达轮次上限"));
		return;
	}
	++CurrentToolRound;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Config.EndpointUrl);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetTimeout(45.0f);

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (IsAnthropicProtocol())
	{
		if (!Config.ApiKey.IsEmpty())
		{
			Req->SetHeader(TEXT("x-api-key"), Config.ApiKey);
			Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.ApiKey));
		}
		Req->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));

		Body->SetStringField(TEXT("model"), Config.ModelName);
		Body->SetStringField(TEXT("system"), BuildSystemPrompt());
		Body->SetNumberField(TEXT("max_tokens"), 1024);
		Body->SetArrayField(TEXT("messages"), ConversationMessages);
		Body->SetArrayField(TEXT("tools"), BuildAnthropicToolsJson());
	}
	else
	{
		if (!Config.ApiKey.IsEmpty())
		{
			Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.ApiKey));
		}

		TArray<TSharedPtr<FJsonValue>> OpenAIMessages;
		{
			TSharedPtr<FJsonObject> SysMsg = MakeShared<FJsonObject>();
			SysMsg->SetStringField(TEXT("role"), TEXT("system"));
			SysMsg->SetStringField(TEXT("content"), BuildSystemPrompt());
			OpenAIMessages.Add(MakeShared<FJsonValueObject>(SysMsg));
		}
		OpenAIMessages.Append(ConversationMessages);

		Body->SetStringField(TEXT("model"), Config.ModelName);
		Body->SetArrayField(TEXT("messages"), OpenAIMessages);
		Body->SetArrayField(TEXT("tools"), BuildOpenAIToolsJson());
		Body->SetStringField(TEXT("tool_choice"), TEXT("auto"));
		Body->SetNumberField(TEXT("temperature"), 0.2);
	}

	Req->SetContentAsString(JsonObjectToString(Body));

	const int32 CapturedSessionId = SessionId;
	TWeakObjectPtr<UBattleAICommandService> WeakSelf(this);
	Req->OnProcessRequestComplete().BindLambda(
		[WeakSelf, CapturedSessionId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
		{
			if (UBattleAICommandService* Self = WeakSelf.Get())
			{
				Self->HandleHttpResponse(Request, Response, bConnected, CapturedSessionId);
			}
		});

	ActiveHttpRequest = Req;
	SetBusyState(true, FString::Printf(TEXT("AI 思考中 (第%d轮)..."), CurrentToolRound));

	if (!Req->ProcessRequest())
	{
		ActiveHttpRequest.Reset();
		EmitLog(EERAICommandLogKind::Error, TEXT("无法发起 HTTP 请求，请检查 Endpoint URL 格式。"));
		SetBusyState(false, TEXT("请求失败"));
	}
}

void UBattleAICommandService::HandleHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, int32 ReqSessionId)
{
	if (ReqSessionId != SessionId)
	{
		return;
	}
	ActiveHttpRequest.Reset();

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("网络连接失败或请求超时，请检查网络、代理或 Endpoint URL。"));
		SetBusyState(false, TEXT("连接失败"));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString RespStr = Response->GetContentAsString();
	if (Code < 200 || Code >= 300)
	{
		EmitLog(EERAICommandLogKind::Error, FString::Printf(TEXT("API 返回 HTTP %d: %s"), Code, *RespStr.Left(400)));
		SetBusyState(false, FString::Printf(TEXT("HTTP %d"), Code));
		return;
	}

	TSharedPtr<FJsonObject> RespRoot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespStr);
	if (!FJsonSerializer::Deserialize(Reader, RespRoot) || !RespRoot.IsValid())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("API 响应 JSON 解析失败。"));
		SetBusyState(false, TEXT("解析失败"));
		return;
	}

	if (IsAnthropicProtocol() && RespRoot->HasField(TEXT("content")))
	{
		HandleAnthropicResponse(RespRoot);
	}
	else
	{
		HandleOpenAIResponse(RespRoot);
	}
}

void UBattleAICommandService::HandleOpenAIResponse(const TSharedPtr<FJsonObject>& RespRoot)
{
	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!RespRoot->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->Num() == 0)
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("API 响应中缺少 choices 字段。"));
		SetBusyState(false, TEXT("格式异常"));
		return;
	}

	const TSharedPtr<FJsonObject>* ChoiceObj = nullptr;
	if (!(*Choices)[0]->TryGetObject(ChoiceObj) || !ChoiceObj || !(*ChoiceObj).IsValid())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("API choices[0] 无效。"));
		SetBusyState(false, TEXT("格式异常"));
		return;
	}

	const TSharedPtr<FJsonObject>* MessageObj = nullptr;
	if (!(*ChoiceObj)->TryGetObjectField(TEXT("message"), MessageObj) || !MessageObj || !(*MessageObj).IsValid())
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("API choice 中缺少 message 字段。"));
		SetBusyState(false, TEXT("格式异常"));
		return;
	}

	ConversationMessages.Add(MakeShared<FJsonValueObject>(*MessageObj));

	FString ContentText;
	(*MessageObj)->TryGetStringField(TEXT("content"), ContentText);
	ContentText.TrimStartAndEndInline();

	PendingToolCalls.Reset();
	PendingAnthropicToolResults.Reset();
	ActiveToolCallIndex = 0;

	const TArray<TSharedPtr<FJsonValue>>* ToolCallsArr = nullptr;
	if ((*MessageObj)->TryGetArrayField(TEXT("tool_calls"), ToolCallsArr) && ToolCallsArr && ToolCallsArr->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& Val : *ToolCallsArr)
		{
			const TSharedPtr<FJsonObject>* CallObj = nullptr;
			if (!Val->TryGetObject(CallObj) || !CallObj || !(*CallObj).IsValid()) continue;

			FPendingToolCall Call;
			(*CallObj)->TryGetStringField(TEXT("id"), Call.Id);
			const TSharedPtr<FJsonObject>* FuncObj = nullptr;
			if ((*CallObj)->TryGetObjectField(TEXT("function"), FuncObj) && FuncObj && (*FuncObj).IsValid())
			{
				(*FuncObj)->TryGetStringField(TEXT("name"), Call.Name);
				(*FuncObj)->TryGetStringField(TEXT("arguments"), Call.ArgumentsJson);
			}
			if (!Call.Name.IsEmpty())
			{
				PendingToolCalls.Add(Call);
			}
		}
	}
	else if (!ContentText.IsEmpty())
	{
		TryParseFallbackToolCallFromContent(ContentText, PendingToolCalls);
	}

	if (PendingToolCalls.Num() > 0)
	{
		if (!ContentText.IsEmpty())
		{
			EmitLog(EERAICommandLogKind::Assistant, ContentText, true);
		}
		ExecuteNextToolCall();
		return;
	}

	if (!ContentText.IsEmpty())
	{
		EmitLog(EERAICommandLogKind::Assistant, ContentText, true);
	}
	else
	{
		EmitLog(EERAICommandLogKind::System, TEXT("AI 未返回可执行内容。"));
	}
	SetBusyState(false, TEXT("就绪"));
}

void UBattleAICommandService::HandleAnthropicResponse(const TSharedPtr<FJsonObject>& RespRoot)
{
	const TArray<TSharedPtr<FJsonValue>>* ContentArr = nullptr;
	if (!RespRoot->TryGetArrayField(TEXT("content"), ContentArr) || !ContentArr)
	{
		EmitLog(EERAICommandLogKind::Error, TEXT("Anthropic 响应中缺少 content 数组。"));
		SetBusyState(false, TEXT("格式异常"));
		return;
	}

	TSharedPtr<FJsonObject> AssistantMsg = MakeShared<FJsonObject>();
	AssistantMsg->SetStringField(TEXT("role"), TEXT("assistant"));
	AssistantMsg->SetArrayField(TEXT("content"), *ContentArr);
	ConversationMessages.Add(MakeShared<FJsonValueObject>(AssistantMsg));

	PendingToolCalls.Reset();
	PendingAnthropicToolResults.Reset();
	ActiveToolCallIndex = 0;

	FString CombinedText;
	for (const TSharedPtr<FJsonValue>& BlockVal : *ContentArr)
	{
		const TSharedPtr<FJsonObject>* BlockObj = nullptr;
		if (!BlockVal->TryGetObject(BlockObj) || !BlockObj || !(*BlockObj).IsValid()) continue;

		FString Type;
		(*BlockObj)->TryGetStringField(TEXT("type"), Type);
		if (Type == TEXT("text"))
		{
			FString TextPart;
			(*BlockObj)->TryGetStringField(TEXT("text"), TextPart);
			if (!TextPart.IsEmpty())
			{
				if (!CombinedText.IsEmpty()) CombinedText += TEXT("\n");
				CombinedText += TextPart;
			}
		}
		else if (Type == TEXT("tool_use"))
		{
			FPendingToolCall Call;
			(*BlockObj)->TryGetStringField(TEXT("id"), Call.Id);
			(*BlockObj)->TryGetStringField(TEXT("name"), Call.Name);
			const TSharedPtr<FJsonObject>* InputObj = nullptr;
			if ((*BlockObj)->TryGetObjectField(TEXT("input"), InputObj) && InputObj && (*InputObj).IsValid())
			{
				Call.ArgumentsJson = JsonObjectToString(*InputObj);
			}
			else
			{
				Call.ArgumentsJson = TEXT("{}");
			}
			if (!Call.Name.IsEmpty())
			{
				PendingToolCalls.Add(Call);
			}
		}
	}

	CombinedText.TrimStartAndEndInline();
	if (PendingToolCalls.Num() > 0)
	{
		if (!CombinedText.IsEmpty())
		{
			EmitLog(EERAICommandLogKind::Assistant, CombinedText, true);
		}
		ExecuteNextToolCall();
		return;
	}

	if (!CombinedText.IsEmpty())
	{
		EmitLog(EERAICommandLogKind::Assistant, CombinedText, true);
	}
	SetBusyState(false, TEXT("就绪"));
}

void UBattleAICommandService::ExecuteNextToolCall()
{
	if (ActiveToolCallIndex >= PendingToolCalls.Num())
	{
		FlushAnthropicToolResultsIfReady();
		DispatchChatRequest();
		return;
	}

	const FPendingToolCall& Call = PendingToolCalls[ActiveToolCallIndex];
	EmitLog(EERAICommandLogKind::ToolCall, FString::Printf(TEXT("%s(%s)"), *Call.Name, *Call.ArgumentsJson));

	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	if (!Call.ArgumentsJson.IsEmpty())
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Call.ArgumentsJson);
		FJsonSerializer::Deserialize(Reader, Args);
		if (!Args.IsValid())
		{
			Args = MakeShared<FJsonObject>();
		}
	}

	SetBusyState(true, FString::Printf(TEXT("执行 %s..."), *Call.Name));

	if (IsLegionMode())
	{
		FString LegionResultJson;
		if (DispatchLegionToolCall(Call.Name, Args, LegionResultJson))
		{
			FinishToolCallAndContinue(LegionResultJson);
			return;
		}
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown legion tool: %s"), *Call.Name));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	if (Call.Name == TEXT("get_battle_state"))
	{
		FinishToolCallAndContinue(Tool_GetBattleState());
	}
	else if (Call.Name == TEXT("focus_camera"))
	{
		FinishToolCallAndContinue(Tool_FocusCamera(Args));
	}
	else if (Call.Name == TEXT("end_turn"))
	{
		FinishToolCallAndContinue(Tool_EndTurn(Args));
	}
	else if (Call.Name == TEXT("advance_and_attack"))
	{
		StartAsyncAdvanceAndAttack(Args);
	}
	else if (Call.Name == TEXT("wait_for_player_turn"))
	{
		StartAsyncWaitForPlayerTurn();
	}
	else if (Call.Name == TEXT("move_towards_unit"))
	{
		StartAsyncMoveTowardsUnit(Args);
	}
	else if (Call.Name == TEXT("move_to_position"))
	{
		StartAsyncMoveToPosition(Args);
	}
	else if (Call.Name == TEXT("use_skill"))
	{
		StartAsyncUseSkill(Args);
	}
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown tool: %s"), *Call.Name));
		FinishToolCallAndContinue(JsonObjectToString(Err));
	}
}

void UBattleAICommandService::FlushAnthropicToolResultsIfReady()
{
	if (IsAnthropicProtocol() && PendingAnthropicToolResults.Num() > 0)
	{
		TSharedPtr<FJsonObject> UserToolMsg = MakeShared<FJsonObject>();
		UserToolMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserToolMsg->SetArrayField(TEXT("content"), PendingAnthropicToolResults);
		ConversationMessages.Add(MakeShared<FJsonValueObject>(UserToolMsg));
		PendingAnthropicToolResults.Reset();
	}
}

void UBattleAICommandService::FinishToolCallAndContinue(const FString& ToolResultJson)
{
	if (!bBusy || !PendingToolCalls.IsValidIndex(ActiveToolCallIndex))
	{
		return;
	}

	const FPendingToolCall& Call = PendingToolCalls[ActiveToolCallIndex];
	EmitLog(EERAICommandLogKind::ToolResult, FString::Printf(TEXT("%s -> %s"), *Call.Name, *ToolResultJson.Left(240)));

	if (IsAnthropicProtocol() && !Call.Id.IsEmpty())
	{
		TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
		Block->SetStringField(TEXT("type"), TEXT("tool_result"));
		Block->SetStringField(TEXT("tool_use_id"), Call.Id);
		Block->SetStringField(TEXT("content"), ToolResultJson);
		PendingAnthropicToolResults.Add(MakeShared<FJsonValueObject>(Block));
	}
	else if (!Call.Id.IsEmpty())
	{
		TSharedPtr<FJsonObject> ToolMsg = MakeShared<FJsonObject>();
		ToolMsg->SetStringField(TEXT("role"), TEXT("tool"));
		ToolMsg->SetStringField(TEXT("tool_call_id"), Call.Id);
		ToolMsg->SetStringField(TEXT("content"), ToolResultJson);
		ConversationMessages.Add(MakeShared<FJsonValueObject>(ToolMsg));
	}
	else
	{
		TSharedPtr<FJsonObject> ToolMsg = MakeShared<FJsonObject>();
		ToolMsg->SetStringField(TEXT("role"), TEXT("user"));
		ToolMsg->SetStringField(TEXT("content"), FString::Printf(TEXT("工具 %s 执行结果：\n%s"), *Call.Name, *ToolResultJson));
		ConversationMessages.Add(MakeShared<FJsonValueObject>(ToolMsg));
	}

	++ActiveToolCallIndex;
	ExecuteNextToolCall();
}

FString UBattleAICommandService::Tool_GetBattleState()
{
	return JsonObjectToString(BuildBattlefieldSnapshotJson());
}

FString UBattleAICommandService::Tool_FocusCamera(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	FString TargetId;
	Args->TryGetStringField(TEXT("target_id"), TargetId);

	ABattlePlayerController* PC = GetBattlePC();
	ABattleUnit* Target = ResolveUnitRef(TargetId);
	if (!PC || !Target)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("reason"), TEXT("未找到目标单位或玩家控制器"));
		return JsonObjectToString(Res);
	}

	PC->FocusOnUnit(Target);
	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("focused"), FString::Printf(TEXT("%s (%s)"), *GetUnitCode(Target), *Target->Stats.DisplayName));
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_EndTurn(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("reason"), TEXT("BattleManager 无效"));
		return JsonObjectToString(Res);
	}

	ABattleUnit* CurrentActive = Mgr->GetActiveUnit();
	FString ExpectedUnitId;
	Args->TryGetStringField(TEXT("unit_id"), ExpectedUnitId);
	ExpectedUnitId.TrimStartAndEndInline();

	// 竞态防护: 若指定了 unit_id 但该单位的回合已被 TryAutoEndTurn 自动推进，绝不误跳当前新单位的回合
	if (!ExpectedUnitId.IsEmpty())
	{
		ABattleUnit* ExpectedUnit = ResolveUnitRef(ExpectedUnitId);
		if (ExpectedUnit && CurrentActive != ExpectedUnit)
		{
			Res->SetBoolField(TEXT("ok"), true);
			Res->SetBoolField(TEXT("already_ended"), true);
			Res->SetStringField(TEXT("requested_unit"), GetUnitCode(ExpectedUnit));
			Res->SetStringField(TEXT("current_active_unit"), GetUnitCode(CurrentActive));
			Res->SetStringField(TEXT("current_active_team"), CurrentActive ? (CurrentActive->Team == EBattleTeam::Player ? TEXT("Player") : TEXT("Enemy")) : TEXT("None"));
			return JsonObjectToString(Res);
		}
	}

	if (!CurrentActive || CurrentActive->Team != EBattleTeam::Player)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("reason"), TEXT("当前不是我方单位回合，无法由玩家端结束回合"));
		Res->SetStringField(TEXT("current_active_unit"), GetUnitCode(CurrentActive));
		Res->SetStringField(TEXT("current_active_team"), CurrentActive ? (CurrentActive->Team == EBattleTeam::Player ? TEXT("Player") : TEXT("Enemy")) : TEXT("None"));
		return JsonObjectToString(Res);
	}

	if (ABattlePlayerController* PC = GetBattlePC())
	{
		PC->RequestEndTurn();
	}
	else
	{
		Mgr->EndCurrentTurn();
	}

	ABattleUnit* NowActive = Mgr->GetActiveUnit();
	BroadcastBattleHUDStatus(FString::Printf(TEXT("已结束 %s 回合，下一位: %s"),
		*GetUnitCode(CurrentActive), *GetUnitCode(NowActive)));

	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("ended_unit"), GetUnitCode(CurrentActive));
	Res->SetStringField(TEXT("next_active_unit"), GetUnitCode(NowActive));
	Res->SetStringField(TEXT("next_active_team"), NowActive ? (NowActive->Team == EBattleTeam::Player ? TEXT("Player") : TEXT("Enemy")) : TEXT("None"));
	return JsonObjectToString(Res);
}

int32 UBattleAICommandService::PickBestOffensiveSkill(ABattleUnit* Caster, ABattleUnit* Target, bool bRequireUsableNow) const
{
	if (!Caster || !Target)
	{
		return INDEX_NONE;
	}

	const TArray<FBattleSkillSpec>& Specs = Caster->GetSkillSpecs();
	const float Dist = FVector::Dist(Caster->GetActorLocation(), Target->GetActorLocation());
	int32 BestIdx = INDEX_NONE;
	float BestScore = -1.0f;

	for (int32 i = 0; i < Specs.Num(); ++i)
	{
		const UBattleSkillData* Skill = Specs[i].SkillData;
		if (!Skill || Skill->TacticalRole != ESkillTacticalRole::Offensive)
		{
			continue;
		}
		if (Specs[i].IsOnCooldown())
		{
			continue;
		}
		if (bRequireUsableNow)
		{
			if (Caster->AP < Skill->APCost)
			{
				continue;
			}
			if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
			{
				if (!Skill->AoE.IsArea() || Dist > Skill->AoE.Radius * 0.92f)
				{
					continue;
				}
			}
			FString Reason;
			ABattleUnit* TestTarget = (Skill->TargetFilter == ESkillTargetFilter::SelfOnly) ? Caster : Target;
			if (!Caster->CanUseSkill(i, TestTarget, Reason))
			{
				continue;
			}
		}

		const float Score = Skill->GetEstimatedTotalDamage() / (float)FMath::Max(1, Skill->APCost);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIdx = i;
		}
	}

	return BestIdx;
}

float UBattleAICommandService::ComputeDesiredEngageDistance(ABattleUnit* Caster, ABattleUnit* Target) const
{
	if (!Caster)
	{
		return 170.0f;
	}
	const TArray<FBattleSkillSpec>& Specs = Caster->GetSkillSpecs();
	float BestDist = 170.0f;
	float BestDmg = -1.0f;

	for (int32 i = 0; i < Specs.Num(); ++i)
	{
		const UBattleSkillData* Skill = Specs[i].SkillData;
		if (!Skill || Skill->TacticalRole != ESkillTacticalRole::Offensive || Specs[i].IsOnCooldown())
		{
			continue;
		}
		const float Dmg = Skill->GetEstimatedTotalDamage();
		if (Dmg > BestDmg)
		{
			BestDmg = Dmg;
			if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly && Skill->AoE.IsArea())
			{
				BestDist = FMath::Clamp(Skill->AoE.Radius * 0.72f, 150.0f, 350.0f);
			}
			else if (Skill->Range >= 450.0f)
			{
				BestDist = FMath::Clamp(Skill->Range * 0.80f, 200.0f, 900.0f);
			}
			else
			{
				BestDist = FMath::Clamp(Skill->Range * 0.70f, 140.0f, 210.0f);
			}
		}
	}
	return BestDist;
}

FVector UBattleAICommandService::ChooseSmartMoveDestination(ABattleUnit* Caster, ABattleUnit* Target, float DesiredDistCm, bool bCheckLOS) const
{
	const FVector TargetLoc = Target->GetActorLocation();
	FVector DirToSelf = (Caster->GetActorLocation() - TargetLoc).GetSafeNormal2D();
	if (DirToSelf.IsNearlyZero())
	{
		DirToSelf = FVector(1.0f, 0.0f, 0.0f);
	}

	UBattleTerrainQuery* Terrain = bCheckLOS ? UBattleTerrainQuery::Get(GetWorld()) : nullptr;
	static const float AngleOffsetsDeg[] = { 0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };

	FVector BestFallback = TargetLoc;
	bool bHaveAnyValid = false;

	for (float AngleDeg : AngleOffsetsDeg)
	{
		const FVector RotDir = DirToSelf.RotateAngleAxis(AngleDeg, FVector::UpVector);
		const FVector Candidate = TargetLoc + RotDir * DesiredDistCm;
		TArray<FVector> Path;
		int32 Cost = 0;
		bool bTruncated = false;
		if (Caster->PlanMove(Candidate, Path, Cost, bTruncated) && Path.Num() >= 2)
		{
			if (!bHaveAnyValid)
			{
				BestFallback = Candidate;
				bHaveAnyValid = true;
			}
			if (Terrain)
			{
				const FVector EndEye = Path.Last() + FVector(0.0f, 0.0f, 120.0f);
				const FVector TargetEye = TargetLoc + FVector(0.0f, 0.0f, 40.0f);
				if (Terrain->EvaluateLineOfSight(EndEye, TargetEye))
				{
					return Candidate;
				}
			}
			else
			{
				return Candidate;
			}
		}
	}

	return BestFallback;
}

void UBattleAICommandService::ReturnMoveOrSkillResultAfterAutoEndCheck(ABattleUnit* ActedUnit, const FString& ResultStatus, const FString& ExtraNote)
{
	UBattleManager* Mgr = GetBattleManager();
	if (ActedUnit && ActedUnit->IsAlive() && ActedUnit->AP <= 0
		&& Mgr && Mgr->GetActiveUnit() == ActedUnit && Mgr->GetBattleState() == EBattleState::TurnActive)
	{
		// ABattleUnit::TryAutoEndTurn() 在 0.45s 后推进回合；等待 0.60s 后再返给模型，避免双端同时结束回合误跳下一位
		PendingActionUnit = ActedUnit;
		PendingReturnStatus = ResultStatus;
		PendingReturnNote = ExtraNote;
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAutoEndSettleCheck, 0.60f, false);
			return;
		}
	}

	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("status"), ResultStatus);
	if (!ExtraNote.IsEmpty()) Res->SetStringField(TEXT("note"), ExtraNote);
	if (ActedUnit)
	{
		Res->SetStringField(TEXT("unit_acted"), GetUnitCode(ActedUnit));
		Res->SetNumberField(TEXT("remaining_ap"), ActedUnit->AP);
	}
	Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
	FinishToolCallAndContinue(JsonObjectToString(Res));
}

void UBattleAICommandService::HandleAutoEndSettleCheck()
{
	ABattleUnit* ActedUnit = PendingActionUnit.Get();
	const FString Status = PendingReturnStatus;
	const FString Note = PendingReturnNote;
	CleanupAsyncWait();

	UBattleManager* Mgr = GetBattleManager();
	const bool bAutoEnded = (Mgr && Mgr->GetActiveUnit() != ActedUnit);

	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("status"), Status);
	if (!Note.IsEmpty()) Res->SetStringField(TEXT("note"), Note);
	Res->SetStringField(TEXT("unit_acted"), GetUnitCode(ActedUnit));
	Res->SetBoolField(TEXT("turn_auto_ended"), bAutoEnded);
	Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
	FinishToolCallAndContinue(JsonObjectToString(Res));
}

void UBattleAICommandService::StartAsyncAdvanceAndAttack(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前战斗状态不允许行动"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || !Active->IsAlive() || Active->Team != EBattleTeam::Player)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前不是我方行动回合，若为敌方回合请调用 wait_for_player_turn"));
		Err->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	FString TargetId;
	Args->TryGetStringField(TEXT("target_id"), TargetId);
	ABattleUnit* Target = ResolveUnitRef(TargetId);
	if (!Target || !Target->IsAlive())
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), FString::Printf(TEXT("目标 %s 已阵亡或不存在"), *TargetId));
		Err->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	int32 PreferredSkill = INDEX_NONE;
	double RawIdx = -1.0;
	if (Args->TryGetNumberField(TEXT("skill_index"), RawIdx))
	{
		PreferredSkill = FMath::RoundToInt(RawIdx);
	}
	bool bEndTurnIfDone = true;
	Args->TryGetBoolField(TEXT("end_turn_if_done"), bEndTurnIfDone);

	// 1. 先检查当前是否已能直接施法攻击
	int32 ChosenSkill = INDEX_NONE;
	if (PreferredSkill >= 0)
	{
		FString Reason;
		const TArray<FBattleSkillSpec>& Specs = Active->GetSkillSpecs();
		if (Specs.IsValidIndex(PreferredSkill) && Specs[PreferredSkill].SkillData)
		{
			ABattleUnit* TestTarget = (Specs[PreferredSkill].SkillData->TargetFilter == ESkillTargetFilter::SelfOnly) ? Active : Target;
			if (Active->CanUseSkill(PreferredSkill, TestTarget, Reason))
			{
				ChosenSkill = PreferredSkill;
			}
		}
	}
	if (ChosenSkill == INDEX_NONE)
	{
		ChosenSkill = PickBestOffensiveSkill(Active, Target, true);
	}

	if (ChosenSkill != INDEX_NONE)
	{
		const UBattleSkillData* SD = Active->GetSkillSpecs()[ChosenSkill].SkillData;
		ABattleUnit* CastTarget = (SD && SD->TargetFilter == ESkillTargetFilter::SelfOnly) ? Active : Target;
		if (ABattlePlayerController* PC = GetBattlePC())
		{
			PC->FocusOnUnit(Active);
		}
		if (Mgr->RequestSkill(Active, ChosenSkill, CastTarget))
		{
			BroadcastBattleHUDStatus(FString::Printf(TEXT("指挥 %s (%s) 对 %s 施放 [%s]"),
				*GetUnitCode(Active), *Active->Stats.DisplayName, *GetUnitCode(Target),
				SD ? *SD->DisplayName.ToString() : TEXT("技能")));

			CleanupAsyncWait();
			PendingActionUnit = Active;
			PendingAdvanceTarget = Target;
			PendingPreferredSkillIndex = PreferredSkill;
			bPendingEndTurnIfDone = bEndTurnIfDone;
			ActiveAsyncToolName = TEXT("advance_and_attack");
			if (UWorld* W = GetWorld())
			{
				W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAdvanceAttackSkillSettled, 0.95f, false);
			}
			else
			{
				HandleAdvanceAttackSkillSettled();
			}
			return;
		}
	}

	// 2. 无法直接打到 -> 朝目标智能走位
	if (Active->AP <= 0)
	{
		if (bEndTurnIfDone)
		{
			Mgr->EndCurrentTurn();
		}
		ReturnMoveOrSkillResultAfterAutoEndCheck(Active, TEXT("no_ap_ended"));
		return;
	}

	const float DesiredDist = ComputeDesiredEngageDistance(Active, Target);
	const FVector ChosenDest = ChooseSmartMoveDestination(Active, Target, DesiredDist, true);

	if (ABattlePlayerController* PC = GetBattlePC())
	{
		PC->FocusOnUnit(Active);
	}

	if (!Mgr->RequestMove(Active, ChosenDest))
	{
		if (bEndTurnIfDone && Mgr->GetActiveUnit() == Active)
		{
			Mgr->EndCurrentTurn();
		}
		ReturnMoveOrSkillResultAfterAutoEndCheck(Active, TEXT("move_blocked_ended"), TEXT("无法寻路靠近目标"));
		return;
	}

	BroadcastBattleHUDStatus(FString::Printf(TEXT("指挥 %s (%s) 向 %s (%s) 机动推进"),
		*GetUnitCode(Active), *Active->Stats.DisplayName, *GetUnitCode(Target), *Target->Stats.DisplayName));

	CleanupAsyncWait();
	ObservedMoveUnit = Active;
	PendingActionUnit = Active;
	PendingAdvanceTarget = Target;
	PendingPreferredSkillIndex = PreferredSkill;
	bPendingEndTurnIfDone = bEndTurnIfDone;
	ActiveAsyncToolName = TEXT("advance_and_attack");
	Active->OnMoveFinished.AddDynamic(this, &UBattleAICommandService::HandleObservedUnitMoveFinished);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAsyncWaitTimeout, 7.0f, false);
	}
}

void UBattleAICommandService::HandleAdvanceAttackMoveFinished(ABattleUnit* Unit)
{
	UBattleManager* Mgr = GetBattleManager();
	ABattleUnit* Target = PendingAdvanceTarget.Get();
	const int32 PreferredSkill = PendingPreferredSkillIndex;
	const bool bEndTurnIfDone = bPendingEndTurnIfDone;
	PendingAdvanceTarget.Reset();
	PendingPreferredSkillIndex = INDEX_NONE;
	bPendingEndTurnIfDone = false;

	if (!Mgr || Mgr->GetBattleState() == EBattleState::BattleOver || !Unit || !Unit->IsAlive() || Mgr->GetActiveUnit() != Unit)
	{
		ReturnMoveOrSkillResultAfterAutoEndCheck(Unit, TEXT("advance_finished"));
		return;
	}

	// 移动后若仍有 AP 且目标存活, 立即尝试接技能 (省去一次 LLM HTTP 往返)
	if (Target && Target->IsAlive() && Unit->AP > 0)
	{
		int32 ChosenSkill = INDEX_NONE;
		if (PreferredSkill >= 0)
		{
			FString Reason;
			const TArray<FBattleSkillSpec>& Specs = Unit->GetSkillSpecs();
			if (Specs.IsValidIndex(PreferredSkill) && Specs[PreferredSkill].SkillData)
			{
				ABattleUnit* TestTarget = (Specs[PreferredSkill].SkillData->TargetFilter == ESkillTargetFilter::SelfOnly) ? Unit : Target;
				if (Unit->CanUseSkill(PreferredSkill, TestTarget, Reason))
				{
					ChosenSkill = PreferredSkill;
				}
			}
		}
		if (ChosenSkill == INDEX_NONE)
		{
			ChosenSkill = PickBestOffensiveSkill(Unit, Target, true);
		}

		if (ChosenSkill != INDEX_NONE)
		{
			const UBattleSkillData* SD = Unit->GetSkillSpecs()[ChosenSkill].SkillData;
			ABattleUnit* CastTarget = (SD && SD->TargetFilter == ESkillTargetFilter::SelfOnly) ? Unit : Target;
			if (Mgr->RequestSkill(Unit, ChosenSkill, CastTarget))
			{
				BroadcastBattleHUDStatus(FString::Printf(TEXT("%s 到位后立即施放 [%s] 攻击 %s"),
					*GetUnitCode(Unit), SD ? *SD->DisplayName.ToString() : TEXT("技能"), *GetUnitCode(Target)));

				PendingActionUnit = Unit;
				PendingAdvanceTarget = Target;
				PendingPreferredSkillIndex = PreferredSkill;
				bPendingEndTurnIfDone = bEndTurnIfDone;
				ActiveAsyncToolName = TEXT("advance_and_attack");
				if (UWorld* W = GetWorld())
				{
					W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAdvanceAttackSkillSettled, 0.95f, false);
					return;
				}
			}
		}
	}

	if (bEndTurnIfDone && Unit->AP > 0 && Mgr->GetActiveUnit() == Unit && Mgr->GetBattleState() == EBattleState::TurnActive)
	{
		Mgr->EndCurrentTurn();
		ReturnMoveOrSkillResultAfterAutoEndCheck(Unit, TEXT("moved_and_ended_turn"), TEXT("已尽最大航程推进，仍未进入射程，结束本单位回合"));
		return;
	}

	ReturnMoveOrSkillResultAfterAutoEndCheck(Unit, TEXT("moved_only"));
}

void UBattleAICommandService::HandleAdvanceAttackSkillSettled()
{
	ABattleUnit* Active = PendingActionUnit.Get();
	ABattleUnit* Target = PendingAdvanceTarget.Get();
	const bool bEndTurnIfDone = bPendingEndTurnIfDone;
	CleanupAsyncWait();

	UBattleManager* Mgr = GetBattleManager();
	if (Mgr && Mgr->GetBattleState() == EBattleState::TurnActive
		&& Active && Active->IsAlive() && Mgr->GetActiveUnit() == Active
		&& Target && Target->IsAlive() && Active->AP > 0)
	{
		// 若还有 AP 且能再打一发, 自动连击
		const int32 FollowUpSkill = PickBestOffensiveSkill(Active, Target, true);
		if (FollowUpSkill != INDEX_NONE)
		{
			const UBattleSkillData* SD = Active->GetSkillSpecs()[FollowUpSkill].SkillData;
			ABattleUnit* CastTarget = (SD && SD->TargetFilter == ESkillTargetFilter::SelfOnly) ? Active : Target;
			if (Mgr->RequestSkill(Active, FollowUpSkill, CastTarget))
			{
				BroadcastBattleHUDStatus(FString::Printf(TEXT("%s 追加连击 [%s]"),
					*GetUnitCode(Active), SD ? *SD->DisplayName.ToString() : TEXT("技能")));
				PendingActionUnit = Active;
				PendingAdvanceTarget = Target;
				bPendingEndTurnIfDone = bEndTurnIfDone;
				ActiveAsyncToolName = TEXT("advance_and_attack");
				if (UWorld* W = GetWorld())
				{
					W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAdvanceAttackSkillSettled, 0.95f, false);
					return;
				}
			}
		}

		if (bEndTurnIfDone)
		{
			Mgr->EndCurrentTurn();
		}
	}

	const FString Note = (Target && !Target->IsAlive()) ? TEXT("目标已被击毁！") : FString();
	ReturnMoveOrSkillResultAfterAutoEndCheck(Active, TEXT("attack_settled"), Note);
}

void UBattleAICommandService::StartAsyncWaitForPlayerTurn()
{
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("BattleManager 无效"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (Mgr->GetBattleState() == EBattleState::BattleOver
		|| (Mgr->GetBattleState() == EBattleState::TurnActive && Active && Active->Team == EBattleTeam::Player && Active->IsAlive()))
	{
		TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
		Res->SetBoolField(TEXT("ok"), true);
		Res->SetStringField(TEXT("status"), Mgr->GetBattleState() == EBattleState::BattleOver ? TEXT("battle_over") : TEXT("player_turn_ready"));
		Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
		FinishToolCallAndContinue(JsonObjectToString(Res));
		return;
	}

	CleanupAsyncWait();
	bWaitingForPlayerTurn = true;
	ActiveAsyncToolName = TEXT("wait_for_player_turn");
	SetBusyState(true, TEXT("等待敌方回合..."));
	BroadcastBattleHUDStatus(TEXT("等待敌方单位行动结束..."));

	Mgr->OnTurnStarted.AddDynamic(this, &UBattleAICommandService::HandleWaitTurnStarted);
	Mgr->OnBattleFinished.AddDynamic(this, &UBattleAICommandService::HandleWaitBattleFinished);

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(WaitTurnPollTimerHandle, this, &UBattleAICommandService::HandleWaitTurnPoll, 0.25f, true);
		W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAsyncWaitTimeout, 40.0f, false);
	}
}

void UBattleAICommandService::HandleWaitTurnPoll()
{
	if (!bWaitingForPlayerTurn)
	{
		return;
	}

	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr)
	{
		CleanupAsyncWait();
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("BattleManager 丢失"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	if (Mgr->GetBattleState() == EBattleState::BattleOver)
	{
		CleanupAsyncWait();
		TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
		Res->SetBoolField(TEXT("ok"), true);
		Res->SetStringField(TEXT("status"), TEXT("battle_over"));
		Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
		FinishToolCallAndContinue(JsonObjectToString(Res));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (Mgr->GetBattleState() == EBattleState::TurnActive && Active && Active->Team == EBattleTeam::Player && Active->IsAlive())
	{
		CleanupAsyncWait();
		TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
		Res->SetBoolField(TEXT("ok"), true);
		Res->SetStringField(TEXT("status"), TEXT("player_turn_ready"));
		Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
		FinishToolCallAndContinue(JsonObjectToString(Res));
	}
}

void UBattleAICommandService::StartAsyncMoveTowardsUnit(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前战斗状态不允许移动"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || !Active->IsAlive() || Active->Team != EBattleTeam::Player)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前行动单位不是存活的我方棋子"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	FString TargetId;
	Args->TryGetStringField(TEXT("target_id"), TargetId);
	ABattleUnit* Target = ResolveUnitRef(TargetId);
	if (!Target || !Target->IsAlive())
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), FString::Printf(TEXT("未找到存活目标: %s"), *TargetId));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	double DesiredDist = 160.0;
	Args->TryGetNumberField(TEXT("desired_distance_cm"), DesiredDist);
	DesiredDist = FMath::Clamp(DesiredDist, 120.0, 1200.0);

	const float CurrentDist = FVector::Dist(Active->GetActorLocation(), Target->GetActorLocation());
	if (CurrentDist <= (float)DesiredDist + 20.0f)
	{
		TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
		Ok->SetBoolField(TEXT("ok"), true);
		Ok->SetStringField(TEXT("status"), TEXT("already_in_range"));
		Ok->SetNumberField(TEXT("distance_cm"), FMath::RoundToInt(CurrentDist));
		Ok->SetNumberField(TEXT("remaining_ap"), Active->AP);
		FinishToolCallAndContinue(JsonObjectToString(Ok));
		return;
	}

	if (Active->AP <= 0)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前 AP 为 0，无法移动"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	const FVector ChosenDest = ChooseSmartMoveDestination(Active, Target, (float)DesiredDist, true);

	if (ABattlePlayerController* PC = GetBattlePC())
	{
		PC->FocusOnUnit(Active);
	}

	if (!Mgr->RequestMove(Active, ChosenDest))
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("导航寻路失败或 AP 不足以移动到目标附近"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	BroadcastBattleHUDStatus(FString::Printf(TEXT("指挥 %s (%s) 向 %s (%s) 机动靠近"),
		*GetUnitCode(Active), *Active->Stats.DisplayName, *GetUnitCode(Target), *Target->Stats.DisplayName));

	CleanupAsyncWait();
	ObservedMoveUnit = Active;
	ActiveAsyncToolName = TEXT("move_towards_unit");
	Active->OnMoveFinished.AddDynamic(this, &UBattleAICommandService::HandleObservedUnitMoveFinished);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAsyncWaitTimeout, 7.0f, false);
	}
}

void UBattleAICommandService::StartAsyncMoveToPosition(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前战斗状态不允许移动"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || !Active->IsAlive() || Active->Team != EBattleTeam::Player)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前行动单位不是存活的我方棋子"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	double X = 0.0, Y = 0.0, Z = 0.0;
	Args->TryGetNumberField(TEXT("x"), X);
	Args->TryGetNumberField(TEXT("y"), Y);
	Args->TryGetNumberField(TEXT("z"), Z);

	if (!Mgr->RequestMove(Active, FVector((float)X, (float)Y, (float)Z)))
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("无法规划到该坐标的导航路径或 AP 不足"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	CleanupAsyncWait();
	ObservedMoveUnit = Active;
	ActiveAsyncToolName = TEXT("move_to_position");
	Active->OnMoveFinished.AddDynamic(this, &UBattleAICommandService::HandleObservedUnitMoveFinished);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleAsyncWaitTimeout, 7.0f, false);
	}
}

void UBattleAICommandService::StartAsyncUseSkill(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
	UBattleManager* Mgr = GetBattleManager();
	if (!Mgr || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前战斗状态不允许施放技能"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	if (!Active || !Active->IsAlive() || Active->Team != EBattleTeam::Player)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("当前行动单位不是存活的我方棋子"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	int32 SkillIndex = INDEX_NONE;
	double RawIndex = -1.0;
	if (Args->TryGetNumberField(TEXT("skill_index"), RawIndex))
	{
		SkillIndex = FMath::RoundToInt(RawIndex);
	}

	FString SkillIdStr;
	Args->TryGetStringField(TEXT("skill_id"), SkillIdStr);
	SkillIdStr.TrimStartAndEndInline();

	const TArray<FBattleSkillSpec>& Specs = Active->GetSkillSpecs();
	if (!SkillIdStr.IsEmpty() && (!Specs.IsValidIndex(SkillIndex) ||
		(Specs[SkillIndex].SkillData && !Specs[SkillIndex].SkillData->SkillId.ToString().Equals(SkillIdStr, ESearchCase::IgnoreCase)
		 && !Specs[SkillIndex].SkillData->DisplayName.ToString().Equals(SkillIdStr, ESearchCase::IgnoreCase))))
	{
		for (int32 i = 0; i < Specs.Num(); ++i)
		{
			const UBattleSkillData* SD = Specs[i].SkillData;
			if (SD && (SD->SkillId.ToString().Equals(SkillIdStr, ESearchCase::IgnoreCase)
				|| SD->DisplayName.ToString().Equals(SkillIdStr, ESearchCase::IgnoreCase)))
			{
				SkillIndex = i;
				break;
			}
		}
	}

	if (!Specs.IsValidIndex(SkillIndex) || !Specs[SkillIndex].SkillData)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("无效的技能索引或 skill_id"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	const UBattleSkillData* Skill = Specs[SkillIndex].SkillData;
	FString TargetId;
	Args->TryGetStringField(TEXT("target_id"), TargetId);
	ABattleUnit* Target = (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
		? Active
		: ResolveUnitRef(TargetId);

	if (!Target)
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), FString::Printf(TEXT("未找到目标单位: %s"), *TargetId));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	FString FailReason;
	if (!Active->CanUseSkill(SkillIndex, Target, FailReason))
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), FailReason);
		Err->SetNumberField(TEXT("distance_cm"), FMath::RoundToInt(FVector::Dist(Active->GetActorLocation(), Target->GetActorLocation())));
		Err->SetNumberField(TEXT("skill_range_cm"), Skill->Range);
		Err->SetNumberField(TEXT("unit_ap"), Active->AP);
		Err->SetNumberField(TEXT("skill_ap_cost"), Skill->APCost);
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	if (!Mgr->RequestSkill(Active, SkillIndex, Target))
	{
		Err->SetBoolField(TEXT("ok"), false);
		Err->SetStringField(TEXT("reason"), TEXT("技能施放请求被 BattleManager 拒绝"));
		FinishToolCallAndContinue(JsonObjectToString(Err));
		return;
	}

	if (ABattlePlayerController* PC = GetBattlePC())
	{
		PC->CancelCurrentAction();
	}

	CleanupAsyncWait();
	PendingActionUnit = Active;
	ActiveAsyncToolName = TEXT("use_skill");
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(AsyncWaitTimerHandle, this, &UBattleAICommandService::HandleSkillSettleDone, 0.95f, false);
	}
	else
	{
		HandleSkillSettleDone();
	}
}

void UBattleAICommandService::HandleSkillSettleDone()
{
	ABattleUnit* ActedUnit = PendingActionUnit.Get();
	CleanupAsyncWait();
	ReturnMoveOrSkillResultAfterAutoEndCheck(ActedUnit, TEXT("skill_settled"));
}

void UBattleAICommandService::HandleAsyncWaitTimeout()
{
	const FString ToolName = ActiveAsyncToolName;
	CleanupAsyncWait();

	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	Res->SetBoolField(TEXT("ok"), true);
	Res->SetStringField(TEXT("status"), FString::Printf(TEXT("%s_wait_timeout"), *ToolName));
	Res->SetObjectField(TEXT("battle_snapshot"), BuildBattlefieldSnapshotJson());
	FinishToolCallAndContinue(JsonObjectToString(Res));
}

void UBattleAICommandService::CleanupAsyncWait()
{
	if (ABattleUnit* Unit = ObservedMoveUnit.Get())
	{
		Unit->OnMoveFinished.RemoveDynamic(this, &UBattleAICommandService::HandleObservedUnitMoveFinished);
	}
	if (bWaitingForPlayerTurn)
	{
		if (UBattleManager* Mgr = GetBattleManager())
		{
			Mgr->OnTurnStarted.RemoveDynamic(this, &UBattleAICommandService::HandleWaitTurnStarted);
			Mgr->OnBattleFinished.RemoveDynamic(this, &UBattleAICommandService::HandleWaitBattleFinished);
		}
	}

	ObservedMoveUnit.Reset();
	PendingActionUnit.Reset();
	PendingAdvanceTarget.Reset();
	PendingPreferredSkillIndex = INDEX_NONE;
	bPendingEndTurnIfDone = false;
	bWaitingForPlayerTurn = false;
	PendingReturnStatus.Reset();
	PendingReturnNote.Reset();
	ActiveAsyncToolName.Reset();

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(AsyncWaitTimerHandle);
		W->GetTimerManager().ClearTimer(WaitTurnPollTimerHandle);
	}
}

bool UBattleAICommandService::TryParseFallbackToolCallFromContent(const FString& Content, TArray<FPendingToolCall>& OutCalls) const
{
	const int32 BraceStart = Content.Find(TEXT("{"));
	const int32 BraceEnd = Content.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (BraceStart == INDEX_NONE || BraceEnd == INDEX_NONE || BraceEnd <= BraceStart)
	{
		return false;
	}

	const FString JsonSlice = Content.Mid(BraceStart, BraceEnd - BraceStart + 1);
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonSlice);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		return false;
	}

	FString ToolName;
	if (!Obj->TryGetStringField(TEXT("tool"), ToolName) && !Obj->TryGetStringField(TEXT("name"), ToolName))
	{
		return false;
	}

	FPendingToolCall Call;
	Call.Name = ToolName;
	const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
	if (Obj->TryGetObjectField(TEXT("arguments"), ArgsObj) || Obj->TryGetObjectField(TEXT("args"), ArgsObj))
	{
		if (ArgsObj && (*ArgsObj).IsValid())
		{
			Call.ArgumentsJson = JsonObjectToString(*ArgsObj);
		}
	}
	if (Call.ArgumentsJson.IsEmpty())
	{
		Call.ArgumentsJson = TEXT("{}");
	}
	OutCalls.Add(Call);
	return true;
}

FString UBattleAICommandService::JsonObjectToString(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid())
	{
		return TEXT("{}");
	}
	FString OutStr;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutStr;
}

// =============================================================================
// 大兵团作战 (Lvl_Legion) AI 战术指挥实现
// =============================================================================

namespace
{
	const TCHAR* LegionStateToString(ELegionState St)
	{
		switch (St)
		{
		case ELegionState::Holding: return TEXT("驻守");
		case ELegionState::Moving:  return TEXT("机动");
		case ELegionState::Engaged: return TEXT("交战");
		case ELegionState::Wiped:   return TEXT("覆灭");
		default:                    return TEXT("未知");
		}
	}

	/** 从中文/数字编号提取 1-based 旅序号 (支持 "第一旅" / "敌第一旅" / "对方第1旅" / "E1" / "1") */
	int32 TryParseOrdinal(const FString& Text)
	{
		static const TCHAR* CN[10] = { TEXT("一"), TEXT("二"), TEXT("三"), TEXT("四"), TEXT("五"),
			TEXT("六"), TEXT("七"), TEXT("八"), TEXT("九"), TEXT("十") };
		for (int32 i = 0; i < 10; ++i)
		{
			if (Text.Contains(CN[i])) { return i + 1; }
		}
		FString Digits;
		for (TCHAR Ch : Text)
		{
			if (FChar::IsDigit(Ch)) { Digits.AppendChar(Ch); }
			else if (!Digits.IsEmpty()) { break; }
		}
		if (!Digits.IsEmpty())
		{
			const int32 V = FCString::Atoi(*Digits);
			return V > 0 ? V : INDEX_NONE;
		}
		return INDEX_NONE;
	}

	bool IsAllToken(const FString& Ref)
	{
		const FString U = Ref.TrimStartAndEnd().ToUpper();
		return U == TEXT("ALL") || U == TEXT("*") || U.Contains(TEXT("全部"))
			|| U.Contains(TEXT("全军")) || U.Contains(TEXT("全体")) || U.Contains(TEXT("所有"));
	}
}

FString UBattleAICommandService::GetLegionCode(const ALegionUnit* Unit) const
{
	const ALegionGameMode* GM = GetLegionGameMode();
	if (!GM || !Unit) { return TEXT("?"); }

	const TArray<TObjectPtr<ALegionUnit>>& Players = GM->GetPlayerLegions();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (Players[i].Get() == Unit) { return FString::Printf(TEXT("P%d"), i + 1); }
	}
	const TArray<TObjectPtr<ALegionUnit>>& Enemies = GM->GetEnemyLegions();
	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		if (Enemies[i].Get() == Unit) { return FString::Printf(TEXT("E%d"), i + 1); }
	}
	return TEXT("?");
}

FString UBattleAICommandService::GetLegionUnitIndexSummary() const
{
	const ALegionGameMode* GM = GetLegionGameMode();
	if (!GM) { return TEXT("战场未就绪"); }

	FString AllyStr;
	const TArray<TObjectPtr<ALegionUnit>>& Players = GM->GetPlayerLegions();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const ALegionUnit* L = Players[i].Get();
		if (!L) { continue; }
		if (!AllyStr.IsEmpty()) { AllyStr += TEXT("  "); }
		AllyStr += FString::Printf(TEXT("P%d %s(%s %d/%d)"),
			i + 1, *L->GetCallsign(), *ERGalacticMeta::GetClassCode(L->GetArchetype()),
			L->GetAliveCount(), L->GetSoldierCount());
	}

	FString EnemyStr;
	const TArray<TObjectPtr<ALegionUnit>>& Enemies = GM->GetEnemyLegions();
	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		const ALegionUnit* L = Enemies[i].Get();
		if (!L) { continue; }
		if (!EnemyStr.IsEmpty()) { EnemyStr += TEXT("  "); }
		EnemyStr += FString::Printf(TEXT("E%d %s(%s %d/%d)"),
			i + 1, *L->GetCallsign(), *ERGalacticMeta::GetClassCode(L->GetArchetype()),
			L->GetAliveCount(), L->GetSoldierCount());
	}

	return FString::Printf(TEXT("上下文: %d 轮\n我方: %s\n敌方: %s"),
		ContextTurnCount,
		AllyStr.IsEmpty() ? TEXT("无") : *AllyStr,
		EnemyStr.IsEmpty() ? TEXT("无") : *EnemyStr);
}

FString UBattleAICommandService::BuildLegionSystemPrompt() const
{
	FString Prompt = TEXT(
		"你要扮演科幻大兵团军事指挥游戏《虹不我与 (Ebbing Rainbow)》中的实时战术参谋 AI。\n"
		"玩家正在等高线战略地图视角下向你下达自然语言军事命令（地图开启时战场处于暂停态，你调用工具下达的所有行军、集火、停驻、跟随命令都会立刻在地图上画出路线与箭头，玩家返回战场后部队立刻执行）。\n\n"
		"【编制与兵种克制】\n"
		"1. 我方部队编号 P1, P2, P3...，称谓为「第一旅、第二旅、第三旅、第四旅...」。\n"
		"2. 敌方部队编号 E1, E2, E3...，称谓为「敌第一旅 / 帝国第一旅 / 叛军第一旅 / 自治领第一旅...」。\n"
		"3. 四大兵种克制：\n"
		"   - 重盾卫队(重盾)：正面高额格挡抗线，惧侧背突袭；\n"
		"   - 长戟方阵(长戟)：驻守反制近战突击并击退，惧远程火力；\n"
		"   - 突击刃兵(刃兵)：高机动近战破阵，适合绕后包抄切敌方射手或盾兵侧背；\n"
		"   - 精确射手(射手)：远程齐射，占据高地可显著增加射程与伤害，林地可提供远程掩体，怕被贴身。\n\n"
		"【坐标与方位】\n"
		"地图画面上 +Y 为北方（敌方初始防线方向），-Y 为南方（我方初始防线方向），+X 为西（地图左侧），-X 为东（地图右侧），单位为厘米（100 cm = 1 米）。\n\n"
		"【可用工具】\n"
		"1. `order_attack(ally_ids, target_id)`：令指定我方部队集火进攻指定敌军。ally_ids 支持 [\"all\"] 表示全部存活我方部队，或传入 [\"P1\", \"第一旅\", \"重盾\"] 等；target_id 支持 \"E1\" / \"敌第一旅\" / \"对方第一旅\" / \"nearest\"。\n"
		"2. `order_all_assault()`：令全部我方存活部队就近自由突击各自最近的敌人。\n"
		"3. `order_move(ally_ids, x, y, facing_yaw_deg)`：令指定我方部队按阵型机动至世界坐标 (x, y)（自动贴合可通行地形），可选指定到达后的面向角度（度）。\n"
		"4. `order_move_relative(ally_ids, anchor_id, direction, distance_m)`：相对某参考目标（我方/敌方旅或 \"commander\" 指挥官）向指定战术方向（front/back/left/right/north/south/east/west/retreat）机动指定米数，非常适合侧翼包抄、绕后、后撤或向指挥官集结。\n"
		"5. `order_halt(ally_ids)`：令指定部队原地停驻待命。\n"
		"6. `order_follow_commander(ally_ids, enable)`：令指定部队跟随/取消跟随指挥官。\n"
		"7. `select_allies(ally_ids)`：在地图上高亮选中指定我方部队。\n"
		"8. `get_legion_battle_state()`：重新获取最新战场状态快照。\n\n"
		"【执行准则】\n"
		"- 收到明确作战指令（如“全部部队优先合力进攻对方第一旅”、“第一旅抗线，第三旅绕后切敌第四旅”）时，**直接一次性调用所需工具**，不要向玩家反复确认。\n"
		"- 可在同一轮连续调用多个工具完成分兵协同（例如同时下达前排集火、侧翼绕后机动、射手驻守高地）。\n"
		"- 工具执行完毕后，用简短、干练的中文军事口吻汇报部署结果（点明哪几支部队被下达了什么命令），严禁编造未执行的命令。"
	);

	if (!Config.CustomPromptExtra.IsEmpty())
	{
		Prompt += TEXT("\n\n【附加战术约束】\n") + Config.CustomPromptExtra;
	}
	return Prompt;
}

TSharedPtr<FJsonObject> UBattleAICommandService::BuildLegionSnapshotJson() const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	const ALegionGameMode* GM = GetLegionGameMode();
	if (!GM)
	{
		Root->SetStringField(TEXT("error"), TEXT("LegionGameMode not ready"));
		return Root;
	}

	const ALegionPlayerController* PC = GetLegionPC();
	Root->SetStringField(TEXT("mode"), TEXT("legion_battle"));
	Root->SetStringField(TEXT("battle_title"), GM->GetBattleTitleText());
	Root->SetBoolField(TEXT("battle_over"), GM->IsBattleOver());
	Root->SetBoolField(TEXT("paused"), GM->GetTacticalTimeMode() == ELegionTimeMode::Paused);
	Root->SetNumberField(TEXT("elapsed_sec"), FMath::RoundToFloat(GM->GetBattleElapsed() * 10.0f) / 10.0f);

	{
		TSharedPtr<FJsonObject> Bounds = MakeShared<FJsonObject>();
		const FVector C = GM->GetDeployCenter();
		Bounds->SetNumberField(TEXT("center_x"), FMath::RoundToInt(C.X));
		Bounds->SetNumberField(TEXT("center_y"), FMath::RoundToInt(C.Y));
		Bounds->SetNumberField(TEXT("half_extent_cm"), FMath::RoundToInt(GM->GetBattlefieldHalfExtent()));
		Root->SetObjectField(TEXT("map_bounds"), Bounds);
	}

	if (PC && PC->GetPawn())
	{
		const FVector P = PC->GetPawn()->GetActorLocation();
		TSharedPtr<FJsonObject> Cmd = MakeShared<FJsonObject>();
		Cmd->SetNumberField(TEXT("x"), FMath::RoundToInt(P.X));
		Cmd->SetNumberField(TEXT("y"), FMath::RoundToInt(P.Y));
		Cmd->SetNumberField(TEXT("z"), FMath::RoundToInt(P.Z));
		Cmd->SetNumberField(TEXT("yaw_deg"), FMath::RoundToInt(PC->GetControlRotation().Yaw));
		Root->SetObjectField(TEXT("commander"), Cmd);
	}

	const FLegionTerrainGrid& Grid = GM->GetTerrainGrid();
	auto MakeLegionJson = [this, PC, &Grid](const ALegionUnit* L, const FString& Code)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Code);
		Obj->SetStringField(TEXT("name"), L->GetCallsign());
		Obj->SetStringField(TEXT("class"), ERGalacticMeta::GetClassName(L->GetArchetype()));
		Obj->SetStringField(TEXT("class_code"), ERGalacticMeta::GetClassCode(L->GetArchetype()));
		Obj->SetBoolField(TEXT("alive"), !L->IsWiped());
		Obj->SetNumberField(TEXT("alive_soldiers"), L->GetAliveCount());
		Obj->SetNumberField(TEXT("max_soldiers"), L->GetSoldierCount());
		Obj->SetStringField(TEXT("state"), LegionStateToString(L->GetLegionState()));

		const FVector Loc = L->GetActorLocation();
		TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
		Pos->SetNumberField(TEXT("x"), FMath::RoundToInt(Loc.X));
		Pos->SetNumberField(TEXT("y"), FMath::RoundToInt(Loc.Y));
		Pos->SetNumberField(TEXT("z"), FMath::RoundToInt(Loc.Z));
		Obj->SetObjectField(TEXT("pos"), Pos);
		Obj->SetNumberField(TEXT("facing_yaw"), FMath::RoundToInt(L->GetFacingYaw()));

		if (const ALegionUnit* Tgt = L->GetAttackTarget())
		{
			Obj->SetStringField(TEXT("attack_target"), FString::Printf(TEXT("%s (%s)"), *GetLegionCode(Tgt), *Tgt->GetCallsign()));
		}
		if (L->HasMoveOrder())
		{
			const FVector D = L->GetMoveDestination();
			TSharedPtr<FJsonObject> Dest = MakeShared<FJsonObject>();
			Dest->SetNumberField(TEXT("x"), FMath::RoundToInt(D.X));
			Dest->SetNumberField(TEXT("y"), FMath::RoundToInt(D.Y));
			Obj->SetObjectField(TEXT("move_dest"), Dest);
		}
		if (L->GetTeam() == ELegionTeam::Player && PC)
		{
			Obj->SetBoolField(TEXT("selected"), PC->IsLegionSelected(L));
			Obj->SetBoolField(TEXT("following_commander"), PC->IsLegionFollowing(L));
		}
		if (Grid.IsBuilt())
		{
			Obj->SetBoolField(TEXT("in_forest_cover"), Grid.GetTreeCountWorld(Loc) > 0);
		}
		return Obj;
	};

	TArray<TSharedPtr<FJsonValue>> AlliesArr;
	const TArray<TObjectPtr<ALegionUnit>>& Players = GM->GetPlayerLegions();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (const ALegionUnit* L = Players[i].Get())
		{
			AlliesArr.Add(MakeShared<FJsonValueObject>(MakeLegionJson(L, FString::Printf(TEXT("P%d"), i + 1))));
		}
	}
	Root->SetArrayField(TEXT("allies"), AlliesArr);

	TArray<TSharedPtr<FJsonValue>> EnemiesArr;
	const TArray<TObjectPtr<ALegionUnit>>& Enemies = GM->GetEnemyLegions();
	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		if (const ALegionUnit* L = Enemies[i].Get())
		{
			EnemiesArr.Add(MakeShared<FJsonValueObject>(MakeLegionJson(L, FString::Printf(TEXT("E%d"), i + 1))));
		}
	}
	Root->SetArrayField(TEXT("enemies"), EnemiesArr);

	TArray<TSharedPtr<FJsonValue>> WavesArr;
	for (const FPendingEnemyWave& PW : GM->GetPendingWaves())
	{
		if (PW.bSpawned) { continue; }
		TSharedPtr<FJsonObject> WObj = MakeShared<FJsonObject>();
		WObj->SetStringField(TEXT("class"), ERGalacticMeta::GetClassName(PW.Spec.Archetype));
		WObj->SetNumberField(TEXT("soldiers"), PW.Spec.SoldierCount);
		WObj->SetNumberField(TEXT("eta_sec"), FMath::RoundToInt(FMath::Max(0.0f, PW.Spec.DelaySeconds - GM->GetBattleElapsed())));
		WavesArr.Add(MakeShared<FJsonValueObject>(WObj));
	}
	Root->SetArrayField(TEXT("pending_enemy_waves"), WavesArr);

	return Root;
}

ALegionUnit* UBattleAICommandService::ResolveLegionUnitRef(const FString& RefStr, bool bPreferEnemy) const
{
	const ALegionGameMode* GM = GetLegionGameMode();
	if (!GM) { return nullptr; }

	FString Clean = RefStr;
	Clean.TrimStartAndEndInline();
	if (Clean.IsEmpty()) { return nullptr; }

	const TArray<TObjectPtr<ALegionUnit>>& Players = GM->GetPlayerLegions();
	const TArray<TObjectPtr<ALegionUnit>>& Enemies = GM->GetEnemyLegions();

	// 1. 显式编号 P1 / E1
	const FString Upper = Clean.ToUpper();
	if (Upper.StartsWith(TEXT("P")) && Upper.Mid(1).IsNumeric())
	{
		const int32 Idx = FCString::Atoi(*Upper.Mid(1)) - 1;
		return Players.IsValidIndex(Idx) ? Players[Idx].Get() : nullptr;
	}
	if (Upper.StartsWith(TEXT("E")) && Upper.Mid(1).IsNumeric())
	{
		const int32 Idx = FCString::Atoi(*Upper.Mid(1)) - 1;
		return Enemies.IsValidIndex(Idx) ? Enemies[Idx].Get() : nullptr;
	}

	// 判断文本是否带有显式敌/我倾向
	bool bSearchEnemy = bPreferEnemy;
	if (Clean.Contains(TEXT("敌")) || Clean.Contains(TEXT("对方")) || Clean.Contains(TEXT("对面"))
		|| Clean.Contains(TEXT("帝国")) || Clean.Contains(TEXT("叛军")) || Clean.Contains(TEXT("自治领")))
	{
		bSearchEnemy = true;
	}
	else if (Clean.Contains(TEXT("我方")) || Clean.Contains(TEXT("友军")) || Clean.Contains(TEXT("己方")))
	{
		bSearchEnemy = false;
	}

	const TArray<TObjectPtr<ALegionUnit>>& Primary = bSearchEnemy ? Enemies : Players;
	const TArray<TObjectPtr<ALegionUnit>>& Secondary = bSearchEnemy ? Players : Enemies;

	auto FindInList = [&Clean](const TArray<TObjectPtr<ALegionUnit>>& List) -> ALegionUnit*
	{
		// 精确匹配 Callsign
		for (ALegionUnit* L : List)
		{
			if (L && L->GetCallsign().Equals(Clean, ESearchCase::IgnoreCase)) { return L; }
		}
		// 包含匹配 Callsign 或 Callsign 被包含 (如 "对方第一旅" 包含 "第一旅", "自治领第一旅" 含 "第一旅")
		for (ALegionUnit* L : List)
		{
			if (L && !L->GetCallsign().IsEmpty()
				&& (L->GetCallsign().Contains(Clean, ESearchCase::IgnoreCase) || Clean.Contains(L->GetCallsign(), ESearchCase::IgnoreCase)))
			{
				return L;
			}
		}
		// 按旅序号匹配 (一/二/三/四 或 1/2/3/4)
		const int32 Ord = TryParseOrdinal(Clean);
		if (Ord != INDEX_NONE && List.IsValidIndex(Ord - 1))
		{
			return List[Ord - 1].Get();
		}
		// 按兵种名/简称匹配 (优先存活)
		for (ALegionUnit* L : List)
		{
			if (!L || L->IsWiped()) { continue; }
			const FString ClsName = ERGalacticMeta::GetClassName(L->GetArchetype());
			const FString ClsCode = ERGalacticMeta::GetClassCode(L->GetArchetype());
			if (Clean.Contains(ClsCode) || ClsName.Contains(Clean))
			{
				return L;
			}
		}
		return nullptr;
	};

	if (ALegionUnit* Found = FindInList(Primary)) { return Found; }
	return FindInList(Secondary);
}

void UBattleAICommandService::ResolveAllyLegions(const TSharedPtr<FJsonObject>& Args, TArray<ALegionUnit*>& OutAllies) const
{
	OutAllies.Reset();
	const ALegionGameMode* GM = GetLegionGameMode();
	if (!GM) { return; }

	auto AddAllAlive = [&]()
	{
		for (ALegionUnit* L : GM->GetPlayerLegions())
		{
			if (L && !L->IsWiped()) { OutAllies.AddUnique(L); }
		}
	};

	auto ResolveOneToken = [&](const FString& Tok)
	{
		if (IsAllToken(Tok))
		{
			AddAllAlive();
			return;
		}
		// 若按兵种名 (如 "重盾"/"射手") 批量匹配全部同兵种存活部队
		for (ALegionUnit* L : GM->GetPlayerLegions())
		{
			if (!L || L->IsWiped()) { continue; }
			const FString ClsCode = ERGalacticMeta::GetClassCode(L->GetArchetype());
			const FString ClsName = ERGalacticMeta::GetClassName(L->GetArchetype());
			if (Tok.Equals(ClsCode) || Tok.Equals(ClsName))
			{
				OutAllies.AddUnique(L);
			}
		}
		if (OutAllies.Num() == 0)
		{
			if (ALegionUnit* One = ResolveLegionUnitRef(Tok, false))
			{
				if (One->GetTeam() == ELegionTeam::Player && !One->IsWiped())
				{
					OutAllies.AddUnique(One);
				}
			}
		}
	};

	if (!Args.IsValid())
	{
		AddAllAlive();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Args->TryGetArrayField(TEXT("ally_ids"), Arr) && Arr && Arr->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			FString S;
			if (V.IsValid() && V->TryGetString(S))
			{
				if (IsAllToken(S))
				{
					AddAllAlive();
					return;
				}
				if (ALegionUnit* One = ResolveLegionUnitRef(S, false))
				{
					if (One->GetTeam() == ELegionTeam::Player && !One->IsWiped())
					{
						OutAllies.AddUnique(One);
					}
				}
				else
				{
					ResolveOneToken(S);
				}
			}
		}
		return;
	}

	FString Single;
	if (Args->TryGetStringField(TEXT("ally_ids"), Single) || Args->TryGetStringField(TEXT("ally_id"), Single))
	{
		ResolveOneToken(Single);
		return;
	}

	// 未传 ally_ids 时默认全部存活我方部队
	AddAllAlive();
}

TArray<TSharedPtr<FJsonValue>> UBattleAICommandService::BuildLegionToolsJson(bool bAnthropic) const
{
	TArray<TSharedPtr<FJsonValue>> Tools;
	auto AddTool = [&](const FString& Name, const FString& Desc,
		const TMap<FString, TSharedPtr<FJsonObject>>& Props, const TArray<FString>& Req)
	{
		TSharedPtr<FJsonObject> Def = bAnthropic
			? MakeAnthropicToolDef(Name, Desc, Props, Req)
			: MakeOpenAIToolDef(Name, Desc, Props, Req);
		Tools.Add(MakeShared<FJsonValueObject>(Def));
	};

	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要执行进攻的我方部队列表，例如 [\"all\"] 表示全部存活部队，或 [\"P1\", \"第二旅\"]")));
		Props.Add(TEXT("target_id"), MakeJsonProp(TEXT("string"), TEXT("目标敌方部队编号或称谓，例如 \"E1\"、\"敌第一旅\"、\"对方第一旅\"、\"自治领第一旅\" 或 \"nearest\"")));
		AddTool(TEXT("order_attack"),
			TEXT("令指定我方部队（或全部部队）集火进攻指定敌方部队。命令会立即反映在战略地图上。"),
			Props, { TEXT("ally_ids"), TEXT("target_id") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		AddTool(TEXT("order_all_assault"),
			TEXT("令全部存活我方部队发起全军突击，各自进攻距离最近的敌方部队。"),
			Props, {});
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要机动的我方部队列表，例如 [\"all\"] 或 [\"P3\", \"第四旅\"]")));
		Props.Add(TEXT("x"), MakeJsonProp(TEXT("number"), TEXT("目标世界坐标 X (cm)")));
		Props.Add(TEXT("y"), MakeJsonProp(TEXT("number"), TEXT("目标世界坐标 Y (cm)")));
		Props.Add(TEXT("facing_yaw_deg"), MakeJsonProp(TEXT("number"), TEXT("可选：到达后的驻守朝向角度（度，90=朝北/+Y，-90=朝南/-Y）")));
		AddTool(TEXT("order_move"),
			TEXT("令指定我方部队按阵型机动至指定世界坐标 (x, y)，自动绕开陡崖与密林。"),
			Props, { TEXT("ally_ids"), TEXT("x"), TEXT("y") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要机动的我方部队列表，例如 [\"P3\"] 或 [\"all\"]")));
		Props.Add(TEXT("anchor_id"), MakeJsonProp(TEXT("string"), TEXT("参考目标：\"commander\"(指挥官) 或某部队编号/名称（如 \"E1\"、\"敌第一旅\"、\"P1\"）")));
		Props.Add(TEXT("direction"), MakeJsonProp(TEXT("string"), TEXT("相对方位：front / back / left / right / north / south / east / west / retreat / near")));
		Props.Add(TEXT("distance_m"), MakeJsonProp(TEXT("number"), TEXT("相对距离（米），例如绕后 25 米、后撤 30 米，默认 20")));
		AddTool(TEXT("order_move_relative"),
			TEXT("相对指定敌我部队或指挥官向某方向机动（用于侧翼包抄、绕后、后撤拉扯、向指挥官靠拢）。"),
			Props, { TEXT("ally_ids"), TEXT("anchor_id"), TEXT("direction") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要原地停驻的我方部队列表，例如 [\"all\"] 或 [\"P4\"]")));
		AddTool(TEXT("order_halt"),
			TEXT("令指定我方部队停止当前机动/进攻，就地驻守。"),
			Props, { TEXT("ally_ids") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要切换跟随状态的我方部队列表，例如 [\"all\"] 或 [\"P1\"]")));
		Props.Add(TEXT("enable"), MakeJsonProp(TEXT("boolean"), TEXT("true=列队跟随指挥官，false=取消跟随")));
		AddTool(TEXT("order_follow_commander"),
			TEXT("令指定我方部队跟随或取消跟随第一人称指挥官。"),
			Props, { TEXT("ally_ids"), TEXT("enable") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		Props.Add(TEXT("ally_ids"), MakeStringArrayProp(TEXT("要在地图上高亮选中的我方部队列表")));
		AddTool(TEXT("select_allies"),
			TEXT("在战略地图上高亮选中指定我方部队。"),
			Props, { TEXT("ally_ids") });
	}
	{
		TMap<FString, TSharedPtr<FJsonObject>> Props;
		AddTool(TEXT("get_legion_battle_state"),
			TEXT("重新获取当前大兵团战场的实时状态 JSON 快照。"),
			Props, {});
	}

	return Tools;
}

bool UBattleAICommandService::DispatchLegionToolCall(const FString& ToolName, const TSharedPtr<FJsonObject>& Args, FString& OutResultJson)
{
	if (ToolName == TEXT("get_legion_battle_state") || ToolName == TEXT("get_battle_state"))
	{
		OutResultJson = JsonObjectToString(BuildLegionSnapshotJson());
		return true;
	}
	if (ToolName == TEXT("order_attack"))
	{
		OutResultJson = Tool_LegionOrderAttack(Args);
		return true;
	}
	if (ToolName == TEXT("order_all_assault"))
	{
		OutResultJson = Tool_LegionOrderAllAssault();
		return true;
	}
	if (ToolName == TEXT("order_move"))
	{
		OutResultJson = Tool_LegionOrderMove(Args);
		return true;
	}
	if (ToolName == TEXT("order_move_relative"))
	{
		OutResultJson = Tool_LegionOrderMoveRelative(Args);
		return true;
	}
	if (ToolName == TEXT("order_halt"))
	{
		OutResultJson = Tool_LegionOrderHalt(Args);
		return true;
	}
	if (ToolName == TEXT("order_follow_commander"))
	{
		OutResultJson = Tool_LegionOrderFollowCommander(Args);
		return true;
	}
	if (ToolName == TEXT("select_allies"))
	{
		OutResultJson = Tool_LegionSelectAllies(Args);
		return true;
	}
	return false;
}

FString UBattleAICommandService::Tool_LegionOrderAttack(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionGameMode* GM = GetLegionGameMode();
	ALegionPlayerController* PC = GetLegionPC();
	if (!GM)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), TEXT("LegionGameMode unavailable"));
		return JsonObjectToString(Res);
	}

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	if (Allies.Num() == 0)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), TEXT("没有找到符合条件的存活我方部队"));
		return JsonObjectToString(Res);
	}

	FString TargetRef;
	if (Args.IsValid()) { Args->TryGetStringField(TEXT("target_id"), TargetRef); }
	TargetRef.TrimStartAndEndInline();

	const bool bNearest = TargetRef.IsEmpty() || TargetRef.Equals(TEXT("nearest"), ESearchCase::IgnoreCase)
		|| TargetRef.Contains(TEXT("最近"));

	ALegionUnit* FixedTarget = bNearest ? nullptr : ResolveLegionUnitRef(TargetRef, true);
	if (!bNearest && (!FixedTarget || FixedTarget->IsWiped() || FixedTarget->GetTeam() != ELegionTeam::Enemy))
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), FString::Printf(TEXT("未找到存活敌方目标: %s"), *TargetRef));
		return JsonObjectToString(Res);
	}

	if (PC)
	{
		PC->SetLegionsFollowing(Allies, false);
		PC->SetSelectedLegions(Allies);
	}

	TArray<TSharedPtr<FJsonValue>> OrderedNames;
	for (ALegionUnit* Ally : Allies)
	{
		ALegionUnit* Tgt = FixedTarget;
		if (!Tgt)
		{
			float BestD2 = MAX_FLT;
			for (ALegionUnit* E : GM->GetEnemyLegions())
			{
				if (!E || E->IsWiped()) { continue; }
				const float D2 = FVector::DistSquared2D(Ally->GetActorLocation(), E->GetActorLocation());
				if (D2 < BestD2) { BestD2 = D2; Tgt = E; }
			}
		}
		if (Tgt)
		{
			Ally->CommandAttack(Tgt);
			OrderedNames.Add(MakeShared<FJsonValueString>(Ally->GetCallsign()));
		}
	}

	Res->SetBoolField(TEXT("ok"), OrderedNames.Num() > 0);
	Res->SetArrayField(TEXT("ordered_allies"), OrderedNames);
	if (FixedTarget)
	{
		Res->SetStringField(TEXT("target"), FString::Printf(TEXT("%s (%s)"), *GetLegionCode(FixedTarget), *FixedTarget->GetCallsign()));
	}
	else
	{
		Res->SetStringField(TEXT("target"), TEXT("nearest_enemy"));
	}
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionOrderAllAssault()
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionGameMode* GM = GetLegionGameMode();
	ALegionPlayerController* PC = GetLegionPC();
	if (!GM)
	{
		Res->SetBoolField(TEXT("ok"), false);
		return JsonObjectToString(Res);
	}

	TArray<ALegionUnit*> Alive;
	for (ALegionUnit* L : GM->GetPlayerLegions())
	{
		if (L && !L->IsWiped()) { Alive.Add(L); }
	}
	if (PC)
	{
		PC->SetLegionsFollowing(Alive, false);
		PC->SetSelectedLegions(Alive);
	}
	GM->AllOutAssault(ELegionTeam::Player);

	Res->SetBoolField(TEXT("ok"), true);
	Res->SetNumberField(TEXT("assaulting_count"), Alive.Num());
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionOrderMove(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionGameMode* GM = GetLegionGameMode();
	ALegionPlayerController* PC = GetLegionPC();
	if (!GM || !Args.IsValid())
	{
		Res->SetBoolField(TEXT("ok"), false);
		return JsonObjectToString(Res);
	}

	double X = 0.0, Y = 0.0;
	if (!Args->TryGetNumberField(TEXT("x"), X) || !Args->TryGetNumberField(TEXT("y"), Y))
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), TEXT("缺少目标坐标 x 或 y"));
		return JsonObjectToString(Res);
	}

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	if (Allies.Num() == 0)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), TEXT("未找到存活我方部队"));
		return JsonObjectToString(Res);
	}

	const FVector DestCenter((float)X, (float)Y, GM->GetDeployCenter().Z);
	double FacingDeg = 1000.0;
	const bool bCustomFacing = Args->TryGetNumberField(TEXT("facing_yaw_deg"), FacingDeg);

	FVector AvgPos = FVector::ZeroVector;
	for (ALegionUnit* L : Allies) { AvgPos += L->GetActorLocation(); }
	AvgPos /= (float)Allies.Num();

	const FVector FacingDir = bCustomFacing
		? FRotator(0.0f, (float)FacingDeg, 0.0f).Vector().GetSafeNormal2D()
		: (DestCenter - AvgPos).GetSafeNormal2D();

	TArray<FVector> Slots;
	if (PC)
	{
		PC->SetLegionsFollowing(Allies, false);
		PC->SetSelectedLegions(Allies);
		PC->ComputeSlotsForLegions(Allies, DestCenter, FacingDir.IsNearlyZero() ? FVector::ForwardVector : FacingDir, Slots);
	}

	TArray<TSharedPtr<FJsonValue>> Names;
	for (int32 i = 0; i < Allies.Num(); ++i)
	{
		const FVector Pt = Slots.IsValidIndex(i) ? Slots[i] : DestCenter;
		Allies[i]->CommandMoveTo(Pt, bCustomFacing ? (float)FacingDeg : 1000.0f);
		Names.Add(MakeShared<FJsonValueString>(Allies[i]->GetCallsign()));
	}

	Res->SetBoolField(TEXT("ok"), true);
	Res->SetArrayField(TEXT("ordered_allies"), Names);
	Res->SetNumberField(TEXT("dest_x"), FMath::RoundToInt(DestCenter.X));
	Res->SetNumberField(TEXT("dest_y"), FMath::RoundToInt(DestCenter.Y));
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionOrderMoveRelative(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionGameMode* GM = GetLegionGameMode();
	ALegionPlayerController* PC = GetLegionPC();
	if (!GM || !Args.IsValid())
	{
		Res->SetBoolField(TEXT("ok"), false);
		return JsonObjectToString(Res);
	}

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	if (Allies.Num() == 0)
	{
		Res->SetBoolField(TEXT("ok"), false);
		Res->SetStringField(TEXT("error"), TEXT("未找到存活我方部队"));
		return JsonObjectToString(Res);
	}

	FString AnchorId, DirStr;
	Args->TryGetStringField(TEXT("anchor_id"), AnchorId);
	Args->TryGetStringField(TEXT("direction"), DirStr);
	double DistM = 20.0;
	Args->TryGetNumberField(TEXT("distance_m"), DistM);
	const float DistCm = FMath::Clamp((float)DistM, 5.0f, 150.0f) * 100.0f;

	FVector AnchorPos = GM->GetDeployCenter();
	FVector AnchorFwd(0.0f, 1.0f, 0.0f);
	if (AnchorId.Equals(TEXT("commander"), ESearchCase::IgnoreCase) || AnchorId.Contains(TEXT("指挥官")))
	{
		if (PC && PC->GetPawn())
		{
			AnchorPos = PC->GetPawn()->GetActorLocation();
			AnchorFwd = FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f).Vector().GetSafeNormal2D();
		}
	}
	else if (ALegionUnit* AnchorUnit = ResolveLegionUnitRef(AnchorId, true))
	{
		AnchorPos = AnchorUnit->GetActorLocation();
		AnchorFwd = AnchorUnit->GetFacingVector();
	}
	else
	{
		// 默认以所选我方部队平均位置为锚
		AnchorPos = FVector::ZeroVector;
		for (ALegionUnit* L : Allies) { AnchorPos += L->GetActorLocation(); }
		AnchorPos /= (float)Allies.Num();
	}

	const FVector AnchorRight(-AnchorFwd.Y, AnchorFwd.X, 0.0f);
	const FString D = DirStr.TrimStartAndEnd().ToLower();
	FVector OffsetDir = FVector::ZeroVector;
	if (D == TEXT("front") || D.Contains(TEXT("前")))       { OffsetDir = AnchorFwd; }
	else if (D == TEXT("back") || D.Contains(TEXT("后")))   { OffsetDir = -AnchorFwd; }
	else if (D == TEXT("left") || D.Contains(TEXT("左")))   { OffsetDir = -AnchorRight; }
	else if (D == TEXT("right") || D.Contains(TEXT("右")))  { OffsetDir = AnchorRight; }
	else if (D == TEXT("north") || D.Contains(TEXT("北")))  { OffsetDir = FVector(0.0f, 1.0f, 0.0f); }
	else if (D == TEXT("south") || D.Contains(TEXT("南")) || D == TEXT("retreat") || D.Contains(TEXT("撤")))
	{
		OffsetDir = FVector(0.0f, -1.0f, 0.0f);
	}
	else if (D == TEXT("west") || D.Contains(TEXT("西")))   { OffsetDir = FVector(1.0f, 0.0f, 0.0f); }
	else if (D == TEXT("east") || D.Contains(TEXT("东")))   { OffsetDir = FVector(-1.0f, 0.0f, 0.0f); }
	else                                                    { OffsetDir = -AnchorFwd; }

	const FVector DestCenter = AnchorPos + OffsetDir.GetSafeNormal2D() * DistCm;
	const FVector FaceAnchor = (AnchorPos - DestCenter).GetSafeNormal2D();
	const float FacingYaw = FaceAnchor.IsNearlyZero() ? 90.0f : FaceAnchor.Rotation().Yaw;

	TArray<FVector> Slots;
	if (PC)
	{
		PC->SetLegionsFollowing(Allies, false);
		PC->SetSelectedLegions(Allies);
		PC->ComputeSlotsForLegions(Allies, DestCenter, FaceAnchor.IsNearlyZero() ? FVector::ForwardVector : FaceAnchor, Slots);
	}

	TArray<TSharedPtr<FJsonValue>> Names;
	for (int32 i = 0; i < Allies.Num(); ++i)
	{
		const FVector Pt = Slots.IsValidIndex(i) ? Slots[i] : DestCenter;
		Allies[i]->CommandMoveTo(Pt, FacingYaw);
		Names.Add(MakeShared<FJsonValueString>(Allies[i]->GetCallsign()));
	}

	Res->SetBoolField(TEXT("ok"), true);
	Res->SetArrayField(TEXT("ordered_allies"), Names);
	Res->SetNumberField(TEXT("dest_x"), FMath::RoundToInt(DestCenter.X));
	Res->SetNumberField(TEXT("dest_y"), FMath::RoundToInt(DestCenter.Y));
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionOrderHalt(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionPlayerController* PC = GetLegionPC();

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	if (PC) { PC->SetLegionsFollowing(Allies, false); }

	TArray<TSharedPtr<FJsonValue>> Names;
	for (ALegionUnit* L : Allies)
	{
		L->CommandHalt();
		Names.Add(MakeShared<FJsonValueString>(L->GetCallsign()));
	}
	Res->SetBoolField(TEXT("ok"), Names.Num() > 0);
	Res->SetArrayField(TEXT("halted_allies"), Names);
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionOrderFollowCommander(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionPlayerController* PC = GetLegionPC();
	if (!PC)
	{
		Res->SetBoolField(TEXT("ok"), false);
		return JsonObjectToString(Res);
	}

	bool bEnable = true;
	if (Args.IsValid()) { Args->TryGetBoolField(TEXT("enable"), bEnable); }

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	PC->SetLegionsFollowing(Allies, bEnable);
	PC->SetSelectedLegions(Allies);

	TArray<TSharedPtr<FJsonValue>> Names;
	for (ALegionUnit* L : Allies) { Names.Add(MakeShared<FJsonValueString>(L->GetCallsign())); }
	Res->SetBoolField(TEXT("ok"), Names.Num() > 0);
	Res->SetBoolField(TEXT("following"), bEnable);
	Res->SetArrayField(TEXT("allies"), Names);
	return JsonObjectToString(Res);
}

FString UBattleAICommandService::Tool_LegionSelectAllies(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
	ALegionPlayerController* PC = GetLegionPC();
	if (!PC)
	{
		Res->SetBoolField(TEXT("ok"), false);
		return JsonObjectToString(Res);
	}

	TArray<ALegionUnit*> Allies;
	ResolveAllyLegions(Args, Allies);
	PC->SetSelectedLegions(Allies);

	TArray<TSharedPtr<FJsonValue>> Names;
	for (ALegionUnit* L : Allies) { Names.Add(MakeShared<FJsonValueString>(L->GetCallsign())); }
	Res->SetBoolField(TEXT("ok"), Names.Num() > 0);
	Res->SetArrayField(TEXT("selected_allies"), Names);
	return JsonObjectToString(Res);
}

bool UBattleAICommandService::RunLegionToolSelfTest()
{
	ALegionGameMode* GM = GetLegionGameMode();
	if (!GM || GM->GetPlayerLegions().Num() == 0 || GM->GetEnemyLegions().Num() == 0)
	{
		UE_LOG(LogERLegion, Warning, TEXT("[LegionAISelfTest] FAIL: LegionGameMode or units not ready"));
		return false;
	}

	// 1. 快照 JSON 与工具定义非空校验
	const TSharedPtr<FJsonObject> Snap = BuildBattlefieldSnapshotJson();
	const bool bSnapOk = Snap.IsValid() && Snap->HasField(TEXT("allies")) && Snap->HasField(TEXT("enemies"));
	const bool bToolsOk = BuildOpenAIToolsJson().Num() >= 7 && BuildAnthropicToolsJson().Num() >= 7;

	// 2. 模拟用户输入 "全部部队优先合力进攻对方第一旅" 触发的工具调用
	EmitLog(EERAICommandLogKind::User, TEXT("全部部队优先合力进攻对方第一旅"));
	TSharedPtr<FJsonObject> AttackArgs = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AllArr;
	AllArr.Add(MakeShared<FJsonValueString>(TEXT("all")));
	AttackArgs->SetArrayField(TEXT("ally_ids"), AllArr);
	AttackArgs->SetStringField(TEXT("target_id"), TEXT("对方第一旅"));

	FString ResultJson;
	const bool bDispatched = DispatchLegionToolCall(TEXT("order_attack"), AttackArgs, ResultJson);
	EmitLog(EERAICommandLogKind::ToolCall, FString::Printf(TEXT("order_attack(%s)"), *JsonObjectToString(AttackArgs)));
	EmitLog(EERAICommandLogKind::ToolResult, ResultJson);

	ALegionUnit* E0 = GM->GetEnemyLegions()[0].Get();
	bool bAllAttackingE0 = (E0 != nullptr);
	for (ALegionUnit* Ally : GM->GetPlayerLegions())
	{
		if (Ally && !Ally->IsWiped() && Ally->GetAttackTarget() != E0)
		{
			bAllAttackingE0 = false;
			break;
		}
	}
	EmitLog(EERAICommandLogKind::Assistant, FString::Printf(TEXT("已下达集火指令：全部我方存活部队正合力进攻 %s。"),
		E0 ? *E0->GetCallsign() : TEXT("敌第一旅")));

	UE_LOG(LogERLegion, Log, TEXT("[LegionAISelfTest] %s: snapshot & OpenAI/Anthropic tool schemas valid"),
		(bSnapOk && bToolsOk) ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogERLegion, Log, TEXT("[LegionAISelfTest] %s: order_attack(all -> 对方第一旅) targets %s for all allies"),
		(bDispatched && bAllAttackingE0) ? TEXT("PASS") : TEXT("FAIL"), E0 ? *E0->GetCallsign() : TEXT("null"));
	return bSnapOk && bToolsOk && bDispatched && bAllAttackingE0;
}

#endif // ER_WITH_AI_COMMAND
