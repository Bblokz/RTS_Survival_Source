// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/TrainingOptionClassSetup.h"

#include "EnemyDirectorUnitOverridesDataAsset.generated.h"

enum class EEnemyDirector : uint8;

/** Sparse class replacements configured for one enemy director. */
USTRUCT(BlueprintType)
struct FEnemyDirectorUnitOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Overrides|Tank")
	TArray<FTankTrainingOptionSetup> M_TankOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Overrides|Squad")
	TArray<FSquadTrainingOptionSetup> M_SquadOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Overrides|Nomadic")
	TArray<FNomadicTrainingOptionSetup> M_NomadicOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Overrides|Aircraft")
	TArray<FAircraftTrainingOptionSetup> M_AircraftOverrides;
};

/**
 * @brief Designers use this asset to configure the sparse unit-class differences for Forge and Harvest missions.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UEnemyDirectorUnitOverridesDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const FEnemyDirectorUnitOverrides* FindOverrides(const EEnemyDirector EnemyDirector) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Director of Forge", meta = (AllowPrivateAccess = "true"))
	FEnemyDirectorUnitOverrides M_DirectorOfForgeOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Director of Harvest", meta = (AllowPrivateAccess = "true"))
	FEnemyDirectorUnitOverrides M_DirectorOfHarvestOverrides;
};
