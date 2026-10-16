// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TeamWeaponCrewFormation.generated.h"

class ASquadUnit;
class ATeamWeapon;
class UCrewPosition;

/** Tuning of how the crew stays with a gun that is being pushed. */
USTRUCT(BlueprintType)
struct FTeamWeaponPushedCrewSettings
{
	GENERATED_BODY()

	// Crew may walk this much faster than the gun so they can catch up with their post.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "1.0"))
	float CatchUpSpeedMultiplier = 1.6f;

	// How hard a soldier is pulled back to his post, per second of the distance he is off it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "0.1"))
	float PostPullStrength = 2.5f;

	// Operators closer than this to their post count as in place; the gun moves at full speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "1.0"))
	float PostToleranceCm = 60.f;

	// The gun stops and waits once its slowest operator is this far from his post.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "1.0"))
	float WaitForCrewDistanceCm = 220.f;

	// The gun creeps on at this fraction of its speed after waiting MaxWaitForCrewSeconds, so a stuck soldier
	// never parks it for good.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "0.0"))
	float MaxWaitForCrewSeconds = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CreepSpeedFractionAfterWaiting = 0.5f;

	// A larger heading error turns the gun on the spot before the crew pushes it on.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float MaxTurnWhileMovingDegrees = 70.f;

	// A soldier this far off his post for StuckSeconds is put back on it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "100.0"))
	float StuckDistanceCm = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|PushedCrew", meta = (ClampMin = "0.5"))
	float StuckSeconds = 5.f;
};

/** A soldier that walks with the pushed gun and the post he keeps. */
USTRUCT()
struct FTeamWeaponFormationMember
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_Unit;

	// Operators keep their crew position; it moves with the gun.
	UPROPERTY()
	TWeakObjectPtr<UCrewPosition> M_CrewPosition;

	// Post in the gun's heading space for members without a crew position (guards).
	UPROPERTY()
	FVector M_LocalPostOffset = FVector::ZeroVector;

	// Only operators hold the gun back; guards just follow.
	UPROPERTY()
	bool bM_IsOperator = false;

	float M_SecondsFarFromPost = 0.f;
};

/**
 * @brief Keeps operators on their crew positions and guards behind the gun while the gun is pushed along its path.
 * The controller starts it when a pushed move begins and ticks it every frame; the speed scale it returns makes
 * the gun wait for its crew instead of rolling away from them.
 */
USTRUCT()
struct FTeamWeaponPushedCrewFormation
{
	GENERATED_BODY()

	void Start(ATeamWeapon* Weapon, const TArray<FTeamWeaponFormationMember>& Members,
	           const FTeamWeaponPushedCrewSettings& Settings);
	void Stop();
	bool GetIsActive() const { return bM_IsActive; }

	/**
	 * @brief Steers every member towards his post around the gun.
	 * @param DeltaSeconds Frame time.
	 * @return Fraction of its push speed the gun may use this frame; 0 means wait for the crew.
	 */
	float Tick(const float DeltaSeconds);

private:
	bool TryGetPostLocation(const FTeamWeaponFormationMember& Member, const FTransform& WeaponHeading,
	                           FVector& OutPostLocation) const;

	/**
	 * @brief Requests the walk that brings one soldier to his post while matching the gun's own speed.
	 * @param Member Soldier and post.
	 * @param WeaponHeading Yaw-only transform of the gun.
	 * @param DeltaSeconds Frame time.
	 * @return Distance of the soldier to his post; negative when he is not steered this frame.
	 */
	float SteerMember(FTeamWeaponFormationMember& Member, const FTransform& WeaponHeading, const float DeltaSeconds) const;
	void PutMemberOnPost(ASquadUnit& Unit, const FVector& PostLocation) const;
	float GetSpeedScaleForCrewLag(const float WorstOperatorDistanceCm, const float DeltaSeconds);

	UPROPERTY()
	TArray<FTeamWeaponFormationMember> M_Members;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeapon> M_Weapon;

	FTeamWeaponPushedCrewSettings M_Settings;
	FVector M_LastWeaponLocation = FVector::ZeroVector;
	FVector M_WeaponVelocity = FVector::ZeroVector;
	bool bM_HasLastWeaponLocation = false;
	float M_SecondsWaitingForCrew = 0.f;
	bool bM_IsActive = false;
};
