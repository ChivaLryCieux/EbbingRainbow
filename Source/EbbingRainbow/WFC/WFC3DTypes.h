// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DTypes.generated.h"

class UStaticMesh;

/** 3D 空间的 6 个邻接方向 */
UENUM(BlueprintType)
enum class EWFC3DDirection : uint8
{
	PosX = 0 UMETA(DisplayName = "+X (Forward)"),
	NegX = 1 UMETA(DisplayName = "-X (Backward)"),
	PosY = 2 UMETA(DisplayName = "+Y (Right)"),
	NegY = 3 UMETA(DisplayName = "-Y (Left)"),
	PosZ = 4 UMETA(DisplayName = "+Z (Up)"),
	NegZ = 5 UMETA(DisplayName = "-Z (Down)")
};

/** 方向操作实用内联辅助函数 */
namespace WFC3DUtils
{
	FORCEINLINE EWFC3DDirection GetOppositeDirection(EWFC3DDirection Dir)
	{
		switch (Dir)
		{
		case EWFC3DDirection::PosX: return EWFC3DDirection::NegX;
		case EWFC3DDirection::NegX: return EWFC3DDirection::PosX;
		case EWFC3DDirection::PosY: return EWFC3DDirection::NegY;
		case EWFC3DDirection::NegY: return EWFC3DDirection::PosY;
		case EWFC3DDirection::PosZ: return EWFC3DDirection::NegZ;
		case EWFC3DDirection::NegZ: return EWFC3DDirection::PosZ;
		default: return EWFC3DDirection::PosX;
		}
	}

	FORCEINLINE FIntVector GetDirectionOffset(EWFC3DDirection Dir)
	{
		switch (Dir)
		{
		case EWFC3DDirection::PosX: return FIntVector(1, 0, 0);
		case EWFC3DDirection::NegX: return FIntVector(-1, 0, 0);
		case EWFC3DDirection::PosY: return FIntVector(0, 1, 0);
		case EWFC3DDirection::NegY: return FIntVector(0, -1, 0);
		case EWFC3DDirection::PosZ: return FIntVector(0, 0, 1);
		case EWFC3DDirection::NegZ: return FIntVector(0, 0, -1);
		default: return FIntVector::ZeroValue;
		}
	}

	FORCEINLINE int32 DirectionToIndex(EWFC3DDirection Dir)
	{
		return static_cast<int32>(Dir);
	}

	FORCEINLINE EWFC3DDirection IndexToDirection(int32 Index)
	{
		return static_cast<EWFC3DDirection>(FMath::Clamp(Index, 0, 5));
	}
}

/** 显式插槽匹配规则对 (非对称相容性) */
USTRUCT(BlueprintType)
struct FWFC3DSocketCompatibilityPair
{
	GENERATED_BODY()

	/** 发送方插槽名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Rules")
	FName SocketA = NAME_None;

	/** 目标方相邻插槽名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Rules")
	FName SocketB = NAME_None;

	/** 是否双向有效 (若为 true 则 A 连 B 同时允许 B 连 A) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Rules")
	bool bBidirectional = true;
};

/** 复合模块的附加网格部件 */
USTRUCT(BlueprintType)
struct FWFC3DMeshPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Mesh Part")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Mesh Part")
	FTransform RelativeTransform = FTransform::Identity;
};

/** 单个 3D 模块的配置定义（可在编辑器中配置或由扫描目录生成） */
USTRUCT(BlueprintType)
struct FWFC3DModuleDefinition
{
	GENERATED_BODY()

	/** 模块唯一标识符 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	FName ModuleId = NAME_None;

	/** 模块对应的静态网格体 (为 nullptr 时代表空气/空单元格) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** 生成时附加的旋转微调 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	FRotator AdditionalRotation = FRotator::ZeroRotator;

	/** 生成时附加的位置偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	FVector AdditionalOffset = FVector::ZeroVector;

	/** 缩放比例 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

	/** 采样相对权重 (权重越大，坍缩时选中的概率越高) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** 6 个方向的插槽定义 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket +X (Forward)"))
	FName SocketPosX = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket -X (Backward)"))
	FName SocketNegX = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket +Y (Right)"))
	FName SocketPosY = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket -Y (Left)"))
	FName SocketNegY = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket +Z (Up)"))
	FName SocketPosZ = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Sockets", meta = (DisplayName = "Socket -Z (Down)"))
	FName SocketNegZ = NAME_None;

	/** 额外的复合网格部件列表 (支持同一单元格组合地面底座、墙体和立柱) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	TArray<FWFC3DMeshPart> AdditionalParts;

	/** 自动派生 4 个 Z 轴旋转方向变体 (0°, 90°, 180°, 270°)，并自动映射 X/Y 面插槽 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Module")
	bool bAutoGenerateRotations = false;

	FName GetSocket(EWFC3DDirection Dir) const
	{
		switch (Dir)
		{
		case EWFC3DDirection::PosX: return SocketPosX;
		case EWFC3DDirection::NegX: return SocketNegX;
		case EWFC3DDirection::PosY: return SocketPosY;
		case EWFC3DDirection::NegY: return SocketNegY;
		case EWFC3DDirection::PosZ: return SocketPosZ;
		case EWFC3DDirection::NegZ: return SocketNegZ;
		default: return NAME_None;
		}
	}

	void SetSocket(EWFC3DDirection Dir, FName Val)
	{
		switch (Dir)
		{
		case EWFC3DDirection::PosX: SocketPosX = Val; break;
		case EWFC3DDirection::NegX: SocketNegX = Val; break;
		case EWFC3DDirection::PosY: SocketPosY = Val; break;
		case EWFC3DDirection::NegY: SocketNegY = Val; break;
		case EWFC3DDirection::PosZ: SocketPosZ = Val; break;
		case EWFC3DDirection::NegZ: SocketNegZ = Val; break;
		default: break;
		}
	}
};

/** 编译后的轻量复合网格部件 */
struct FCompiledWFC3DMeshPart
{
	TWeakObjectPtr<UStaticMesh> Mesh = nullptr;
	FTransform LocalTransform = FTransform::Identity;
};

/** 编译后的轻量内部模块数据，供求解器与渲染器使用 */
struct FCompiledWFC3DModule
{
	FName SourceModuleId = NAME_None;
	FName VariantId = NAME_None;
	TWeakObjectPtr<UStaticMesh> Mesh = nullptr;
	FTransform LocalTransform = FTransform::Identity;
	TArray<FCompiledWFC3DMeshPart> AdditionalParts;
	float Weight = 1.0f;
	FName Sockets[6];
};

/** 求解器网格中的单元格 Slot */
struct FWFCSlot3D
{
	TBitArray<> Candidates;
	int32 RemainingCandidatesCount = 0;
	float CachedEntropy = 0.0f;
	bool bCollapsed = false;
	int32 FinalModuleIndex = INDEX_NONE;

	void Initialize(int32 TotalModuleCount, float InitialEntropy)
	{
		Candidates.Init(true, TotalModuleCount);
		RemainingCandidatesCount = TotalModuleCount;
		CachedEntropy = InitialEntropy;
		bCollapsed = false;
		FinalModuleIndex = INDEX_NONE;
	}
};
