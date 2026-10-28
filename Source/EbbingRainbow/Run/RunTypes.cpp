// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RunTypes.h"

namespace
{
	/**
	 * 节点类型元数据表。顺序必须与 ERouteNodeType 一致（下面有 static_assert 兜底）。
	 * 新增类型：加枚举值 + 在这里加一行，其余系统自动适配。
	 */
	const FRouteNodeTypeInfo GNodeTypeTable[] =
	{
		{ ERouteNodeType::Safe,   TEXT("安全节点"), TEXT("安"), FLinearColor(0.42f, 0.78f, 0.92f, 1.0f), false },
		{ ERouteNodeType::Battle, TEXT("战斗节点"), TEXT("战"), FLinearColor(1.00f, 0.44f, 0.30f, 1.0f), true  },
	};

	static_assert(UE_ARRAY_COUNT(GNodeTypeTable) == static_cast<int32>(ERouteNodeType::Count),
		"GNodeTypeTable 必须与 ERouteNodeType 一一对应：新增节点类型时请同步补一行元数据。");
}

const FRouteNodeTypeInfo& GetRouteNodeTypeInfo(ERouteNodeType Type)
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Type), 0, static_cast<int32>(ERouteNodeType::Count) - 1);
	return GNodeTypeTable[Index];
}

bool IsRouteNodeCombat(ERouteNodeType Type)
{
	return GetRouteNodeTypeInfo(Type).bIsCombat;
}
