// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "WorldCampaignMapContentSettings.generated.h"

/**
 * @brief What a pinned place holds once the campaign is generated.
 */
UENUM(BlueprintType)
enum class EWorldPinnedPlaceRole : uint8
{
	None,
	PlayerHQ,
	EnemyHQ,
	EnemyWall,
	Mission
};

/**
 * @brief A fixed, named location on the campaign map (Berlin, the Harz, the Ural Shield). The generator resolves it
 * to an anchor before any seeded placement runs, so the story spine is the same in every campaign.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignPinnedPlace
{
	GENERATED_BODY()

	/** Stable id; a hand-placed anchor with the same pin id is used instead of spawning one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pinned Place")
	FName PinId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pinned Place")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pinned Place")
	EWorldPinnedPlaceRole Role = EWorldPinnedPlaceRole::None;

	/** Mission placed here when Role is Mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pinned Place")
	EMapMission Mission = EMapMission::None;

	/** World location on the campaign map; an anchor is spawned here when no hand-placed anchor carries the pin id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pinned Place")
	FVector WorldLocation = FVector::ZeroVector;
};

/**
 * @brief The name the campaign map shows for one mission. Only needed for a mission that stands on no pinned place,
 * or that should read differently from its place; a pinned mission is otherwise named after its place.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldMissionNameDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Name")
	EMapMission Mission = EMapMission::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Name")
	FText DisplayName;
};

/**
 * @brief Gives a country mesh on the campaign map a stable id and a readable name.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCountryDefinition
{
	GENERATED_BODY()

	/** Stable id used in saves and by build nodes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country")
	FName CountryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country")
	FText DisplayName;

	/** Object name of the country fill mesh actor in the campaign level (AActor::GetFName). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country")
	FName CountryMeshActorName = NAME_None;
};

/**
 * @brief A place inside a country where the player may put up one building once the country is free enough.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldBuildNodeDefinition
{
	GENERATED_BODY()

	/** Stable id used in saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Node")
	FName BuildNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Node")
	FName CountryId = NAME_None;

	/** 0 = the first node of the country to unlock, 1 = the second, and so on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Node", meta = (ClampMin = "0"))
	int32 UnlockOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Node")
	FVector WorldLocation = FVector::ZeroVector;
};

/**
 * @brief The hand-authored content of the campaign map kept in config (Config/DefaultGame.ini) instead of in the
 * level, so it can be authored and reviewed as text: pinned story places, countries and build nodes.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Map Content"))
class RTS_SURVIVAL_API UWorldCampaignMapContentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Off = the generator behaves exactly as it did before pinned places existed. */
	UPROPERTY(EditAnywhere, Config, Category = "Pinned Places")
	bool bUsePinnedPlaces = true;

	UPROPERTY(EditAnywhere, Config, Category = "Pinned Places")
	TArray<FWorldCampaignPinnedPlace> PinnedPlaces;

	/** Mission names that win over the pinned place's name; see WorldCampaignMissionNames. */
	UPROPERTY(EditAnywhere, Config, Category = "Missions")
	TArray<FWorldMissionNameDefinition> MissionNames;

	UPROPERTY(EditAnywhere, Config, Category = "Countries")
	TArray<FWorldCountryDefinition> Countries;

	UPROPERTY(EditAnywhere, Config, Category = "Countries")
	TArray<FWorldBuildNodeDefinition> BuildNodes;

	/** Level opened when a campaign operation ends and the player returns to the map. */
	UPROPERTY(EditAnywhere, Config, Category = "Operations", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath WorldCampaignMap = FSoftObjectPath(TEXT("/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign.RTS_WorldCampaign"));

	/** Mission levels by type, used when the world player controller Blueprint has no entry for a mission. */
	UPROPERTY(EditAnywhere, Config, Category = "Operations", meta = (AllowedClasses = "/Script/Engine.World"))
	TMap<EMapMission, FSoftObjectPath> MissionMaps;

	static const UWorldCampaignMapContentSettings* Get()
	{
		return GetDefault<UWorldCampaignMapContentSettings>();
	}
};
