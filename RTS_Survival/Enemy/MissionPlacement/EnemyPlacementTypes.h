// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "EnemyPlacementTypes.generated.h"

UENUM(BlueprintType)
enum class EEnemyPlacementUnitKind : uint8
{
	Vehicle UMETA(DisplayName = "Vehicle"),
	Squad UMETA(DisplayName = "Squad")
};

/**
 * One enemy unit picked on a placed actor in the level. FTrainingOption keeps its fields EditDefaultsOnly, so the
 * details panel of a placed actor hides them; this picker shows a vehicle or squad dropdown instead.
 */
USTRUCT(BlueprintType)
struct FEnemyPlacementUnit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
	EEnemyPlacementUnitKind Kind = EEnemyPlacementUnitKind::Vehicle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit",
		meta = (EditCondition = "Kind == EEnemyPlacementUnitKind::Vehicle", EditConditionHides))
	ETankSubtype VehicleSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit",
		meta = (EditCondition = "Kind == EEnemyPlacementUnitKind::Squad", EditConditionHides))
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

	FTrainingOption ToTrainingOption() const;

	// False while the dropdown of the chosen kind is still on None.
	bool GetIsSet() const;

	static FEnemyPlacementUnit MakeVehicle(const ETankSubtype InVehicleSubtype);
	static FEnemyPlacementUnit MakeSquad(const ESquadSubtype InSquadSubtype);
};

/**
 * Editor colours of the placed enemy mission actors. One hue per actor type, so a busy map reads at a glance;
 * the shape tells the role inside an actor (spheres spawn, arrows move, boxes hold, rings guard or fortify).
 * Yellow is kept for problem labels and white for facing arrows.
 */
namespace EnemyPlacementColors
{
	inline const FColor AttackWave(235, 64, 40);
	inline const FColor PatrolWave(40, 150, 255);
	inline const FColor Convoy(70, 210, 90);
	inline const FColor Strongpoint(225, 70, 200);
	// Teal-cyan, set apart from the patrol blue by its green.
	inline const FColor AirDrop(20, 225, 200);
	inline const FColor Problem(255, 210, 0);
	inline const FColor Facing = FColor::White;

	/** @return The colour scaled towards black, for secondary lines of the same actor. */
	RTS_SURVIVAL_API FColor GetDimmed(const FColor& Color, const float Brightness);
}

/** Helpers every placed enemy mission actor shares (difficulty, spawn spread, navmesh projection). */
namespace EnemyPlacement
{
	/**
	 * @brief Reads the difficulty the mission runs on; the mission manager sets it up during the first frames.
	 * @param WorldContextObject Any object in the game world.
	 * @param OutDifficulty Receives the difficulty.
	 * @return False while neither the mission manager nor the game instance knows the difficulty yet.
	 */
	RTS_SURVIVAL_API bool TryGetGameDifficulty(const UObject* WorldContextObject, ERTSGameDifficulty& OutDifficulty);

	/** @return True when Current is FromDifficulty or harder; Ironman counts as harder than Brutal. */
	RTS_SURVIVAL_API bool GetIsDifficultyIncluded(const ERTSGameDifficulty Current,
	                                              const ERTSGameDifficulty FromDifficulty);

	/**
	 * @brief Spreads units that share one point on rings around it so they do not spawn inside each other.
	 * @param UseIndex 0 for the first unit on the point (no offset), 1.. for the next ones.
	 * @return Offset from the shared point.
	 */
	RTS_SURVIVAL_API FVector GetRingSpreadOffset(const int32 UseIndex);

	/** @return Location on the navmesh near Location, or Location itself when there is no navmesh there. */
	RTS_SURVIVAL_API FVector ProjectToNavigationIfPossible(UWorld* World, const FVector& Location);

	RTS_SURVIVAL_API TArray<FTrainingOption> ToTrainingOptions(const TArray<FEnemyPlacementUnit>& Units);

	/** @return True when the list has at least one unit and every unit has its subtype picked. */
	RTS_SURVIVAL_API bool GetAreUnitsSet(const TArray<FEnemyPlacementUnit>& Units);
}
