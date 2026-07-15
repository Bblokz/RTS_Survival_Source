// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGEnemyBaseSetupTypes.h"
#include "PCGSymmetricalDefenses.generated.h"

/**
 * @brief Settings for the Symmetrical Defenses node. Selects a symmetrical set of positions from
 * the CandidatePoints around the DefendCenter (rectangular mirror, radial ring or single-axis
 * mirror) and builds a fortified building (with sandbags, obstacle belt, wire and decorators) at
 * each, facing outward from or inward toward the center. Candidates left unused are passed through
 * on the UnusedPoints pin.
 * @note Each chosen position becomes one cluster: Bunkers, Sandbags, Obstacles, BarbedWire and
 * Decorators output points-with-bounds per category; TotalBounds encloses each cluster.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSymmetricalDefensesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	// --- General ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	int32 RandomSeed = 42;

	/** @brief Which symmetry the selected defensive positions must obey around the DefendCenter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	EEnemyDefenseFormation DefenseFormation = EEnemyDefenseFormation::RectangularSymmetrical;

	/** @brief Upper bound on how many positions are chosen while preserving the symmetry. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0"))
	int32 AmountToPlace = 8;

	/** @brief Radial only: how many equally spaced positions make one symmetric ring. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General",
		meta = (ClampMin = "2", ClampMax = "24",
			EditCondition = "DefenseFormation == EEnemyDefenseFormation::RadialSymmetrical"))
	int32 RadialSymmetryCount = 6;

	/** @brief Minimum separation (cm) required between two chosen positions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0"))
	float PositionSpacing = 500.0f;

	/** @brief How close (cm) a candidate must be to an ideal symmetric target to be accepted. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "1"))
	float MatchTolerance = 600.0f;

	/** @brief How each fortified position faces; defaults to facing away from the DefendCenter (outward). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	EEnemyFacingMode FacingMode = EEnemyFacingMode::AwayFromTarget;

	/** @brief Fixed facing yaw (degrees) used when FacingMode is FixedYaw. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General",
		meta = (EditCondition = "FacingMode == EEnemyFacingMode::FixedYaw"))
	float FixedYawDegrees = 0.0f;

	// --- Buildings & defensive works (one per chosen position) ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings")
	TArray<FEnemyBlueprintEntry> Buildings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings", meta = (ClampMin = "0"))
	float BuildingSpacingExtra = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags")
	FEnemySandbagSettings Sandbags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt")
	FEnemyObstacleBeltSettings ObstacleBelt;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire")
	FEnemyBarbedWireSettings BarbedWire;

	// --- Decoration ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorators")
	TArray<FEnemyDecoratorProfile> DecoratorProfiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	FEnemyFoliageSettings Foliage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals")
	FEnemyDecalSettings Decals;
};

/**
 * @brief Execution element for Symmetrical Defenses. Selects the symmetric positions, builds a
 * fortified setup at each with the shared placement builder, spawns managed actors and emits the
 * categorized outputs plus the unused candidate points. Runs on the game thread; never cached.
 */
class FPCGSymmetricalDefensesElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
