// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/WeakInterfacePtr.h"
// Full include required: TWeakInterfacePtr<ICommands> below instantiates with the complete type
// (a forward declaration breaks in unity blobs that lack another include of Commands.h).
#include "RTS_Survival/Interfaces/Commands.h"

#include "EnemyRetreatData.generated.h"

UENUM(BlueprintType)
enum class EPostRetreatCounterStrategy : uint8
{
	None,
	Attack,
	RemoveUnits
};

USTRUCT()
struct FRetreatElement
{
	GENERATED_BODY()

	TWeakInterfacePtr<ICommands> Unit = nullptr;
	FVector RetreatLocation = FVector::ZeroVector;
};

USTRUCT()
struct FEnemyRetreatData
{
	GENERATED_BODY()

	int32 RetreatID = -1;
	TArray<FRetreatElement> RetreatingSquadControllers = {};
	TArray<FRetreatElement> ReverseRetreatUnits = {};
	EPostRetreatCounterStrategy PostRetreatCounterStrategy = EPostRetreatCounterStrategy::None;
	FVector CounterattackLocation = FVector::ZeroVector;
	float TimeTillCounterAttackAfterLastRetreatingUnitReached = 0.f;
	float MaxTimeWaitTillCounterAttack = 0.f;
	float RetreatStartTimeSeconds = 0.f;
	float AllUnitsReachedTimeSeconds = 0.f;
	bool bM_HasAllUnitsReached = false;
	bool bM_HasReportedNoneStrategy = false;
	FTimerHandle MaxWaitCounterAttackTimerHandle;
	FTimerHandle PostReachCounterAttackTimerHandle;
};
