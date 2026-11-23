// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "WorldCampaignTechSettings.generated.h"

/**
 * @brief Branches of the world-map tech tree. They follow the buildings that train the units a tech improves.
 * Append only: saved in config by value.
 */
UENUM(BlueprintType)
enum class EWorldCampaignTechBranch : uint8
{
	None,
	Infantry,
	LightArmour,
	HeavyArmour,
	Air,
	Economy
};

/**
 * @brief Where one technology sits in the world-map tree and what it costs there.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignTechDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	EWorldCampaignTechBranch Branch = EWorldCampaignTechBranch::None;

	/** 0 is the root row of the branch; techs in a row are ordered by their enum value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech", meta = (ClampMin = "0"))
	int32 Row = 0;

	/** Price in world-map resources (blueprints and radixite). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	FWorldMapResources Cost;

	/** Every one of these must be researched first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	TArray<ETechnology> Prerequisites;
};

/**
 * @brief The whole world-map tech tree as plain data, so the rules in WorldCampaignTechRules can be tested without a
 * world. The constructor holds the starting layout of the 24 technologies.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignTechTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	TMap<ETechnology, FWorldCampaignTechDefinition> Technologies;

	FWorldCampaignTechTable();

	const FWorldCampaignTechDefinition* FindDefinition(const ETechnology Technology) const
	{
		return Technologies.Find(Technology);
	}

private:
	void AddDefinition(ETechnology Technology, EWorldCampaignTechBranch Branch, int32 Row,
	                   const TArray<ETechnology>& Prerequisites);
};

/**
 * @brief Project settings page for the world-map tech tree (Config/DefaultGame.ini). Read through GetTechTable.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Tech Tree"))
class RTS_SURVIVAL_API UWorldCampaignTechSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "World Campaign Tech Tree")
	FWorldCampaignTechTable TechTable;

	static const FWorldCampaignTechTable& GetTechTable();
};
