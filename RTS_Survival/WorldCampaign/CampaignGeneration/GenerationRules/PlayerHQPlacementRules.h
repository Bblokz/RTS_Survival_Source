#pragma once

#include "CoreMinimal.h"

#include "PlayerHQPlacementRules.generated.h"

class AAnchorPoint;

USTRUCT(BlueprintType)
struct FPlayerHQPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	// Anchor degree constraints: avoid dead-ends (typical 2 or 3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorDegreeForHQ = 2;

	// Start neighborhood size: ensure enough anchors nearby to populate early content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorsWithinHops = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorsWithinHopsRange = 2;

	// Safe ring: no enemies inside a hop radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 SafeZoneMaxHops = 1;
};
