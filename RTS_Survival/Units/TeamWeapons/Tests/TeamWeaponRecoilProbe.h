// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ATeamWeapon;
class UAnimMeshFeedbackComponent;
class USkeletalMeshComponent;

/** Peak recoil of one kick, in the frame of the barrel at the moment of firing (world space, horizontal). */
struct FTWRecoilKick
{
	float StartSeconds = 0.f;
	// Mesh offset from rest: back along the barrel, lateral to the barrel (+ right), up.
	float PeakBackCm = 0.f;
	float PeakLateralCm = 0.f;
	float PeakUpCm = 0.f;
	// Mesh rotation from rest about the barrel's lateral axis (nose up +), the up axis and the barrel axis itself.
	float PeakPitchDeg = 0.f;
	float PeakYawDeg = 0.f;
	float PeakRollDeg = 0.f;
	// Movement of the muzzle (fire socket) from rest; what the player actually sees swing.
	float PeakMuzzleLateralCm = 0.f;
	float PeakMuzzleBackCm = 0.f;
	float PeakMuzzleUpCm = 0.f;
};

/**
 * @brief Measures the fire feedback of one team weapon without rendering: per shot, how far and which way the
 * feedback component moves the gun mesh compared to the barrel direction, so sideways kicks show up as numbers.
 * Owned per subject by the crew anim audit; writes RTS_TWQC recoil_setup / recoil_kick lines.
 */
class FTeamWeaponRecoilProbe
{
public:
	/**
	 * @brief Samples the gun mesh once per frame; a kick starts when the feedback component starts ticking.
	 * @param SubjectName Test subject name for the log lines.
	 * @param Weapon The subject's gun.
	 * @param GameSeconds Current game time.
	 */
	void Tick(const FString& SubjectName, ATeamWeapon& Weapon, const float GameSeconds);

private:
	bool bM_LoggedSetup = false;
	bool bM_WasKicking = false;
	int32 M_KickCount = 0;
	// Mesh and muzzle in world space on the frame before a kick, the rest pose every kick is measured against.
	// World, not actor space: the feedback mesh is usually the actor root, so the kick moves the actor too.
	FTransform M_LastMeshWorld = FTransform::Identity;
	FVector M_LastMuzzleWorld = FVector::ZeroVector;
	FTransform M_RestMeshWorld = FTransform::Identity;
	FVector M_RestMuzzleWorld = FVector::ZeroVector;
	FVector M_BarrelForward = FVector::ForwardVector;
	FTWRecoilKick M_Kick;

	void LogSetup(const FString& SubjectName, const ATeamWeapon& Weapon,
	              const UAnimMeshFeedbackComponent& Feedback, const USkeletalMeshComponent& Mesh) const;
	void StartKick(const ATeamWeapon& Weapon, const USkeletalMeshComponent& Mesh, const float GameSeconds);
	void SampleKick(const FTransform& MeshWorld, const FVector& MuzzleWorld);
	void LogKick(const FString& SubjectName, const ATeamWeapon& Weapon, const float GameSeconds) const;
	static FName GetFireSocket(const ATeamWeapon& Weapon);
};
