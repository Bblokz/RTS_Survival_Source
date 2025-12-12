#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "HarvesterCargoSlot.generated.h"


USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FHarvesterCargoSlot
{
	GENERATED_BODY()

public:
	FHarvesterCargoSlot()
		: ResourceType(ERTSResourceType::Resource_None)
		  , MaxCapacity(0)
		  , CurrentAmount(0)
	{
	}

	FHarvesterCargoSlot(const ERTSResourceType InResourceType, const int32 InMaxCapacity)
		: ResourceType(InResourceType)
		  , MaxCapacity(InMaxCapacity)
		  , CurrentAmount(0)
	{
	}

	inline bool IsFull() const
	{
		return CurrentAmount >= MaxCapacity;
	}

	/** @return How much was left of the provided resource after adding as much as possible to the slot. */
	inline int32 AddResource(const int32 Amount)
	{
		if (CurrentAmount + Amount > MaxCapacity)
		{
			const int32 Difference = Amount + CurrentAmount - MaxCapacity;
			CurrentAmount = MaxCapacity;
			return Difference;
		}
		CurrentAmount += Amount;
		return 0;
	}

	inline bool HasCapacityForAmount(const int32 Amount) const
	{
		return CurrentAmount + Amount <= MaxCapacity;
	}

	inline int32 GetPercentageFilled() const
	{
		if (MaxCapacity <= 0)
		{
			return 0;
		}
		return (CurrentAmount * 100) / MaxCapacity;
	}

	/** The type of resource this slot holds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSResourceType ResourceType;

	/** The maximum capacity of this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxCapacity;

	/** The current amount of resource in this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentAmount;
};
