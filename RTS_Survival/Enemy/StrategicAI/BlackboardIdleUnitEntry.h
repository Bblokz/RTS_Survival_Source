#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "BlackboardIdleUnitEntry.generated.h"

enum class EAllUnitType : uint8;

// Contains the unit idle pointer (note that idle here does not mean ICommands idle but not assigned to any enemy ai controller
// task while set available for such tasks.
// Has subtype raw value and all unit type enum for easy unit type verification without casting.
USTRUCT()
struct FBlackboardIdleUnitEntry
{
	GENERATED_BODY()

	FBlackboardIdleUnitEntry() = default;
	FBlackboardIdleUnitEntry(AActor* UnitActor)
		: IdleUnit(UnitActor)
	{
	}

	FBlackboardIdleUnitEntry(const TWeakObjectPtr<AActor>& UnitActor)
		: IdleUnit(UnitActor)
	{
	}

	bool operator==(const FBlackboardIdleUnitEntry& Other) const
	{
		return IdleUnit == Other.IdleUnit;
	}

	bool operator==(const AActor* UnitActor) const
	{
		return IdleUnit.Get() == UnitActor;
	}

	bool operator==(const TWeakObjectPtr<AActor>& UnitActor) const
	{
		return IdleUnit == UnitActor;
	}

	operator TWeakObjectPtr<AActor>() const
	{
		return IdleUnit;
	}

	AActor* GetIdleUnitActor() const
	{
		return IdleUnit.Get();
	}

	void SetIdleUnitActor(AActor* UnitActor)
	{
		IdleUnit = UnitActor;
	}

	bool GetIsMatchingIdleUnitActor(const AActor* UnitActor) const
	{
		return IdleUnit.Get() == UnitActor;
	}

	static int32 RemoveByUnitActor(TArray<FBlackboardIdleUnitEntry>& UnitEntries, const AActor* UnitActor)
	{
		return UnitEntries.RemoveAll(
			[UnitActor](const FBlackboardIdleUnitEntry& Entry)
			{
				return Entry.GetIsMatchingIdleUnitActor(UnitActor);
			});
	}

	friend bool operator==(const AActor* UnitActor, const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.GetIsMatchingIdleUnitActor(UnitActor);
	}

	friend bool operator==(const TWeakObjectPtr<AActor>& UnitActor, const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry == UnitActor;
	}

	bool IsValid() const
	{
		return IdleUnit.IsValid();
	}

	// Make sure to check IsValid first!!
	AActor* Get() const
	{
		return IdleUnit.Get();
	}
	

	UPROPERTY()
	TWeakObjectPtr<AActor> IdleUnit = nullptr;

	EAllUnitType UnitType = EAllUnitType::UNType_None;
	int32 UnitSubtypeRaw = 0;
};
