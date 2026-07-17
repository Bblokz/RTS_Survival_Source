// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateDivision.generated.h"

class AActor;

/** @brief Order in which the tank and squad rosters claim the formation slots (front to back). */
UENUM(BlueprintType)
enum class EDivisionSpawnOrdering : uint8
{
	// Tanks claim the front slots; squads fill the remaining slots behind them.
	TanksFirst,
	// Squads claim the front slots; tanks fill the remaining slots behind them.
	SquadsFirst,
	// Tanks and squads are deterministically shuffled over all slots.
	Mixed
};

/** @brief Geometric arrangement of the division's formation slots. */
UENUM(BlueprintType)
enum class EDivisionFormationShape : uint8
{
	// A fixed amount of units per row; rows stack away from the face-towards point.
	Rectangular,
	// Concentric semicircle arcs bulging toward the face-towards point.
	SemiSpheres
};

/**
 * @brief One unit Blueprint contributing Count members to the division roster.
 * Measured Blueprint bounds keep differently sized units from overlapping.
 */
USTRUCT(BlueprintType)
struct FDivisionUnitEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> UnitClass;

	/** @brief How many formation members this entry contributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** @brief Horizontal radius used for placement; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
	float FootprintRadiusOverride = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;

	/** @brief Added to the computed facing yaw for assets authored with a nonstandard forward axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;
};

/**
 * @brief Creates spline-bounded division areas from candidate points, rejects areas that overlap
 * exclusions or one another, then arranges tank and squad units in a terrain-snapped formation
 * that rotates toward the Face Towards input point.
 * @note Connect candidate point data to In, the point to face on Face Towards, and optional
 * blocking spatial data to Excluded Bounds. Individual units nudge away from excluded pockets.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateDivisionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
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

public:
	// --- Divisions ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions")
	int32 RandomSeed = 1944;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0"))
	int32 MinDivisionsToCreate = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0"))
	int32 MaxDivisionsToCreate = 1;

	/** @brief Free space kept between the outermost formation slot and the spline boundary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0", Units = "cm"))
	float AreaPadding = 600.0f;

	/** @brief Random radial variation gives each temporary spline a natural, non-circular edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0", ClampMax = "0.8"))
	float AreaBoundaryVariation = 0.15f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "6", ClampMax = "64"))
	int32 SplinePointCount = 16;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0", Units = "cm"))
	float DivisionClearance = 500.0f;

	/** @brief Spacing used while checking the carved area against exclusion data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "25", Units = "cm"))
	float ExclusionSampleSpacing = 250.0f;

	/** @brief Fraction of the carved area allowed to overlap exclusions; units dodge those pockets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Divisions", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxExcludedAreaFraction = 0.2f;

	// --- Formation ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation")
	EDivisionFormationShape FormationShape = EDivisionFormationShape::Rectangular;

	/** @brief Center-to-center spacing between neighbouring units in a row or along an arc. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation", meta = (ClampMin = "50", Units = "cm"))
	float OffsetBetweenUnits = 450.0f;

	/** @brief Spacing between consecutive rows (Rectangular) or arcs (SemiSpheres). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation", meta = (ClampMin = "50", Units = "cm"))
	float OffsetBetweenRows = 550.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation")
	EDivisionSpawnOrdering SpawnOrdering = EDivisionSpawnOrdering::TanksFirst;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation",
		meta = (ClampMin = "1", EditCondition = "FormationShape == EDivisionFormationShape::Rectangular"))
	int32 UnitsPerRow = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation",
		meta = (ClampMin = "1", EditCondition = "FormationShape == EDivisionFormationShape::SemiSpheres"))
	int32 UnitsPerSemiSphere = 7;

	/** @brief Rotates every semicircle arc around the formation center. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation",
		meta = (Units = "deg", EditCondition = "FormationShape == EDivisionFormationShape::SemiSpheres"))
	float SemiSphereYawOffsetDegrees = 0.0f;

	/** @brief Radius of the innermost arc; 0 derives it from the offset between units. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Formation",
		meta = (ClampMin = "0", Units = "cm", EditCondition = "FormationShape == EDivisionFormationShape::SemiSpheres"))
	float InnerSemiSphereRadius = 0.0f;

	// --- Units ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Units")
	TArray<FDivisionUnitEntry> TankUnits;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Units")
	TArray<FDivisionUnitEntry> SquadUnits;

	// --- Placement adjustment ---

	/** @brief Nudges a unit off its ideal slot when that dodges exclusions or blocked terrain. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement Adjustment")
	bool bAdjustPlacementsToAvoidExclusions = true;

	/** @brief Furthest a unit may be nudged away from its ideal formation slot. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement Adjustment",
		meta = (ClampMin = "0", Units = "cm", EditCondition = "bAdjustPlacementsToAvoidExclusions"))
	float MaxPlacementAdjustment = 600.0f;

	/** @brief Radial step between attempted nudge rings around the ideal slot. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement Adjustment",
		meta = (ClampMin = "25", Units = "cm", EditCondition = "bAdjustPlacementsToAvoidExclusions"))
	float PlacementAdjustmentStep = 150.0f;

	// --- Terrain ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceUp = 15000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceDown = 40000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 55.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bAlignUnitsToGround = true;
};

/**
 * @brief Main-thread execution element that measures configured unit Blueprints and registers all
 * generated splines and unit actors as PCG-managed resources.
 */
class FPCGCreateDivisionElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
