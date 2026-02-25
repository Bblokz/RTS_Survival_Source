// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TurretAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTurretAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	float GetCurrentYawAngle() const { return Yaw; }
	float GetCurrentPitchAngle() const { return Pitch; }
	inline void SetYaw(const float NewYaw) {Yaw = NewYaw; };
	inline void SetPitch(const float NewPitch) {Pitch = NewPitch; };

protected:
	UPROPERTY(BlueprintReadWrite, Category = "TurretAnimInstance")
	float Yaw;

	UPROPERTY(BlueprintReadWrite, Category = "TurretAnimInstance")
	float Pitch;
};
