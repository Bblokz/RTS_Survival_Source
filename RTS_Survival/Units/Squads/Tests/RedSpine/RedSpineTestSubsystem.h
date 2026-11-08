// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RedSpineTestSubsystem.generated.h"

class AAircraftMaster;
class ACameraActor;
class ASquadController;
class ASquadUnit;
class UGlobalAbility;

UENUM()
enum class ERedSpineTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	MovingSpawnedSquad,
	AttackingGround,
	RunningDrop,
	MovingDroppedSquads,
	Finished
};

/**
 * @brief -RedSpineValidate: spawns one Squad_Ger_RedSpine through the async spawner and checks it (3 Red Spine
 * walkers, player 1, subtype, health and armour from the squad data, MP46s), orders it to move and to attack the
 * ground, then fires GA_GerRedSpineDrop and GA_GerRedSpinePairDrop. For each drop it checks the landed Ju 390 lets
 * out the right number of squads, that the walkers stand on the ground beside the aircraft (not on it, not inside its
 * collision, not falling), survive its take-off and follow a move order. Anchor: -RedSpineAnchorX= -RedSpineAnchorY=
 * (else where the camera looks); the pair drop lands -RedSpinePairOffset= (default 4500) on the far side of the
 * drop point.
 * Screenshots into Saved/RedSpineTest. Logs RTS_REDSPINE_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API URedSpineTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ERedSpineTestPhase M_Phase = ERedSpineTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_DirectSpawnLocation = FVector::ZeroVector;
	FVector M_MoveTarget = FVector::ZeroVector;
	// Landscape extent of the map and the direction from the anchor to its centre; keeps test spots on the map.
	FBox M_LandscapeBounds = FBox(ForceInit);
	FVector M_TowardCentre = FVector(0.f, 1.f, 0.f);
	bool bM_AttackGroundFired = false;

	// Index into the drop list (single, pair) of the drop that runs.
	int32 M_DropIndex = 0;
	FVector M_DropAnchor = FVector::ZeroVector;
	bool bM_DropEnded = false;
	bool bM_CheckedLandedState = false;
	float M_FirstSquadSeenSeconds = -1.f;
	float M_SquadsCompleteSeconds = -1.f;
	float M_DropEndedSeconds = -1.f;
	int32 M_MostDropAircraftSeen = 0;

	// Nothing else holds the running ability; its load callbacks and timers outlive the start of the drop.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	// Earlier drops stay referenced: their aircraft are still flying out and call back into the ability.
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_StartedDrops;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	// Every Red Spine squad already in the world before the running drop started.
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_KnownSquads;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_DroppedSquads;

	// Move order per dropped squad (same order as M_DroppedSquads): where it started and where it was sent.
	TArray<FVector> M_DroppedMoveStarts;
	TArray<FVector> M_DroppedMoveTargets;

	// The Ju 390 of the running drop nearest the drop point, kept from landing to take-off.
	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_DropAircraft;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterPhase(ERedSpineTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartDirectSpawn();
	void OnDirectSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	void TickMovingSpawnedSquad();
	void TickAttackingGround();

	/** @brief Squad-level checks shared by the spawned and the dropped squads. */
	void CheckSquad(ASquadController* Squad, const FString& Label);
	/** @brief Every walker stands on the ground: not falling, nothing but terrain below it, feet near that ground. */
	void CheckWalkersOnGround(ASquadController* Squad, const FString& Label);
	/** @brief No walker overlaps the landed aircraft's collision or stands in or under its airframe (wings included). */
	void CheckWalkersOutsideAircraft(const FString& Label);

	void StartDrop(int32 DropIndex);
	void TickRunningDrop();
	void CollectNewSquads();
	void FinishDropChecks();
	void TickMovingDroppedSquads();

	static bool GetHasNoResistanceOverride(const ASquadUnit* Unit);
	FBox FindLandscapeBounds() const;
	/** @brief Clamps a test location into the landscape (minus a margin) and puts it on the ground. */
	FVector KeepInsideLandscape(const FVector& Location) const;
	static FVector GetSquadCentre(ASquadController* Squad);
	void ViewFrom(const FVector& Focus, float Distance, float Pitch, float Yaw = 135.f) const;
	void RequestScreenshot(const FString& Label);
};
