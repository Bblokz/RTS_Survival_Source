// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "TrackNavFilter.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTrackNavFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UTrackNavFilter(const FObjectInitializer& ObjectInitializer);

	
};
