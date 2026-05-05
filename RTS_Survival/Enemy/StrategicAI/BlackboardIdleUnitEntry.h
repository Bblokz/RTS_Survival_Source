#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "BlackboardIdleUnitEntry.generated.h"

enum class EAllUnitType : uint8;

USTRUCT()
struct FBlackboardIdleUnitEntry
{
	GENERATED_BODY()

	FBlackboardIdleUnitEntry() = default;

	UPROPERTY()
	TWeakObjectPtr<AActor> IdleUnit = nullptr;

	EAllUnitType UnitType = EAllUnitType::UNType_None;
	int32 UnitSubtypeRaw = 0;
};