// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponCrewFormation.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/CrewPositions/CrewPosition.h"

namespace TeamWeaponCrewFormationStatics
{
	// Below this requested speed the soldier is left to brake instead of shuffling on the spot.
	constexpr float MinRequestedSpeedCmPerSec = 12.f;
	// A soldier this close to his post while the gun stands still is in place.
	constexpr float StandStillPostFraction = 0.5f;
	constexpr float GunStandingStillSpeedCmPerSec = 15.f;
	constexpr float WaitingSpeedScale = 0.05f;
	constexpr float PostTeleportClearanceCm = 2.f;

	FVector GetPlanar(const FVector& Vector)
	{
		return FVector(Vector.X, Vector.Y, 0.f);
	}
}

void FTeamWeaponPushedCrewFormation::Start(ATeamWeapon* Weapon, const TArray<FTeamWeaponFormationMember>& Members,
                                           const FTeamWeaponPushedCrewSettings& Settings)
{
	M_Weapon = Weapon;
	M_Members = Members;
	M_Settings = Settings;
	bM_HasLastWeaponLocation = false;
	M_WeaponVelocity = FVector::ZeroVector;
	M_SecondsWaitingForCrew = 0.f;
	bM_IsActive = IsValid(Weapon) && Members.Num() > 0;
}

void FTeamWeaponPushedCrewFormation::Stop()
{
	bM_IsActive = false;
	M_Members.Reset();
	M_Weapon.Reset();
	bM_HasLastWeaponLocation = false;
	M_SecondsWaitingForCrew = 0.f;
}

float FTeamWeaponPushedCrewFormation::Tick(const float DeltaSeconds)
{
	if (not bM_IsActive || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	const ATeamWeapon* Weapon = M_Weapon.Get();
	if (not IsValid(Weapon))
	{
		Stop();
		return 1.f;
	}
	const FVector WeaponLocation = Weapon->GetActorLocation();
	if (bM_HasLastWeaponLocation)
	{
		M_WeaponVelocity = TeamWeaponCrewFormationStatics::GetPlanar(WeaponLocation - M_LastWeaponLocation) / DeltaSeconds;
	}
	M_LastWeaponLocation = WeaponLocation;
	bM_HasLastWeaponLocation = true;

	// Posts follow the gun's heading only; its ground tilt must not swing the guards up or down a slope.
	const FTransform WeaponHeading(FRotator(0.f, Weapon->GetActorRotation().Yaw, 0.f), WeaponLocation);
	float WorstOperatorDistanceCm = 0.f;
	for (FTeamWeaponFormationMember& Member : M_Members)
	{
		const float DistanceToPostCm = SteerMember(Member, WeaponHeading, DeltaSeconds);
		if (Member.bM_IsOperator)
		{
			WorstOperatorDistanceCm = FMath::Max(WorstOperatorDistanceCm, DistanceToPostCm);
		}
	}
	return GetSpeedScaleForCrewLag(WorstOperatorDistanceCm, DeltaSeconds);
}

bool FTeamWeaponPushedCrewFormation::TryGetPostLocation(const FTeamWeaponFormationMember& Member,
                                                        const FTransform& WeaponHeading,
                                                        FVector& OutPostLocation) const
{
	if (const UCrewPosition* CrewPosition = Member.M_CrewPosition.Get())
	{
		OutPostLocation = CrewPosition->GetComponentLocation();
		return true;
	}
	if (Member.bM_IsOperator && Member.M_CrewPosition.IsStale())
	{
		return false;
	}
	OutPostLocation = WeaponHeading.TransformPosition(Member.M_LocalPostOffset);
	return true;
}

float FTeamWeaponPushedCrewFormation::SteerMember(FTeamWeaponFormationMember& Member, const FTransform& WeaponHeading,
                                                  const float DeltaSeconds) const
{
	using namespace TeamWeaponCrewFormationStatics;
	ASquadUnit* Unit = Member.M_Unit.Get();
	if (not IsValid(Unit) || not Unit->IsUnitAlive())
	{
		return 0.f;
	}
	FVector PostLocation;
	if (not TryGetPostLocation(Member, WeaponHeading, PostLocation))
	{
		return 0.f;
	}
	// Another order walks this soldier (evasion, cargo, a player move on him); pick him up again once it ends.
	if (Unit->GetIsPathFollowingActive())
	{
		return 0.f;
	}
	UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
	if (not IsValid(Movement))
	{
		return 0.f;
	}
	const FVector ToPost = GetPlanar(PostLocation - Unit->GetActorLocation());
	const float DistanceToPostCm = ToPost.Size();
	if (DistanceToPostCm > M_Settings.StuckDistanceCm)
	{
		Member.M_SecondsFarFromPost += DeltaSeconds;
		if (Member.M_SecondsFarFromPost >= M_Settings.StuckSeconds)
		{
			Member.M_SecondsFarFromPost = 0.f;
			PutMemberOnPost(*Unit, PostLocation);
			return 0.f;
		}
	}
	else
	{
		Member.M_SecondsFarFromPost = 0.f;
	}

	const bool bIsGunStandingStill = M_WeaponVelocity.Size() < GunStandingStillSpeedCmPerSec;
	if (bIsGunStandingStill && DistanceToPostCm < M_Settings.PostToleranceCm * StandStillPostFraction)
	{
		return DistanceToPostCm;
	}
	// Walk with the gun and close the remaining gap; the speed limit is the unit's catch-up walk speed.
	FVector DesiredVelocity = M_WeaponVelocity + ToPost * M_Settings.PostPullStrength;
	DesiredVelocity = DesiredVelocity.GetClampedToMaxSize(Movement->GetMaxSpeed());
	if (DesiredVelocity.Size() >= MinRequestedSpeedCmPerSec)
	{
		Movement->RequestDirectMove(DesiredVelocity, false);
	}
	return DistanceToPostCm;
}

void FTeamWeaponPushedCrewFormation::PutMemberOnPost(ASquadUnit& Unit, const FVector& PostLocation) const
{
	using namespace TeamWeaponCrewFormationStatics;
	const ATeamWeapon* Weapon = M_Weapon.Get();
	const UCapsuleComponent* Capsule = Unit.GetCapsuleComponent();
	float GroundZ = 0.f;
	if (not IsValid(Weapon) || not IsValid(Capsule) || not Weapon->TryGetGroundHeightAt(PostLocation, GroundZ))
	{
		return;
	}
	const FVector TeleportLocation(PostLocation.X, PostLocation.Y,
	                               GroundZ + Capsule->GetScaledCapsuleHalfHeight() + PostTeleportClearanceCm);
	Unit.SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

float FTeamWeaponPushedCrewFormation::GetSpeedScaleForCrewLag(const float WorstOperatorDistanceCm,
                                                              const float DeltaSeconds)
{
	using namespace TeamWeaponCrewFormationStatics;
	const float LagRangeCm = FMath::Max(M_Settings.WaitForCrewDistanceCm - M_Settings.PostToleranceCm, 1.f);
	const float LagBeyondToleranceCm = FMath::Max(WorstOperatorDistanceCm - M_Settings.PostToleranceCm, 0.f);
	float SpeedScale = 1.f - FMath::Clamp(LagBeyondToleranceCm / LagRangeCm, 0.f, 1.f);
	if (SpeedScale <= WaitingSpeedScale)
	{
		M_SecondsWaitingForCrew += DeltaSeconds;
	}
	else
	{
		M_SecondsWaitingForCrew = 0.f;
	}
	if (M_SecondsWaitingForCrew > M_Settings.MaxWaitForCrewSeconds)
	{
		SpeedScale = FMath::Max(SpeedScale, M_Settings.CreepSpeedFractionAfterWaiting);
	}
	return SpeedScale;
}
