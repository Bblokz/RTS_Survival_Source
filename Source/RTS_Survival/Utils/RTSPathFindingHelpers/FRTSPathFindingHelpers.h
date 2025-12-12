#pragma once
#include "CoreMinimal.h"
struct FPathFindingQuery;

struct FRTSPathFindingHelpers
{
static bool IsLocationInHighCostArea(const UWorld* World,
    	                              const FVector& Location,
    	                              const FPathFindingQuery& Query);	
};
