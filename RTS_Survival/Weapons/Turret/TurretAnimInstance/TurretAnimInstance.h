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

protected:
	UPROPERTY(BlueprintReadWrite, Category = "TurretAnimInstance")
	float Yaw;

	UPROPERTY(BlueprintReadWrite, Category = "TurretAnimInstance")
	float Pitch;
};
