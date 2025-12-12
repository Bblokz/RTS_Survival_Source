#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "UnitCost.generated.h"

enum class ERTSResourceType : uint8;

// Resource-Cost Pair Struct
USTRUCT(BlueprintType)
struct FResourceCostPair
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ERTSResourceType ResourceType;

	UPROPERTY(BlueprintReadOnly)
	int32 Amount;

	FResourceCostPair()
		: ResourceType(ERTSResourceType::Resource_Metal), Amount(0)
	{
	}

	FResourceCostPair(ERTSResourceType InResourceType, int32 InAmount)
		: ResourceType(InResourceType), Amount(InAmount)
	{
	}
};

// Unit Cost Struct
USTRUCT(Blueprintable)
struct FUnitCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<ERTSResourceType, int32> ResourceCosts;

	FUnitCost()
	{
	}

	FUnitCost(std::initializer_list<FResourceCostPair> ResourceCostList, float InTimeCost = 0.0f)
	{
		for (const FResourceCostPair& Pair : ResourceCostList)
		{
			ResourceCosts.Add(Pair.ResourceType, Pair.Amount);
		}
	}
};
