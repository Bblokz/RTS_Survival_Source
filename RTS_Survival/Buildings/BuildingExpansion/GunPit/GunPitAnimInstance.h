// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"
#include "GunPitAnimInstance.generated.h"

/**
 * @brief How a pit gun bucks on a shot, on top of its recoil montage: a damped kick that shoves the carriage back,
 * rocks it about its spades and throws the muzzle up. Tuned per gun in its anim blueprint's class defaults; zero
 * amplitudes leave the gun still.
 */
USTRUCT(BlueprintType)
struct FGunPitFireKickSettings
{
	GENERATED_BODY()

	// Bone the anim blueprint's Fire Kick Modify Bone node moves: a child of the root bone that carries the whole gun
	// but not the crew socket, so the crew stay put while the gun bucks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick")
	FName KickBone = NAME_None;

	// The gun frame socket (+X the muzzle, on the ground); the kick's direction and rock pivot are in its frame.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick")
	FName GunFrameSocket = TEXT("PitOrigin");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0"))
	float JoltCentimetres = 0.f;

	// Muzzle-up rock of the whole carriage about RockPivotBackCentimetres behind the gun frame origin.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0"))
	float RockDegrees = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0"))
	float RockPivotBackCentimetres = 250.f;

	// Added to the aim pitch on the elevation bone.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0"))
	float MuzzleJumpDegrees = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0.5", ClampMax = "20"))
	float FrequencyHz = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Kick", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float DampingRatio = 0.35f;
};

/** Runtime state of the fire kick; the gun frame is read once from the mesh's reference pose. */
struct FGunPitFireKickState
{
	// Seconds since each shot whose kick still plays; an autocannon's kicks overlap and add up like a real spring.
	TArray<float> ShotAges;
	bool bHasGunFrame = false;
	FVector BackDirection = FVector::BackwardVector;
	FVector RockAxis = FVector::RightVector;
	// Kick bone's reference location relative to the rock pivot (component space).
	FVector KickBoneFromPivot = FVector::ZeroVector;
};

/**
 * @brief Anim instance of a gun standing in a round pit: the whole gun turns with the turret yaw (a Modify Bone
 * node on the gun's root bone reads TurntableYawRotation) and the barrel elevates with the pitch (a second Modify
 * Bone on the elevation bone reads CradlePitchRotation), so the gun and the crew riding on its root bone traverse
 * a full circle inside the pit. An optional curve in a reload montage takes the elevation away from the aim.
 * A third Modify Bone on FireKick.KickBone (before the yaw node) reads FireKickRotation / FireKickTranslation;
 * UGunPitCrewComponent starts the kick on every shot.
 */
UCLASS()
class RTS_SURVIVAL_API UGunPitAnimInstance : public UTurretAnimInstance
{
	GENERATED_BODY()

public:
	void PlayFireKick();

	FName GetFireKickBone() const { return FireKick.KickBone; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Curve in a reload montage: 1 while the crew loads at a fixed loading elevation. None: always follow the aim.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FName LoadingElevationCurveName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FGunPitFireKickSettings FireKick;

	// 1 follows the turret pitch, 0 leaves the elevation to the reload montage. Alpha of the pitch Modify Bone.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	float AimPitchAlpha = 1.f;

	// Component-space yaw of the gun's root bone.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FRotator TurntableYawRotation = FRotator::ZeroRotator;

	// Component-space elevation of the elevation bone; positive pitch raises the muzzle.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FRotator CradlePitchRotation = FRotator::ZeroRotator;

	// Additive component-space rotation and translation of FireKick.KickBone while the gun bucks.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FRotator FireKickRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FVector FireKickTranslation = FVector::ZeroVector;

private:
	FGunPitFireKickState M_FireKickState;

	/** @return The kick of all shots still playing, after ageing them; one shot's first swing peaks at 1. */
	float UpdateFireKickResponse(const float DeltaSeconds);

	// LagSeconds: how much later than the carriage this response follows (the muzzle jump trails it).
	float GetSummedKickResponse(const float LagSeconds) const;

	float GetKickResponseAt(const float Seconds) const;
	bool CacheGunFrame();
	void UpdateFireKickPose(const float Response);
};
