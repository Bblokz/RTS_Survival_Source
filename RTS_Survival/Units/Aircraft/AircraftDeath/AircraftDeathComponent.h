// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AircraftDeathTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "AircraftDeathComponent.generated.h"

class AAircraftMaster;
class AAircraftWreckActor;
class UGeometryCollection;
class UNiagaraComponent;
class USkeletalMeshComponent;

/**
 * @brief Every aircraft owns one (created by AAircraftMaster); AAircraftMaster::UnitDies calls StartDeath. In the air
 * an engine catches fire (the nose engine of a single-engine aircraft, else a wing engine), the wreck falls on with
 * the speed it had and breaks into the pieces of its crash collection where it hits the ground. Hit while parked,
 * the aircraft breaks up where it stands. Ticks only while the wreck falls.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAircraftDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAircraftDeathComponent();

	/**
	 * @brief Starts the burning fall, or the breakup on the spot when the aircraft is not flying. Runs once.
	 * @param VelocityAtDeath Velocity of the aircraft when it was shot down, read before its movement stopped.
	 */
	void StartDeath(const FVector& VelocityAtDeath);

	EAircraftDeathPhase GetDeathPhase() const { return M_DeathPhase; }
	EAircraftFireOrigin GetFireOrigin() const { return M_FireOrigin; }
	EAircraftDamagedSide GetDamagedSide() const { return M_FlightState.DamagedSide; }
	FName GetFireAttachName() const { return M_FireAttachName; }
	bool GetIsFireBurning() const;
	bool GetHasCrashCollection() const;
	FVector GetCrashVelocity() const { return M_FlightState.Velocity; }
	float GetInitialCrashSpeed() const { return M_InitialCrashSpeed; }
	FVector GetImpactLocation() const { return M_ImpactLocation; }
	int32 GetBreakupPieceCount() const { return M_BreakupPieceCount; }
	int32 GetSmokePuffCount() const { return M_SmokePuffCount; }
	bool GetWasParked() const { return bM_WasParked; }
	float GetSizeEffectScaleOfAircraft() const { return M_SizeEffectScale; }
	AAircraftWreckActor* GetWreck() const { return M_Wreck.Get(); }

	const FAircraftDeathSettings& GetDeathSettings() const { return M_DeathSettings; }

#if RTS_WITH_MAP_TESTS
	/** @return Map tests: the settings, to switch effects off or swap them while judging how a crash looks. */
	FAircraftDeathSettings& MapTest_GetMutableDeathSettings() { return M_DeathSettings; }
#endif

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Death", meta=(AllowPrivateAccess="true"))
	FAircraftDeathSettings M_DeathSettings;

private:
	EAircraftDeathPhase M_DeathPhase = EAircraftDeathPhase::Alive;
	EAircraftFireOrigin M_FireOrigin = EAircraftFireOrigin::NoseEngine;
	FName M_FireAttachName = NAME_None;
	// Whether the engine still burns; its last flames and smoke live on after it stops at the impact.
	bool bM_IsFireBurning = false;
	bool bM_WasParked = false;
	FAircraftCrashFlightState M_FlightState;
	FVector M_ImpactLocation = FVector::ZeroVector;
	int32 M_BreakupPieceCount = 0;
	float M_InitialCrashSpeed = 0.f;
	// Half the height of the aircraft: the trace for the ground runs this far below its centre.
	float M_GroundClearance = 0.f;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_Aircraft;
	bool GetIsValidAircraft() const;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> M_AircraftMesh;

	// Loaded at BeginPlay so the impact does not wait on a load; empty when no collection was built for the meshes.
	UPROPERTY()
	TObjectPtr<UGeometryCollection> M_CrashCollection;

	// Set when a collection exists for the meshes, so a death before its load finished can still load it.
	FSoftObjectPath M_CrashCollectionPath;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_EngineFire;

	// Bigger aircraft get bigger effects; measured once from the meshes at BeginPlay.
	float M_SizeEffectScale = 1.f;
	float GetSizeEffectScale() const;
	/** @return The aircraft's effect scale times its size. */
	float GetEffectScale() const;

	// Where on the mesh the fire burns when it is not on a bone; smoke puffs start there too.
	FVector M_FireLocalOffset = FVector::ZeroVector;
	float M_DistanceSinceSmokePuff = 0.f;
	int32 M_SmokePuffCount = 0;

	UPROPERTY()
	TWeakObjectPtr<AAircraftWreckActor> M_Wreck;

	void BeginPlay_LoadCrashCollection();
	FString GetCrashCollectionPath() const;

	/** @brief Chooses the burning engine from the propeller bones and starts its fire and smoke. */
	void StartEngineFire();
	void ChooseFireAttachPoint(USkeletalMeshComponent& Mesh, FVector& OutLocalLocation);
	/** @brief The propeller bones of the mesh and where they are in its space. */
	static void GatherEngineBones(const USkeletalMeshComponent& Mesh, TArray<FName>& OutBones,
	                              TArray<FVector>& OutLocations);
	void StartCrashFlight(const FVector& VelocityAtDeath);
	/** @return Whether the aircraft stands on the ground (in its bay or on a field), so there is nothing to fall from. */
	bool GetIsParkedOnTheGround() const;
	void TickCrashFlight(float DeltaTime);
	/** @brief Leaves a smoke puff behind the burning engine every SmokeTrailSpacing of the fall. */
	void DropSmokePuffs();

	/** @return Whether the wreck reached the ground (or something static) on its way to NewLocation. */
	bool FindImpact(const FVector& OldLocation, const FVector& NewLocation, FHitResult& OutHit) const;

	/** @brief The wreck hit the ground: explosion, burning fuel, the pieces, and the aircraft is cleared away. */
	void Impact(const FVector& ImpactLocation, const FVector& ImpactNormal);
	void Impact_SpawnEffects(const FVector& ImpactLocation, const FVector& ImpactNormal) const;
	void Impact_SpawnWreck();
	// Fallback for the wreck height when the crash collection is missing; the padded mesh bounds sit too low.
	float GetLowestVisibleMeshPointZ() const;
	/** @return The highest ground (static geometry) under the box, sampled on a grid; lowest float when none. */
	float GetHighestGroundUnder(const FBox& WorldBox) const;
	void Impact_HideAircraft();
	void Impact_LetTrailsFade();
};
