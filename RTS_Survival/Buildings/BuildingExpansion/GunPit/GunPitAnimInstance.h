// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"
#include "GunPitAnimInstance.generated.h"

/**
 * @brief Anim instance of a gun standing in a round pit: the whole gun turns with the turret yaw (a Modify Bone
 * node on the gun's root bone reads TurntableYawRotation) and the barrel elevates with the pitch (a second Modify
 * Bone on the elevation bone reads CradlePitchRotation), so the gun and the crew riding on its root bone traverse
 * a full circle inside the pit. An optional curve in a reload montage takes the elevation away from the aim.
 */
UCLASS()
class RTS_SURVIVAL_API UGunPitAnimInstance : public UTurretAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Curve in a reload montage: 1 while the crew loads at a fixed loading elevation. None: always follow the aim.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FName LoadingElevationCurveName = NAME_None;

	// 1 follows the turret pitch, 0 leaves the elevation to the reload montage. Alpha of the pitch Modify Bone.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	float AimPitchAlpha = 1.f;

	// Component-space yaw of the gun's root bone.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FRotator TurntableYawRotation = FRotator::ZeroRotator;

	// Component-space elevation of the elevation bone; positive pitch raises the muzzle.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit")
	FRotator CradlePitchRotation = FRotator::ZeroRotator;
};
