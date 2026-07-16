#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthContribution.h"
#include "WorldMissionContext.generated.h"

/**
 * @brief Persistent value snapshot consumed after an enemy or mission operation map transition.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldMissionContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	EMapItemType OperationType = EMapItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	EMapEnemyItem EnemyObjectType = EMapEnemyItem::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	EMapMission MissionType = EMapMission::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	FGuid SourceAnchorKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	int32 OperationSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	TArray<FWorldStrengthContribution> StrengthContributions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context")
	bool bIsInitialized = false;

	/**
	 * @brief Sums the signed contribution ledger so the total cannot become stale.
	 * @return Total operation strength percentage.
	 */
	int32 GetTotalStrengthPercentage() const;

	/**
	 * @brief Validates the enemy/mission discriminator and its corresponding enum.
	 * @return true when this snapshot can be consumed by an operation map.
	 */
	bool GetIsValid() const;
};

/**
 * @brief Exposes calculated world mission context values to Blueprint without duplicating cached fields.
 */
UCLASS()
class RTS_SURVIVAL_API UWorldMissionContextBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "World Campaign|Mission Context",
		meta = (DisplayName = "Get Total Strength Percentage"))
	static int32 GetTotalStrengthPercentage(const FWorldMissionContext& WorldMissionContext);

	UFUNCTION(BlueprintPure, Category = "World Campaign|Mission Context",
		meta = (DisplayName = "Is World Mission Context Valid"))
	static bool GetIsWorldMissionContextValid(const FWorldMissionContext& WorldMissionContext);
};
