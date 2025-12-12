#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "AsyncBxpBatchRequest.generated.h"

struct FBxpOptionData;
enum class EBuildingExpansionType : uint8;

/**
 * Tracks a single “instant placement” batch request:
 * - which expansion types to load
 * - who to notify
 * - the handle for cancelation if needed
 */
USTRUCT()
struct FInstantPlacementBxpBatch
{
    GENERATED_BODY()

    // The exact list of expansion‐types the user wants, in order.
    UPROPERTY()
    TArray<EBuildingExpansionType> TypesToLoad;

    // Who to call back once all are spawned.
    TScriptInterface<IBuildingExpansionOwner> Owner;

    // The StreamableHandle for this batch’s async load.
    TSharedPtr<FStreamableHandle> Handle;

	TArray<int32> BxpItemIndices;

	TArray<FBxpOptionData> BxpTypesAndConstructionRules;
};
