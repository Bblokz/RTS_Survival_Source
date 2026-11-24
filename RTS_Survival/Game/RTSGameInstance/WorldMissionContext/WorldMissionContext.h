#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthContribution.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/PlayerCardData/PlayerCardData.h"
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

	// True only when the world map launched this operation. PIE startup settings can fill the context too, so the
	// campaign-only behaviour (tier cap, return to the map, reporting a result) is gated on this, not on bIsInitialized.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	bool bLaunchedFromWorldMap = false;

	// Short name of the level the operation is fought on. The game instance drops the context when any other
	// mission level loads, so a campaign operation left through a menu cannot leak into a later survival game.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	FString OperationLevelShortName;

	// Division or convoy the operation is fought against; invalid for anchor targets.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	FGuid SourceDivisionKey;

	// Highest global ability tier the campaign rank allows; the mission's own cap can only lower it further.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	EGlobalAbilityTier CampaignGlobalAbilityTierCap = EGlobalAbilityTier::TierIV;

	// Technologies bought on the world map; researched for free when the mission starts.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	TArray<ETechnology> CampaignResearchedTechnologies;

	// The campaign deck as the deck builder saved it; the mission loads its cards from here instead of the profile.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	FPlayerCardSaveData CampaignCardData;

	// Finished stages of the enemy superweapon when the operation started. No effect is designed yet.
	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Context")
	int32 FreezeStage = 0;

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

	/**
	 * @brief Whether this mission is being played as a turn of the world campaign.
	 * @return true only for operations the world map launched.
	 */
	bool GetIsCampaignOperation() const { return bIsInitialized && bLaunchedFromWorldMap; }
};

UENUM(BlueprintType)
enum class EWorldMissionOutcome : uint8
{
	None,
	Victory,
	Defeat
};

/**
 * @brief What a campaign operation reports back to the world map. Written when the mission ends, read once when
 * the world map level loads again.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldMissionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Result")
	EWorldMissionOutcome Outcome = EWorldMissionOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Result")
	FGuid SourceAnchorKey;

	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Mission Result")
	FGuid SourceDivisionKey;

	bool GetHasResult() const { return Outcome != EWorldMissionOutcome::None; }
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
