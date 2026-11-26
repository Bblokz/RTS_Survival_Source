// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantProbes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "TechWheelEffectProbes.generated.h"

class UTechnologyEffect;
struct FStreamableHandle;

/** One weapon a technology changes: its stats on an affected unit without and with the technology. */
USTRUCT()
struct FTechWheelWeaponChange
{
	GENERATED_BODY()

	UPROPERTY()
	FUnitVariantWeaponStats Before;

	UPROPERTY()
	FUnitVariantWeaponStats After;
};

/** What the probes of one technology read. */
USTRUCT()
struct FTechWheelEffectResult
{
	GENERATED_BODY()

	// One entry per distinct weapon with a changed damage, penetration, reload or magazine.
	UPROPERTY()
	TArray<FTechWheelWeaponChange> WeaponChanges;

	// For the dump: how many units were spawned and how many weapons were read off them.
	UPROPERTY()
	int32 ProbedUnitCount = 0;

	UPROPERTY()
	int32 WeaponsRead = 0;
};

/**
 * Reads a technology's effect class without a mission: which units it applies to, straight from the class defaults.
 */
namespace TechWheelEffects
{
	/** @return The loaded effect class of the tech wheel settings; null when the technology has none. */
	UClass* LoadEffectClass(const ETechnology Technology);

	/** @return Every unit the effect lists, tanks first, as the training options the name and icon tables use. */
	TArray<FTrainingOption> GetAffectedUnits(const UTechnologyEffect& Effect);

	/** @return Whether the effect hands out a weapon behaviour; only then are vehicles worth spawning to read. */
	bool GetCanChangeWeapons(const UTechnologyEffect& Effect);
}

/**
 * @brief What a technology does to weapons is set by behaviour Blueprints, so the tech wheel learns it the way the
 * unit variant screen learns a variant: it spawns each affected vehicle hidden far below the map, reads its weapons,
 * applies the technology and reads them again. Results are kept per technology. Owned by UW_WorldTechWheel.
 */
UCLASS()
class RTS_SURVIVAL_API UTechWheelEffectProbes : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Probes the technology unless it is known; calls OnRead once its result is there (at once when it is
	 * known or has nothing to probe). A new call cancels probes that are still settling.
	 * @param World Game world to spawn in.
	 * @param Technology Technology to read.
	 * @param OnRead Called on the game thread when FindResult has an answer.
	 */
	void ProbeTechnology(UWorld& World, const ETechnology Technology, TFunction<void()> OnRead);

	/** @return Null when the technology was not probed (yet). */
	const FTechWheelEffectResult* FindResult(const ETechnology Technology) const;

	/** Destroys probes that are still settling, without reading them; the tech wheel calls it when it closes. */
	void CancelProbes();

private:
	// A spawned vehicle and what it read before the technology was applied.
	struct FLiveProbe
	{
		TWeakObjectPtr<AActor> Actor;
		FUnitVariantProbeResult Before;
	};

	UPROPERTY()
	TMap<ETechnology, FTechWheelEffectResult> M_Results;

	// Kept alive between applying and reading: behaviours it added may still point at it.
	UPROPERTY()
	TObjectPtr<UTechnologyEffect> M_AppliedEffect;

	TArray<FLiveProbe> M_LiveProbes;
	TWeakObjectPtr<UWorld> M_World;
	TSharedPtr<FStreamableHandle> M_LoadHandle;
	FTimerHandle M_StepTimer;
	TFunction<void()> M_OnRead;
	ETechnology M_ProbedTechnology = ETechnology::Tech_NONE;
	TSubclassOf<UTechnologyEffect> M_ProbedEffectClass;

	/** @return The classes of the vehicles and buildings the effect lists; squads and aircraft have no probe. */
	static TArray<FSoftObjectPath> FindProbeClassPaths(UWorld& World, const UTechnologyEffect& Effect);
	void SpawnProbes(const TArray<FSoftObjectPath>& ClassPaths);
	void ReadBeforeAndApply();
	void ReadAfterAndDestroy();
	void FinishProbing(const FTechWheelEffectResult& Result);
	static void AddWeaponChanges(const FUnitVariantProbeResult& Before, const FUnitVariantProbeResult& After,
	                             FTechWheelEffectResult& InOutResult);
};
