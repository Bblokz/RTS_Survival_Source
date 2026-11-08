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
	/** @return Pieces that tore off while the wreck was still in the air. */
	int32 GetAirBreakupPieceCount() const { return M_AirBreakupPieceCount; }
	/** @return Whether the wreck is still tearing apart and its blasts are still to come after the impact. */
	bool GetIsImpactStaging() const { return bM_IsImpactStaging; }
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

	FRandomStream M_Random;
	bool bM_AircraftMeshesHidden = false;

	// In the air.
	float M_AirBreakupSeconds = -1.f;
	bool bM_DidFirstAirBreakup = false;
	bool bM_DidSecondAirBreakup = false;
	TArray<int32> M_AirBreakupOrder;
	int32 M_NextAirBreakupIndex = 0;
	int32 M_AirBreakupPieceCount = 0;
	int32 M_SmokingTornPiece = INDEX_NONE;
	FVector M_SmokingTornPieceLastLocation = FVector::ZeroVector;
	float M_DistanceSinceTornPartPuff = 0.f;
	float M_TornPartSmokeSeconds = 0.f;

	// On the ground.
	bool bM_IsImpactStaging = false;
	bool bM_HullFinished = false;
	float M_ImpactStageSeconds = 0.f;
	float M_ImpactStageDuration = 0.f;
	FVector M_ContactPoint = FVector::ZeroVector;
	FVector M_ContactNormal = FVector::UpVector;
	FVector M_SkidDirection = FVector::ForwardVector;
	float M_SkidStartSpeed = 0.f;
	float M_SkidDistance = 0.f;
	FQuat M_HullStartRotation = FQuat::Identity;
	FQuat M_HullLevelRotation = FQuat::Identity;
	FVector M_HullScale = FVector::OneVector;
	// Where the nose touched, in the hull's own space: the hull pivots on it as the tail comes down.
	FVector M_PivotLocal = FVector::ZeroVector;
	// How far the belly hangs below the pivot once the hull lies flat.
	float M_BellyDepth = 0.f;
	float M_NoseLocalX = 0.f;
	float M_TailLocalX = 0.f;
	bool bM_MainBlastDone = false;
	bool bM_FuelBlastDone = false;
	bool bM_DebrisFiresDone = false;
	TArray<float> M_SecondaryBlastSeconds;
	int32 M_NextSecondaryBlast = 0;

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

	/**
	 * @return Whether the wreck reached the ground (or something static) on its way to NewLocation: with its belly or,
	 * first in a dive, with its nose (bOutNoseHit).
	 */
	bool FindImpact(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation,
	                const FQuat& NewRotation, FHitResult& OutHit, bool& bOutNoseHit) const;

	/** @brief The wreck hit the ground: explosion, burning fuel, the pieces, and the aircraft is cleared away. */
	void Impact(const FVector& ImpactLocation, const FVector& ImpactNormal, bool bTearApartFromTheNose);
	void Impact_SpawnEffects(const FVector& ImpactLocation, const FVector& ImpactNormal) const;
	void Impact_SpawnWreck();
	/** @brief Deactivates every effect on the aircraft actor (exhausts, the engine fire): they burn out and stop. */
	void Impact_StopAircraftEffects();

	/** @return Spawns one death effect in the world, scaled by the aircraft's size times ScaleMultiplier. */
	UNiagaraComponent* SpawnDeathEffect(const FAircraftDeathEffect& Effect, const FVector& Location,
	                                    const FRotator& Rotation, float ScaleMultiplier = 1.f) const;
	/** @brief One smoke puff; it bursts once and is then switched off, so it never loops on where it was left. */
	void SpawnSmokePuff(const FVector& Location, float Scale);

	// ---- In the air: the collection replaces the aircraft's meshes and parts tear off. ----
	/** @return Whether the kinematic wreck now carries the aircraft's look; spawns it when there is none yet. */
	bool EnsureCarriedWreck();
	void HideAircraftMeshes() const;
	/** @brief Orders the pieces that tear off first: the burning wing's outer panel, or a single-engine's tail or a wing tip. */
	void BuildAirBreakupOrder();
	void TickAirBreakup();
	/** @brief Lets the next pieces of the breakup order go with a small blast where they tear off. */
	void TearOffPieces(int32 Count);
	void TickCarriedWreck(float DeltaTime);
	/** @brief Smoke from the first torn-off part while it tumbles down. */
	void TickTornPartSmoke();

	// ---- On the ground: the hull ploughs on and comes apart from the nose back while it explodes. ----
	void StartImpactStage(const FVector& ContactPoint, const FVector& ContactNormal);
	/** @brief Skid direction and speed, the flat pose the hull settles into, and the nose pivot it turns on. */
	void StartImpactStage_SetupHullMotion(const AAircraftWreckActor& Wreck);
	void StartImpactStage_ScheduleBlasts();
	void TickImpactStage(float DeltaTime);
	void ImpactStage_MoveHull(float StageAlpha);
	void ImpactStage_ReleasePieces(float StageAlpha);
	void ImpactStage_FinishHull();
	void ImpactStage_SpawnBlasts();
	void ImpactStage_SpawnBurningDebris();
	/** @return A random piece that was let go, or INDEX_NONE. */
	int32 GetRandomReleasedPiece();
	/** @return Furthest forward point of the aircraft along its nose, from its centre: the nose meets the ground first. */
	float GetNoseReach() const;
	// Fallback for the wreck height when the crash collection is missing; the padded mesh bounds sit too low.
	float GetLowestVisibleMeshPointZ() const;
	/** @return The highest ground (static geometry) under the box, sampled on a grid; lowest float when none. */
	float GetHighestGroundUnder(const FBox& WorldBox) const;
	void Impact_HideAircraft();
	void Impact_LetTrailsFade();
};
