#pragma once

#include "CoreMinimal.h"
#include "WorldCampaign/CampaignGeneration/Enums/PlacementFailurePolicy/Enum_PlacementFailurePolicy.h"

#include "WorldCampaignPlacementFailurePolicy.generated.h"

USTRUCT(BlueprintType)
struct FWorldCampaignPlacementFailurePolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy GlobalPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy CreateConnectionsPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceHQPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyHQPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyObjectsPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceNeutralObjectsPolicy = EPlacementFailurePolicy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceMissionsPolicy = EPlacementFailurePolicy::NotSet;
};
