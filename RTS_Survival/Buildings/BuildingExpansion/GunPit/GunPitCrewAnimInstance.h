// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GunPitCrewAnimInstance.generated.h"

class UAnimSequenceBase;

/** Runtime state of one crew member's flinch at a shot. */
struct FGunPitCrewFlinchState
{
	// Negative while no flinch plays.
	float SecondsSinceShot = -1.f;
	// Each man reacts a little later and harder or softer than the next.
	float DelaySeconds = 0.f;
	float Strength = 1.f;
	// The head turns away to the left (+1) or the right (-1).
	float HeadTurnSign = 1.f;
};

/**
 * @brief Anim instance of one gun pit crew member: loops IdleAnimation and plays the reload montage in its
 * DefaultSlot on top, so the crew blends smoothly into and out of the loading drill.
 * The anim blueprint plays IdleAnimation (Property Access) into a DefaultSlot node, then three additive
 * component-space Modify Bone nodes (spine_01, spine_03, head) read the Flinch rotations, so the crew brace at
 * every shot whatever they are doing (UGunPitCrewComponent starts the flinch).
 */
UCLASS()
class RTS_SURVIVAL_API UGunPitCrewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetIdleAnimation(UAnimSequenceBase* NewIdleAnimation) { IdleAnimation = NewIdleAnimation; }

	// Hunch, duck and turn the head away from the blast; Strength scales the whole reaction.
	void PlayFireFlinch(const float Strength);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Set by UGunPitCrewComponent; each crew member idles at its own spot in the pit.
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit Crew")
	TObjectPtr<UAnimSequenceBase> IdleAnimation = nullptr;

	// Additive component-space rotations of the flinch (identity when no shot is fired).
	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit Crew")
	FRotator FlinchLowerSpineRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit Crew")
	FRotator FlinchUpperSpineRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Gun Pit Crew")
	FRotator FlinchHeadRotation = FRotator::ZeroRotator;

private:
	FGunPitCrewFlinchState M_FlinchState;

	/** @return The flinch weight now: 0 at rest, 1 at the full brace. */
	float UpdateFlinchWeight(const float DeltaSeconds);

	// The direction the man faces in component space, from his root bone in the last pose (the shell handler walks).
	FVector GetFacingDirection() const;
	void ClearFlinch();
};
