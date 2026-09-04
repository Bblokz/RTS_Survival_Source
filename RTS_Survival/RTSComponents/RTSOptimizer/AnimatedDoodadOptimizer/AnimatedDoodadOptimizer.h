// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"

#include "AnimatedDoodadOptimizer.generated.h"

/**
 * @brief Added to animated doodads so their skeletal animations run only while directly in camera FOV.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAnimatedDoodadOptimizer : public URTSOptimizer
{
	GENERATED_BODY()

protected:
	virtual void OutFovCloseUpdateComponents() override;
	virtual void OutFovFarUpdateComponents() override;
	virtual bool ShouldAlwaysConsiderOwnerInFOVAtCloseRange() const override;
};
