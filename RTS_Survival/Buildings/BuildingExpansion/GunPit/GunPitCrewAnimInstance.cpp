// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitCrewAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"

namespace GunPitCrewFlinch
{
	constexpr float LowerSpineHunchDegrees = 7.f;
	constexpr float UpperSpineHunchDegrees = 8.f;
	constexpr float HeadDuckDegrees = 14.f;
	constexpr float HeadTurnDegrees = 16.f;
	// Snap into the brace, hold it through the blast, then straighten up.
	constexpr float RiseSeconds = 0.07f;
	constexpr float HoldEndSeconds = 0.25f;
	constexpr float EndSeconds = 0.95f;
	constexpr float MaxDelaySeconds = 0.06f;
	constexpr float StrengthJitter = 0.2f;
	// The infantry skeleton faces +Y in component space.
	const FVector MeshForward = FVector::RightVector;
}

void UGunPitCrewAnimInstance::PlayFireFlinch(const float Strength)
{
	const float SecondsIntoFlinch = M_FlinchState.SecondsSinceShot - M_FlinchState.DelaySeconds;
	if (M_FlinchState.SecondsSinceShot >= 0.f && SecondsIntoFlinch > GunPitCrewFlinch::RiseSeconds)
	{
		// A shot while braced (an autocannon burst) keeps the brace: back to the start of the hold, no snap.
		M_FlinchState.SecondsSinceShot = M_FlinchState.DelaySeconds + GunPitCrewFlinch::RiseSeconds;
		return;
	}
	if (M_FlinchState.SecondsSinceShot >= 0.f)
	{
		// Still rising into the brace: let it finish.
		return;
	}
	M_FlinchState.SecondsSinceShot = 0.f;
	M_FlinchState.DelaySeconds = FMath::FRandRange(0.f, GunPitCrewFlinch::MaxDelaySeconds);
	M_FlinchState.Strength = Strength * FMath::FRandRange(1.f - GunPitCrewFlinch::StrengthJitter,
	                                                      1.f + GunPitCrewFlinch::StrengthJitter);
	M_FlinchState.HeadTurnSign = FMath::RandBool() ? 1.f : -1.f;
}

void UGunPitCrewAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float Weight = UpdateFlinchWeight(DeltaSeconds);
	if (Weight <= 0.f)
	{
		ClearFlinch();
		return;
	}
	const FVector Facing = GetFacingDirection();
	// Turning about Up x Facing tips the upper body forward over the feet.
	const FVector HunchAxis = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
	const float Amount = Weight * M_FlinchState.Strength;
	FlinchLowerSpineRotation = FQuat(HunchAxis, FMath::DegreesToRadians(GunPitCrewFlinch::LowerSpineHunchDegrees * Amount)).Rotator();
	FlinchUpperSpineRotation = FQuat(HunchAxis, FMath::DegreesToRadians(GunPitCrewFlinch::UpperSpineHunchDegrees * Amount)).Rotator();
	const FQuat Duck(HunchAxis, FMath::DegreesToRadians(GunPitCrewFlinch::HeadDuckDegrees * Amount));
	const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(GunPitCrewFlinch::HeadTurnDegrees * Amount * M_FlinchState.HeadTurnSign));
	FlinchHeadRotation = (Turn * Duck).Rotator();
}

float UGunPitCrewAnimInstance::UpdateFlinchWeight(const float DeltaSeconds)
{
	if (M_FlinchState.SecondsSinceShot < 0.f)
	{
		return 0.f;
	}
	M_FlinchState.SecondsSinceShot += DeltaSeconds;
	const float Seconds = M_FlinchState.SecondsSinceShot - M_FlinchState.DelaySeconds;
	if (Seconds >= GunPitCrewFlinch::EndSeconds)
	{
		M_FlinchState.SecondsSinceShot = -1.f;
		return 0.f;
	}
	if (Seconds <= 0.f)
	{
		return 0.f;
	}
	if (Seconds < GunPitCrewFlinch::RiseSeconds)
	{
		return FMath::SmoothStep(0.f, GunPitCrewFlinch::RiseSeconds, Seconds);
	}
	if (Seconds < GunPitCrewFlinch::HoldEndSeconds)
	{
		return 1.f;
	}
	return 1.f - FMath::SmoothStep(GunPitCrewFlinch::HoldEndSeconds, GunPitCrewFlinch::EndSeconds, Seconds);
}

FVector UGunPitCrewAnimInstance::GetFacingDirection() const
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent();
	if (not IsValid(SkeletalMeshComponent) || SkeletalMeshComponent->GetNumBones() == 0)
	{
		return GunPitCrewFlinch::MeshForward;
	}
	const FName RootBone = SkeletalMeshComponent->GetBoneName(0);
	const FQuat RootNow = SkeletalMeshComponent->GetBoneQuaternion(RootBone, EBoneSpaces::ComponentSpace);
	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FQuat RootReference = ReferenceSkeleton.GetRefBonePose()[0].GetRotation();
	FVector Facing = (RootNow * RootReference.Inverse()).RotateVector(GunPitCrewFlinch::MeshForward);
	Facing.Z = 0.f;
	return Facing.GetSafeNormal(UE_SMALL_NUMBER, GunPitCrewFlinch::MeshForward);
}

void UGunPitCrewAnimInstance::ClearFlinch()
{
	FlinchLowerSpineRotation = FRotator::ZeroRotator;
	FlinchUpperSpineRotation = FRotator::ZeroRotator;
	FlinchHeadRotation = FRotator::ZeroRotator;
}
