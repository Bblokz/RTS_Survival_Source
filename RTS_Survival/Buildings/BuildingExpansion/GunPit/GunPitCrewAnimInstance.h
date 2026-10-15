// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GunPitCrewAnimInstance.generated.h"

class UAnimSequenceBase;

/**
 * @brief Anim instance of one gun pit crew member: loops IdleAnimation and plays the reload montage in its
 * DefaultSlot on top, so the crew blends smoothly into and out of the loading drill.
 * The anim blueprint plays IdleAnimation (Property Access) into a DefaultSlot node.
 */
UCLASS()
class RTS_SURVIVAL_API UGunPitCrewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetIdleAnimation(UAnimSequenceBase* NewIdleAnimation) { IdleAnimation = NewIdleAnimation; }

protected:
	// Set by UGunPitCrewComponent; each crew member idles at its own spot in the pit.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit Crew")
	TObjectPtr<UAnimSequenceBase> IdleAnimation = nullptr;
};
