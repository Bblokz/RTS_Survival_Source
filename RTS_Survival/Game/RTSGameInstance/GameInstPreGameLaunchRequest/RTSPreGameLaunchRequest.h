#pragma once

#include "CoreMinimal.h"

#include "RTSPreGameLaunchRequest.generated.h"

UENUM(BlueprintType)
enum class ERTSPreGameSetupFlow : uint8
{
	NotInitialised,
	FactionAndDifficulty,
	FactionAndDifficultyLoadCampaign,
};

/**
 * @brief Persists the requested setup flow and destination while travelling through pre-game maps.
 */
USTRUCT(BlueprintType)
struct FRTSPreGameLaunchRequest
{
	GENERATED_BODY()

	FRTSPreGameLaunchRequest() = default;
	FRTSPreGameLaunchRequest(
		const ERTSPreGameSetupFlow NewSetupFlow,
		const TSoftObjectPtr<UWorld> NewDestinationMap);

	bool GetIsValid() const;
	void Reset();

	UPROPERTY(BlueprintReadOnly, Category = "Pre-Game Setup")
	ERTSPreGameSetupFlow SetupFlow = ERTSPreGameSetupFlow::NotInitialised;

	UPROPERTY(BlueprintReadOnly, Category = "Pre-Game Setup")
	TSoftObjectPtr<UWorld> DestinationMap = nullptr;
};
