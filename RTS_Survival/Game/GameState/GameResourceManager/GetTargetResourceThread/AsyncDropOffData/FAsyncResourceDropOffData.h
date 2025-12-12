#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"


struct FAsyncDropOffData
{
    TWeakObjectPtr<UResourceDropOff> DropOff;
    uint32 OwningPlayer;
    TMap<ERTSResourceType, FHarvesterCargoSlot> ResourceDropOffCapacity;
    FVector DropOffLocation;
	bool bIsActiveDropOff;
};

struct FAsyncResourceData
{
   TWeakObjectPtr<UResourceComponent> Resource;
	ERTSResourceType ResourceType;
	FVector ResourceLocation;
	bool bStillContainsResources;
};
