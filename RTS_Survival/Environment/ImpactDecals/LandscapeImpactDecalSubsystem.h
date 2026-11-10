// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LandscapeImpactDecalSubsystem.generated.h"

class AActor;
class UDecalComponent;
class UMaterialInterface;
struct FRTSImpactDecalBand;
struct FRTSBombCraterBand;

/** Running totals of the crater pool, read by the impact decal benchmark. */
struct FLandscapeImpactDecalStats
{
	int32 Requests = 0;
	int32 AddedDecals = 0;
	int32 CreatedComponents = 0;
	int32 RecycledComponents = 0;
	int32 MergedIntoExisting = 0;
	int32 BombCraters = 0;
	double AddSeconds = 0.0;
};

/**
 * @brief Shell craters on the landscape. A shell larger than 15 mm (URTSImpactDecalSettings::MinCalibreMm) that hits
 * a landscape leaves a deferred decal at the impact, sized by its calibre band, until newer craters push it out.
 * Works on every map: the subsystem lives in every game world and only reacts to hits on a landscape actor.
 *
 * Cost model: all craters share one material and one transient owner actor; the decals are a ring of
 * UDecalComponents created once each and then recycled (new transform + Decal Color), so a crater never spawns an
 * actor, a material instance or garbage. The ring is as long as rts.ImpactDecals (0 = off: every decal is destroyed
 * and nothing is spawned); when it is full the oldest crater is moved to the new impact.
 * A crater landing inside a same-or-larger crater adds nothing, so barrages on one spot do not stack overdraw.
 * Bombs and missiles leave their own crater set, picked by explosive weight (RequestBombImpactDecal); they share the
 * ring and the cap with the shell craters and only differ in material (one per set, swapped on recycle) and size.
 */
UCLASS()
class RTS_SURVIVAL_API ULandscapeImpactDecalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Entry point for weapon impacts: leaves a crater when the shell is large enough, craters are on and the hit
	 * actor is a landscape. Cheap to call for every impact (calibre and setting are checked before anything else).
	 * @param WorldContext Any object in the world that was hit.
	 * @param CalibreMm Calibre of the shell in mm.
	 * @param ImpactLocation Point on the landscape.
	 * @param ImpactNormal Landscape normal at the impact; the decal projects along its opposite.
	 * @param HitActor The actor the shell hit; nothing happens unless it is a landscape.
	 */
	static void RequestImpactDecal(const UObject* WorldContext, float CalibreMm, const FVector& ImpactLocation,
	                               const FVector& ImpactNormal, const AActor* HitActor);

	/**
	 * @brief For explosions without a hit (flight timer ran out): leaves a crater when landscape lies within
	 * MaxTimedExplosionGroundGapCm below, so artillery timed to land at its target still marks the ground.
	 * One short line trace, only for shells large enough while craters are on.
	 * @param WorldContext Any object in the world.
	 * @param CalibreMm Calibre of the shell in mm.
	 * @param ExplosionLocation Where the shell went off.
	 */
	static void RequestImpactDecalBelowExplosion(const UObject* WorldContext, float CalibreMm,
	                                             const FVector& ExplosionLocation);

	/**
	 * @brief Entry point for bomb and missile impacts: leaves a bomb crater (sized by explosive weight, its own atlas)
	 * when craters are on and the hit actor is a landscape. Never uses the shell bands: a bomb's calibre says nothing
	 * about its crater.
	 * @param WorldContext Any object in the world that was hit.
	 * @param TntGrams The weapon's TNTExplosiveGrams.
	 * @param ImpactLocation Point on the landscape.
	 * @param ImpactNormal Landscape normal at the impact.
	 * @param HitActor The actor the bomb hit; nothing happens unless it is a landscape.
	 */
	static void RequestBombImpactDecal(const UObject* WorldContext, float TntGrams, const FVector& ImpactLocation,
	                                   const FVector& ImpactNormal, const AActor* HitActor);

	// True when the actor is (part of) a landscape.
	static bool GetIsLandscapeActor(const AActor* Actor);

	// Most craters at once, from rts.ImpactDecals; 0 = off.
	static int32 GetMaxDecalsSetting();

	/**
	 * @brief Adds a crater without the landscape check (the caller knows it hit a landscape).
	 * @return True when a decal was placed or an existing crater already covers the spot.
	 */
	bool AddImpactDecal(float CalibreMm, const FVector& ImpactLocation, const FVector& ImpactNormal);

	/**
	 * @brief Adds a bomb crater without the landscape check (the caller knows it hit a landscape).
	 * @return True when a decal was placed or an existing crater already covers the spot.
	 */
	bool AddBombImpactDecal(float TntGrams, const FVector& ImpactLocation, const FVector& ImpactNormal);

	// Removes every crater and its component.
	void ClearDecals();

	int32 GetDecalCount() const { return M_DecalCount; }
	// Centres of the craters on the map (same order as the ring).
	const TArray<FVector>& GetCraterCentres() const { return M_Centres; }
	// Radii of the craters on the map (same order as the ring), in cm.
	const TArray<float>& GetCraterRadii() const { return M_Radii; }
	// True for the slot's crater when it is a bomb crater (same order as the ring).
	bool GetIsBombCrater(int32 Slot) const;

	/**
	 * @brief Test builds only: impacts of large enough shells that reached the landscape check since the last reset,
	 * split into landscape hits (crater requested) and hits on anything else (no crater). Always 0 in shipping.
	 */
	static void GetMapTestImpactCounts(int32& OutLandscapeImpacts, int32& OutOtherImpacts);
	// Test builds only: bomb and missile impacts on the landscape since the last reset (part of the landscape count).
	static int32 GetMapTestBombLandscapeImpacts();
	static void ResetMapTestImpactCounts();
	// Test builds only: "Class xN" for the actor classes the non-landscape impacts hit, most frequent first.
	static FString DescribeMapTestOtherHitClasses();
	int32 GetPooledComponentCount() const { return M_Decals.Num(); }
	const FLandscapeImpactDecalStats& GetStats() const { return M_Stats; }
	void ResetStats() { M_Stats = FLandscapeImpactDecalStats(); }

private:
	// Ring of crater decals; when full, M_NextSlot is the oldest. Parallel arrays hold each slot's crater.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> M_Decals;

	TArray<FVector> M_Centres;
	TArray<float> M_Radii;
	// Size rank of each crater for merging: shell band index, bomb bands after all shell bands.
	TArray<uint8> M_SizeRanks;

	// Slot the next crater goes into.
	int32 M_NextSlot = 0;
	// Craters on the map; equals M_Decals.Num().
	int32 M_DecalCount = 0;
	// Ring length currently applied (rts.ImpactDecals when last read).
	int32 M_AppliedMaxDecals = 0;
	// Sort order offset of the next crater, so newer craters draw over older ones.
	int32 M_NextSortOffset = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> M_DecalOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> M_DecalMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> M_BombDecalMaterial = nullptr;

	bool bM_HasReportedMissingMaterial = false;
	bool bM_HasReportedMissingBombMaterial = false;
	FDelegateHandle M_CVarChangedHandle;
	FLandscapeImpactDecalStats M_Stats;

	void OnMaxDecalsChanged(IConsoleVariable* Variable);
	// Shrinks the ring to the newest NewMaxDecals craters (destroying the rest) or makes room to grow.
	void ApplyMaxDecals(int32 NewMaxDecals);
	// Puts the ring in age order, oldest first, with the next slot after the newest.
	void ReorderOldestFirst();
	void DestroyAllDecals();

	/** What one crater needs, from a shell band or a bomb band. */
	struct FCraterSpec
	{
		UMaterialInterface* Material = nullptr;
		float Radius = 0.f;
		float HalfDepth = 0.f;
		int32 FirstAtlasCell = 0;
		int32 AtlasCellCount = 1;
		uint8 SizeRank = 0;
	};

	// Finds the subsystem for a crater request on a landscape hit (counts the hit for map tests) and the crater
	// location snapped onto the drawn heightfield; nullptr when the hit is not on a landscape.
	static ULandscapeImpactDecalSubsystem* GetSubsystemForLandscapeHit(const UObject* WorldContext,
	                                                                   const AActor* HitActor,
	                                                                   const FVector& ImpactLocation,
	                                                                   bool bIsBomb, FVector& OutCraterLocation);
	// Places (or merges) one crater; shared by shells and bombs.
	bool AddCrater(const FCraterSpec& Spec, const FVector& ImpactLocation, const FVector& ImpactNormal,
	               uint64 StartCycles);

	bool GetIsReadyToSpawn();
	// Loads the bomb crater material on first use; false (warned once) when it is missing.
	bool GetIsBombMaterialReady();
	bool EnsureDecalOwner();
	// True when a same-or-larger crater already covers the new impact.
	bool GetIsCoveredByExistingCrater(const FVector& Centre, float Radius, int32 SizeRank) const;
	UDecalComponent* CreateDecalComponent(UMaterialInterface* Material);
	static FTransform MakeDecalTransform(const FVector& Location, const FVector& ImpactNormal, float Radius,
	                                     float HalfDepth);
	static FLinearColor MakeDecalColor(int32 FirstAtlasCell, int32 AtlasCellCount);
};
