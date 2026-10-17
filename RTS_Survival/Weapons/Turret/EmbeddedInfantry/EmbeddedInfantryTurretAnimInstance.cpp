// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "EmbeddedInfantryTurretAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

void UEmbeddedInfantryTurretAnimInstance::PlayFireMontage(const EWeaponFireMode FireMode)
{
	const bool bIsBurst = FireMode != EWeaponFireMode::Single;
	UAnimMontage* const MontageToPlay = bIsBurst && IsValid(BurstFireMontage) ? BurstFireMontage : SingleFireMontage;
	if (not IsValid(MontageToPlay))
	{
		return;
	}
	Montage_Play(MontageToPlay);
}

void UEmbeddedInfantryTurretAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	Pitch = FMath::FInterpTo(Pitch, M_TargetAimPitch, DeltaSeconds, PitchInterpSpeed);
}
