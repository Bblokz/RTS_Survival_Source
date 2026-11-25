// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"

#include "SovietRefineryDropOff.generated.h"

/**
 * @brief The drop-off a Soviet refinery (USovietRefineryComponent) creates at runtime. Its cargo slots are the
 * refinery's stockpile: harvesters of the refinery's owner deliver into it and the refinery converts from it.
 * @note Only exposes the protected init so the refinery component can configure it from C++.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API USovietRefineryDropOff : public UResourceDropOff
{
	GENERATED_BODY()

public:
	void InitRefineryDropOff(const TMap<ERTSResourceType, int32>& StockCapacity, UMeshComponent* BuildingMesh,
	                         const FName DropOffSocketName)
	{
		InitResourceDropOff(StockCapacity, BuildingMesh, DropOffSocketName, 300.f);
	}
};
