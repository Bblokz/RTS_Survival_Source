// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"
#include "EmbeddedInfantryTurretAnimInstance.generated.h"

class UAnimMontage;
enum class EWeaponFireMode : uint8;

/**
 * @brief Anim instance of an embedded infantry turret: the soldier aims his weapon with the infantry rifle aim offset
 * (Pitch drives its TargetPitch axis) and plays the infantry fire montages; the turret actor turns the whole soldier.
 * @note Anim graph: base aim sequence -> Slot 'FIreSlot' -> AimOffset Player (X = Yaw, Y = Pitch) -> Output Pose.
 */
UCLASS()
class RTS_SURVIVAL_API UEmbeddedInfantryTurretAnimInstance : public UTurretAnimInstance
{
	GENERATED_BODY()

public:
	// The pitch the soldier eases towards; the turret sets it whenever it aims at a new target.
	void SetTargetAimPitch(const float NewTargetPitch) { M_TargetAimPitch = NewTargetPitch; }
	float GetTargetAimPitch() const { return M_TargetAimPitch; }

	void PlayFireMontage(const EWeaponFireMode FireMode);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<UAnimMontage> SingleFireMontage = nullptr;

	// Played for burst fire modes; falls back to SingleFireMontage when not set.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<UAnimMontage> BurstFireMontage = nullptr;

	// How fast Pitch follows the target pitch; 0 snaps.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0"))
	float PitchInterpSpeed = 8.f;

private:
	float M_TargetAimPitch = 0.f;
};
