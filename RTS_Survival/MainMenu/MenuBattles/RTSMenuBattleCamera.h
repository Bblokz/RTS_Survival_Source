// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RTSMenuBattleTypes.h"

#include "RTSMenuBattleCamera.generated.h"

class ARTSMenuBattleArena;

/** The shot the camera is holding and what it needs to move along it. */
USTRUCT()
struct FRTSMenuBattleShotState
{
	GENERATED_BODY()

	ERTSMenuBattleShot Shot = ERTSMenuBattleShot::HighOverview;
	float StartTimeSeconds = 0.f;
	float DurationSeconds = 0.f;
	// +1 or -1: which flank or which way round the shot moves.
	float SideSign = 1.f;
	float OrbitStartYaw = 0.f;
	ERTSMenuBattleSide LineSide = ERTSMenuBattleSide::German;

	UPROPERTY()
	TWeakObjectPtr<AActor> ChaseTarget;

	FVector ChaseAnchor = FVector::ZeroVector;
	// The first update of a shot is a cut: no smoothing from the previous shot's transform.
	bool bIsCut = true;
};

/**
 * @brief Spawned by the menu battle director: films the running battle as a sequence of cut shots (orbit, flank
 * dolly, behind a line, chasing a vehicle, high overview) around the fighting units' smoothed centre.
 */
UCLASS(NotPlaceable)
class RTS_SURVIVAL_API ARTSMenuBattleCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ARTSMenuBattleCamera();

	/**
	 * @brief Starts filming a new battle; the first shot is an establishing shot placed at once.
	 * @param Arena Arena of the battle.
	 * @param Seed Seed of this battle's shot choices.
	 * @param MapCentre Shots stand on the arena's outer side and look toward this point, so they film the map and
	 * not the void past the landscape's edge.
	 */
	void BeginCoverage(ARTSMenuBattleArena& Arena, const int32 Seed, const FVector& MapCentre);

	/** The units the shots frame, per side; called whenever the battle's lines change. */
	void SetSubjects(const TArray<TWeakObjectPtr<AActor>>& GermanUnits, const TArray<TWeakObjectPtr<AActor>>& SovietUnits);

	void StopCoverage();

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TWeakObjectPtr<ARTSMenuBattleArena> M_Arena;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_GermanSubjects;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SovietSubjects;

	UPROPERTY()
	FRTSMenuBattleShotState M_ShotState;

	FRandomStream M_ShotStream;

	// Centre of the fighting, eased toward the alive units so cuts and deaths do not jerk the framing.
	FVector M_SmoothedFocus = FVector::ZeroVector;

	// Radius of the fighting around its centre, eased like the centre; drives how far the camera pulls back.
	float M_SmoothedSpread = 0.f;

	// Unit vector from the arena toward the map centre; zero when the arena sits on the centre.
	FVector M_InwardDirection = FVector::ZeroVector;

	bool bM_IsCovering = false;

	bool GetIsValidArena() const;

	float GetTimeSeconds() const;

	void StartShot(const ERTSMenuBattleShot Shot);

	/**
	 * @brief Picks +1 or -1 so that Axis * Sign points away from the map centre, randomly when Axis is about square to
	 * the inward direction.
	 * @param Axis Horizontal direction the sign is applied to.
	 * @return The sign that puts the camera on the arena's outer side.
	 */
	float PickOutwardSign(const FVector& Axis) const;
	ERTSMenuBattleShot PickNextShot() const;
	bool GetHasAliveSubject() const;
	AActor* PickChaseTarget();

	/** Centre of the alive subjects of one side, or of both sides when OnlySide is null. */
	bool GetSubjectsCentre(const TArray<TWeakObjectPtr<AActor>>* OnlySide, FVector& OutCentre) const;
	FVector GetFocusTarget() const;

	void UpdateShot(const float DeltaSeconds);

	/**
	 * @brief The camera location and look-at point of the current shot at its local time.
	 * @param ShotSeconds Seconds since the shot started.
	 * @param DeltaSeconds Frame time, for the chase anchor's easing.
	 * @param OutLocation Camera location.
	 * @param OutLookAt Point the camera aims at.
	 * @return Field of view of the shot in degrees.
	 */
	float ComputeShot(const float ShotSeconds, const float DeltaSeconds, FVector& OutLocation, FVector& OutLookAt);

	float ComputeOrbit(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const;
	float ComputeFlankDolly(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const;
	float ComputeBehindLine(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const;
	float ComputeChase(const float DeltaSeconds, FVector& OutLocation, FVector& OutLookAt);
	float ComputeColumnTrack(const float DeltaSeconds, FVector& OutLocation, FVector& OutLookAt);
	float ComputeHighOverview(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const;

	// Largest 2D distance of a living subject from the centre.
	float GetSubjectsSpread(const FVector& Centre) const;
	float GetFramingDistance() const;
	float GetFramingHeight() const;

	// Lifts a camera location that would sit in or just above the ground.
	FVector KeepAboveGround(const FVector& Location) const;
};
