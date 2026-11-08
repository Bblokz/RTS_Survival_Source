// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "AbilityPreviewTargets.generated.h"

class UGlobalAbility;
struct FStreamableHandle;

/** What an attack ability's preview puts under the attack: the targets the ability is meant for. */
UENUM(BlueprintType)
enum class EAbilityPreviewTargetKind : uint8
{
	None,
	// Machine gun strafing (its 12 mm penetration cannot pierce even the trucks) and incendiary bombs.
	Infantry,
	// Cannon aircraft: the Horten 229's cannons, the Ju 87 G's 37 mm pods.
	LightVehicles,
	// Bombs: dive bombers, carpet and incendiary bombing.
	MediumVehicles
};

USTRUCT(BlueprintType)
struct FAbilityPreviewTargetSet
{
	GENERATED_BODY()

	// One unit per entry, spread evenly along the attack line.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets")
	TArray<TSoftClassPtr<AActor>> UnitClasses;
};

/** Health of the spawned targets, read by the preview test. */
struct FAbilityPreviewTargetHealth
{
	// Vehicles and soldiers that were spawned.
	int32 Units = 0;
	// Units below full health, including destroyed ones.
	int32 Damaged = 0;
	// Units that are gone or at zero health.
	int32 Destroyed = 0;
};

/**
 * @brief Puts Soviet units under an attack ability's preview run, so strafing and bombing aircraft have something to
 * hit. AAbilityPreviewDirector asks GetTargetKindForAbility, spawns the targets along the attack line before the
 * ability executes and clears them when the preview stops. The targets are player 2 and stand still.
 */
UCLASS(ClassGroup = (AbilityPreview), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAbilityPreviewTargetsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityPreviewTargetsComponent();

	/**
	 * @brief The targets an ability is meant for: its entry in M_AbilityTargetKinds, else by ability class (carpet
	 * bombing takes medium vehicles, strafing and incendiary strikes take infantry); None for every other ability.
	 * @param Ability The previewed ability.
	 * @param AbilityCopy The preview's copy of its template.
	 * @return The kind of targets to spawn; None spawns nothing.
	 */
	EAbilityPreviewTargetKind GetTargetKindForAbility(const EGlobalAbility Ability,
	                                                  const UGlobalAbility& AbilityCopy) const;

	/**
	 * @brief Spawns the kind's units along the line the aircraft attack, alternating a little to either side of it.
	 * @param Kind Which set to spawn.
	 * @param LineStart Ground point where the attack run starts.
	 * @param LineEnd Ground point where it ends.
	 * @return How many units were spawned.
	 */
	int32 SpawnTargets(const EAbilityPreviewTargetKind Kind, const FVector& LineStart, const FVector& LineEnd);

	/** Destroys the targets of the last preview; safe without targets. */
	void ClearTargets();

	EAbilityPreviewTargetKind GetSpawnedKind() const { return M_SpawnedKind; }

	/** Spawned vehicles and squads (not the soldiers); null entries are destroyed ones. */
	const TArray<TWeakObjectPtr<AActor>>& GetTargets() const { return M_Targets; }

	FAbilityPreviewTargetHealth GetTargetHealth() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The units each kind spawns; the base (enemy) Blueprints, not the Soviet faction's player versions.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets")
	TMap<EAbilityPreviewTargetKind, FAbilityPreviewTargetSet> M_TargetSets;

	// Which targets an ability gets; abilities not listed fall back to their class (GetTargetKindForAbility).
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets")
	TMap<EGlobalAbility, EAbilityPreviewTargetKind> M_AbilityTargetKinds;

	// The targets' player: the aircraft are player 1.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets", meta = (ClampMin = "2"))
	uint8 M_TargetOwningPlayer = 2;

	// Targets alternate this far to either side of the attack line, so a run hits them without them lining up.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets", meta = (ClampMin = "0"))
	float M_SideOffset = 180.f;

	// Units stand this far in from both ends of the line.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets", meta = (ClampMin = "0", ClampMax = "0.45"))
	float M_LineEndMargin = 0.12f;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_Targets;

	// Soldiers of the target squads, collected as the squads spawn them.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_TargetSoldiers;

	EAbilityPreviewTargetKind M_SpawnedKind = EAbilityPreviewTargetKind::None;

	// Keeps every target class loaded, so spawning at preview start does not hitch.
	TSharedPtr<FStreamableHandle> M_PreloadHandle;

	FTimerHandle M_KeepPresentableTimerHandle;

	void PreloadTargetClasses();
	AActor* SpawnTarget(UClass* UnitClass, const FVector& GroundLocation, const float Yaw);
	FVector GetGroundPoint(const FVector& Location) const;

	// Each tick of the timer: soldiers take the squad's owner, nothing is hidden by the fog, no health bars show.
	void KeepTargetsPresentable();
	void CollectSquadSoldiers();
	static void PrepareTarget(AActor& Target);
	static bool GetIsUnitDestroyed(const AActor* Unit);
	static bool GetIsUnitDamaged(const AActor* Unit);
};
