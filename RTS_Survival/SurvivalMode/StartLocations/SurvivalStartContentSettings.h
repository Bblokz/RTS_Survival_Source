// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"

#include "SurvivalStartContentSettings.generated.h"

class AActor;
class UStaticMesh;

/**
 * @brief What a planned start-content spawn is; drives placement order, debug colours and spawn bookkeeping.
 */
UENUM(BlueprintType)
enum class ESurvivalStartContentCategory : uint8
{
	Radixite,
	Metal,
	Wreck,
	Pit,
	Crater
};

/**
 * @brief One Blueprint that may be picked for a start-content slot.
 * FootprintRadius is the horizontal radius used for spacing (Blueprints are not measured at runtime).
 */
USTRUCT(BlueprintType)
struct FSurvivalWeightedActorClass
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// Horizontal radius used to keep this actor clear of other placed content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "10.0", Units = "cm"))
	float FootprintRadius = 300.f;

	// Added to the generated yaw for assets whose visual forward axis is not +X.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.f;

	// Random yaw; when false the planner's yaw (e.g. facing away from the base) is used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRandomYaw = true;

	// Tilt the actor to the landscape normal (wrecks, debris); upright otherwise (buildings, pits).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAlignToGround = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.f;
};

/**
 * @brief One static mesh used for the light foliage scatter (spawned as instances, no collision).
 */
USTRUCT(BlueprintType)
struct FSurvivalWeightedMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.01"))
	float MinScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.01"))
	float MaxScale = 1.25f;
};

/**
 * @brief A resource field type (radixite or metal) generated in a ring just outside the build area.
 * Every field gets one centre actor (growth crystal, metal tower) and a cluster of field actors around it.
 */
USTRUCT(BlueprintType)
struct FSurvivalResourceFieldSettings
{
	GENERATED_BODY()

	// Fields at Normal difficulty; scaled by DifficultyScaling.ResourceFieldCountMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count", meta = (ClampMin = "0"))
	int32 FieldCount = 2;

	// Empty ground between the build area edge and the nearest edge of a field.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float GapFromBuildEdge = 600.f;

	// Each field is pushed outward by a random 0..this extra distance so the ring does not look drawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxExtraOutwardOffset = 1200.f;

	// Radius of one field; the centre actor sits in the middle, field actors fill the disc.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "300.0", Units = "cm"))
	float FieldRadius = 1800.f;

	// One is picked per field and placed at its centre (optional).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actors")
	TArray<FSurvivalWeightedActorClass> CentreActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actors")
	TArray<FSurvivalWeightedActorClass> FieldActors;

	// Field actors per field at Normal difficulty; scaled by DifficultyScaling.ResourceFieldActorMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	int32 MinFieldActors = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	int32 MaxFieldActors = 8;

	// Empty space kept between the footprints of two actors in the same field.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0.0", Units = "cm"))
	float ActorClearance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxGroundSlopeDegrees = 30.f;
};

/**
 * @brief A group of scavengable wrecks placed in a band a bit further out than the resource fields.
 */
USTRUCT(BlueprintType)
struct FSurvivalWreckGroup
{
	GENERATED_BODY()

	// Only for readability in the details panel and the debug preview.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks")
	FName GroupName = TEXT("Wrecks");

	// Wrecks at Normal difficulty; scaled by DifficultyScaling.WreckCountMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks", meta = (ClampMin = "0"))
	int32 Count = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks", meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistanceFromBuildEdge = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxDistanceFromBuildEdge = 9000.f;

	// Minimum centre-to-centre distance between two wrecks of any group.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpacing = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxGroundSlopeDegrees = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks")
	TArray<FSurvivalWeightedActorClass> Wrecks;
};

/**
 * @brief Capturable pits placed on the start location's defense points (designer points first, then hill tops).
 */
USTRUCT(BlueprintType)
struct FSurvivalPitSettings
{
	GENERATED_BODY()

	// Pits at Normal difficulty; scaled by DifficultyScaling.PitCountMultiplier and capped by the defense points.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits", meta = (ClampMin = "0"))
	int32 PitCount = 3;

	// Classes must implement ICaptureInterface (ACaptureActor children) so the player can take them over.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits")
	TArray<FSurvivalWeightedActorClass> Pits;

	// Pits look outward, away from the deploy centre (ignored for entries with bRandomYaw).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits")
	bool bFaceAwayFromDeployCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits", meta = (ClampMin = "0.0", Units = "cm"))
	float MinPitSpacing = 2000.f;

	// Defense points steeper than this are skipped for pits.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxGroundSlopeDegrees = 30.f;
};

/**
 * @brief Light dressing around the start: crater decal actors and instanced foliage meshes without collision.
 */
USTRUCT(BlueprintType)
struct FSurvivalClutterSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistanceFromBuildEdge = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxDistanceFromBuildEdge = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Craters", meta = (ClampMin = "0"))
	int32 CraterCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Craters")
	TArray<FSurvivalWeightedActorClass> CraterActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage", meta = (ClampMin = "0"))
	int32 FoliageInstanceCount = 80;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	TArray<FSurvivalWeightedMesh> FoliageMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float FoliageMaxGroundSlopeDegrees = 40.f;

	// 0 keeps the mesh defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage", meta = (ClampMin = "0.0", Units = "cm"))
	float FoliageCullDistance = 20000.f;
};

/**
 * @brief Multipliers applied to the Normal-difficulty counts; counts are rounded and never drop below 1
 * when the base count is at least 1 (resource fields only; wrecks and pits may reach 0).
 */
USTRUCT(BlueprintType)
struct FSurvivalStartContentDifficultyScale
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float ResourceFieldCountMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float ResourceFieldActorMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float WreckCountMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float PitCountMultiplier = 1.f;
};

/**
 * @brief What is generated around the start location the player picked: radixite and metal fields close by for
 * the harvesters, vehicle wrecks to scavenge a bit further out, capturable pits on the defense points and light
 * craters/foliage. Referenced by USurvivalModeSettings::StartContent; read by ASurvivalStartLocation.
 * @note The constructor fills verified project Blueprints, so a freshly created asset works out of the box.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalStartContentSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USurvivalStartContentSettings();

	/** @return The multipliers for this difficulty; identity when the map has no entry for it. */
	FSurvivalStartContentDifficultyScale GetDifficultyScale(const ERTSGameDifficulty Difficulty) const;

	/** @return The furthest distance beyond the build edge any start content can be placed at. */
	float GetOuterContentDistanceFromBuildEdge() const;

	// 0 = a new layout every match; any other value always produces the same layout (testing).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources")
	FSurvivalResourceFieldSettings RadixiteFields;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources")
	FSurvivalResourceFieldSettings MetalFields;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrecks")
	TArray<FSurvivalWreckGroup> WreckGroups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits")
	FSurvivalPitSettings Pits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter")
	FSurvivalClutterSettings Clutter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TMap<ERTSGameDifficulty, FSurvivalStartContentDifficultyScale> DifficultyScaling;

	// Resource fields, wrecks and craters stay this far from every defense point (keeps hill tops free).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", Units = "cm"))
	float DefensePointReserveRadius = 900.f;

	// Added to the build radius of every other start location: no player content there (Soviet bases go there).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", Units = "cm"))
	float OtherStartLocationClearance = 3000.f;

	// Empty space between two resource fields.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", Units = "cm"))
	float FieldClearance = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerItem = 48;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceUp = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceDown = 50000.f;

private:
	void InitDefaultRadixiteFields();
	void InitDefaultMetalFields();
	void InitDefaultWreckGroups();
	void InitDefaultPits();
	void InitDefaultClutter();
	void InitDefaultDifficultyScaling();
};
