// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RailTrooperAnimTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class ASquadUnit;
class UGlobalAbility;

UENUM()
enum class ERailTrooperAnimTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	MovingSpawnedSquad,
	AttackingGround,
	RunningParadrop,
	MovingDroppedSquads,
	Finished
};

/**
 * @brief -RailTrooperAnimValidate: proves the Rail Troopers animate with the shared infantry AnimBP (SK_RailTrooper on
 * InfantryMaster_Skeleton). Spawns one Squad_Ger_RailTroopers, checks mesh, skeleton, anim instance and the M920 in the
 * right hand, measures bone motion and distance from the reference pose while idle, walking and firing, then fires
 * GA_GerRailTroopersParadrop and repeats the checks on the landed squads. Anchor: -RailTrooperAnchorX= / AnchorY=.
 * Screenshots into Saved/RailTrooperSkeletonTest. Logs RTS_RAILTROOPER_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API URailTrooperAnimTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ERailTrooperAnimTestPhase M_Phase = ERailTrooperAnimTestPhase::WaitingForStart;
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
	bool bM_WalkShotTaken = false;
	// When the spawned squad stood still after walking off its spawn spot; the idle window starts there.
	float M_IdleSettledSeconds = -1.f;

	// Largest component-space travel of a trooper's left foot and right hand since the phase began, and the largest
	// distance of the right hand from where the reference pose puts it: a trooper left in the reference pose scores 0
	// travel and 0 offset.
	float M_MaxFootTravel = 0.f;
	float M_MaxHandTravel = 0.f;
	float M_MaxHandOffsetFromRefPose = 0.f;
	float M_MaxTrooperSpeed = 0.f;
	TArray<FVector> M_FootStartPositions;
	TArray<FVector> M_HandStartPositions;
	TSet<FString> M_MontagesSeen;

	FVector M_DropAnchor = FVector::ZeroVector;
	// First time every dropped trooper stood on the ground (not falling); the landed checks run a moment later.
	float M_DroppedLandedSeconds = -1.f;
	bool bM_DropIdleSampled = false;

	// Nothing else holds the running ability; its load callbacks and timers outlive the start of the drop.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	// Every Rail Trooper squad already in the world before the paradrop started.
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_KnownSquads;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_DroppedSquads;

	// Move order per dropped squad (same order as M_DroppedSquads).
	TArray<FVector> M_DroppedMoveStarts;
	TArray<FVector> M_DroppedMoveTargets;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterPhase(ERailTrooperAnimTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartDirectSpawn();
	void OnDirectSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	void TickMovingSpawnedSquad();
	void TickAttackingGround();

	void StartParadrop();
	void TickRunningParadrop();
	void CollectNewSquads();
	/** @return True once every dropped squad is loaded and all of its troopers walk on the ground, shown. */
	bool GetAreDroppedSquadsLanded() const;
	void StartDroppedSquadsMove();
	static bool GetAreTroopersStanding(ASquadController* Squad);
	void TickMovingDroppedSquads();

	/** @brief Squad, unit and weapon classes plus owner; shared by the spawned and the dropped squads. */
	void CheckSquad(ASquadController* Squad, const FString& Label);
	/** @brief Mesh, skeleton, anim instance and rail rifle in the right hand for every trooper of the squad. */
	void CheckTrooperAnimationSetup(ASquadController* Squad, const FString& Label);
	/** @brief The phase's pose samples show an animated (not reference) pose; walking also needs a moving foot. */
	void CheckPoseAnimated(const FString& Label, bool bExpectWalkCycle);
	void ResetPoseTracking();
	/** @brief Samples foot/hand travel, hand offset from the reference pose, trooper speed and active montages. */
	void SamplePose(ASquadController* Squad);
	void SampleTrooperPose(const ASquadUnit& Unit, int32 Index, bool bFirstSample);
	void LogPoseTracking(const FString& Label) const;

	FBox FindLandscapeBounds() const;
	/** @brief Clamps a test location into the landscape (minus a margin) and puts it on the ground. */
	FVector KeepInsideLandscape(const FVector& Location) const;
	static FVector GetSquadCentre(ASquadController* Squad);
	void ViewFrom(const FVector& Focus, float Distance, float Pitch, float Yaw = 135.f) const;
	void ViewCloseUp(ASquadController* Squad, float Yaw) const;
	void RequestScreenshot(const FString& Label);
};
