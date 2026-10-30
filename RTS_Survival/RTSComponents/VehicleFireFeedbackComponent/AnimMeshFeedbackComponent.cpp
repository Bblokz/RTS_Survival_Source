#include "AnimMeshFeedbackComponent.h"
#include "Math/UnrealMathUtility.h" // For FMath::IsFinite


#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

bool IsFiniteVector(const FVector& Vector)
{
    return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
}

bool IsFiniteRotator(const FRotator& Rotator)
{
    return FMath::IsFinite(Rotator.Pitch) && FMath::IsFinite(Rotator.Yaw) && FMath::IsFinite(Rotator.Roll);
}

void UAnimMeshFeedbackComponent::InitializeAnimMeshFeedbackComponent(
	USkeletalMeshComponent* InAnimatedMesh,
	ACPPTurretsMaster* InTurretMaster)
{
	M_AnimatedMesh = InAnimatedMesh;

	Super::InitializeFeedbackComponent(
		InAnimatedMesh,
		nullptr,
		InTurretMaster);
}

bool UAnimMeshFeedbackComponent::GetIsValidHullMesh() const
{
	return GetIsValidAnimatedMesh();
}

void UAnimMeshFeedbackComponent::CacheHullBaseTransform()
{
	if (not GetIsValidAnimatedMesh())
	{
		return;
	}

	M_LastAppliedRecoilLocationOffsetCm = FVector::ZeroVector;
	M_LastAppliedRecoilPitchDeg = 0.0f;
	M_LastAppliedRecoilRollDeg = 0.0f;
}

void UAnimMeshFeedbackComponent::ApplyFeedbackKick(
	const float NormalisedMuzzleEnergy,
	const float TurretWorldYawDegrees)
{
	(void)TurretWorldYawDegrees;
	if (not GetIsValidAnimatedMesh())
	{
		return;
	}

	const float ClampedEnergy = FMath::Clamp(NormalisedMuzzleEnergy, 0.0f, 1.0f);
	const FRotator RecoilFrameYawInParentSpace = GetRecoilFrameYawInParentSpace();
	const FVector BackwardAxis = RecoilFrameYawInParentSpace.RotateVector(
		GetSignedAxisVector(M_AnimMeshRecoilAxesSettings.M_BackwardRecoilAxis));
	const FVector BackwardRecoilOffset =
		BackwardAxis * (M_FeedbackSettings.M_MaxBackCm * ClampedEnergy);
	M_RecoilOffsetCm += BackwardRecoilOffset;

	if (M_AnimMeshRecoilAxesSettings.bM_EnableSideSway)
	{
		const FVector SideSwayAxis = RecoilFrameYawInParentSpace.RotateVector(
			GetSignedAxisVector(M_AnimMeshRecoilAxesSettings.M_SideSwayAxis));
		const float SideSwayJitterCm = FMath::FRandRange(
			-M_AnimMeshRecoilAxesSettings.M_MaxSideSwayCm,
			M_AnimMeshRecoilAxesSettings.M_MaxSideSwayCm) * ClampedEnergy;
		M_RecoilOffsetCm += SideSwayAxis * SideSwayJitterCm;
	}

	AddNoseUpKickAlongBarrel(M_FeedbackSettings.M_MaxPitchDeg * ClampedEnergy, RecoilFrameYawInParentSpace);

	if (not M_AnimMeshRecoilAxesSettings.bM_EnableOptionalRoll)
	{
		return;
	}

	const float RollJitterDeg = FMath::FRandRange(
		-M_AnimMeshRecoilAxesSettings.M_MaxOptionalRollDeg,
		M_AnimMeshRecoilAxesSettings.M_MaxOptionalRollDeg) * ClampedEnergy;

	if (M_AnimMeshRecoilAxesSettings.M_OptionalRollChannel == EAnimMeshOptionalRollChannel::Roll)
	{
		M_RecoilRotDeg.Z += RollJitterDeg;
		return;
	}

	M_RecoilRotDeg.X += RollJitterDeg;
}

void UAnimMeshFeedbackComponent::ApplyHullFeedbackTransform() const
{
	if (not GetIsValidAnimatedMesh())
	{
		return;
	}
	ApplyPreservedRecoilLocation();
	ApplyPreservedRecoilRotation();
}

bool UAnimMeshFeedbackComponent::GetIsValidAnimatedMesh() const
{
	if (IsValid(M_AnimatedMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_AnimatedMesh",
		"GetIsValidAnimatedMesh",
		this);
	return false;
}

FVector UAnimMeshFeedbackComponent::GetSignedAxisVector(const EAnimMeshRecoilSignedAxis Axis) const
{
	switch (Axis)
	{
		case EAnimMeshRecoilSignedAxis::ForwardX:
			return FVector::ForwardVector;
		case EAnimMeshRecoilSignedAxis::BackwardX:
			return -FVector::ForwardVector;
		case EAnimMeshRecoilSignedAxis::RightY:
			return FVector::RightVector;
		case EAnimMeshRecoilSignedAxis::LeftY:
			return -FVector::RightVector;
		case EAnimMeshRecoilSignedAxis::UpZ:
			return FVector::UpVector;
		case EAnimMeshRecoilSignedAxis::DownZ:
			return -FVector::UpVector;
		default:
			return -FVector::ForwardVector;
	}
}

FRotator UAnimMeshFeedbackComponent::GetMeshYawInParentSpace() const
{
	// Recoil never writes yaw, so the relative yaw is the mesh's own facing (its world yaw when it is the root).
	return FRotator(0.0f, M_AnimatedMesh->GetRelativeRotation().Yaw, 0.0f);
}

FRotator UAnimMeshFeedbackComponent::GetRecoilFrameYawInParentSpace() const
{
	const FRotator MeshYawInParentSpace = GetMeshYawInParentSpace();
	if (not GetHasValidTrackedWeapon())
	{
		return MeshYawInParentSpace;
	}
	const FName FireSocketName = M_TrackedWeapon->GetWeaponSocket();
	if (not M_AnimatedMesh->DoesSocketExist(FireSocketName))
	{
		return MeshYawInParentSpace;
	}

	FVector BarrelForward = M_AnimatedMesh->GetSocketTransform(FireSocketName).GetUnitAxis(EAxis::X);
	if (const USceneComponent* MeshParent = M_AnimatedMesh->GetAttachParent())
	{
		BarrelForward = MeshParent->GetComponentTransform().InverseTransformVectorNoScale(BarrelForward);
	}
	BarrelForward.Z = 0.0f;
	if (BarrelForward.IsNearlyZero())
	{
		return MeshYawInParentSpace;
	}
	return FRotator(0.0f, BarrelForward.Rotation().Yaw, 0.0f);
}

void UAnimMeshFeedbackComponent::AddNoseUpKickAlongBarrel(const float NoseUpDeg,
                                                          const FRotator& RecoilFrameYawInParentSpace)
{
	const float BarrelYawFromMeshDeg = FRotator::NormalizeAxis(
		RecoilFrameYawInParentSpace.Yaw - GetMeshYawInParentSpace().Yaw);
	float SinBarrelYaw = 0.0f;
	float CosBarrelYaw = 1.0f;
	FMath::SinCos(&SinBarrelYaw, &CosBarrelYaw, FMath::DegreesToRadians(BarrelYawFromMeshDeg));
	M_RecoilRotDeg.X += NoseUpDeg * CosBarrelYaw;
	// Positive roll lowers the mesh's +Y side, so lifting a barrel that points to +Y is a negative roll.
	M_RecoilRotDeg.Z -= NoseUpDeg * SinBarrelYaw;
}

void UAnimMeshFeedbackComponent::ApplyPreservedRecoilLocation() const
{
	const FVector CurrentRelativeLocation = M_AnimatedMesh->GetRelativeLocation();
	if (not IsFiniteVector( CurrentRelativeLocation)
		|| not IsFiniteVector(M_LastAppliedRecoilLocationOffsetCm)
		|| not IsFiniteVector(M_RecoilOffsetCm))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyPreservedRecoilLocation cancelled because recoil location values are invalid."));
		M_LastAppliedRecoilLocationOffsetCm = FVector::ZeroVector;
		return;
	}

	const FVector BaseLocationWithoutRecoil = CurrentRelativeLocation - M_LastAppliedRecoilLocationOffsetCm;
	const FVector RecoilLocationToApply = M_RecoilOffsetCm;
	const FVector NewRelativeLocation = BaseLocationWithoutRecoil + RecoilLocationToApply;
	if (not IsFiniteVector(NewRelativeLocation))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyPreservedRecoilLocation cancelled because resulting location is invalid."));
		M_LastAppliedRecoilLocationOffsetCm = FVector::ZeroVector;
		return;
	}

	M_AnimatedMesh->SetRelativeLocation(NewRelativeLocation);
	M_LastAppliedRecoilLocationOffsetCm = RecoilLocationToApply;
}

void UAnimMeshFeedbackComponent::ApplyPreservedRecoilRotation() const
{
	const FRotator CurrentRelativeRotation = M_AnimatedMesh->GetRelativeRotation();
	if (not IsFiniteRotator(CurrentRelativeRotation)
		|| not FMath::IsFinite(M_LastAppliedRecoilPitchDeg)
		|| not FMath::IsFinite(M_LastAppliedRecoilRollDeg))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyPreservedRecoilRotation cancelled because recoil rotation values are invalid."));
		M_LastAppliedRecoilPitchDeg = 0.0f;
		M_LastAppliedRecoilRollDeg = 0.0f;
		return;
	}

	const float BasePitchWithoutRecoil = CurrentRelativeRotation.Pitch - M_LastAppliedRecoilPitchDeg;
	const float BaseRollWithoutRecoil = CurrentRelativeRotation.Roll - M_LastAppliedRecoilRollDeg;

	float RecoilPitchToApplyDeg = 0.0f;
	float RecoilRollToApplyDeg = 0.0f;
	GetCurrentRecoilPitchAndRoll(RecoilPitchToApplyDeg, RecoilRollToApplyDeg);
	if (not FMath::IsFinite(RecoilPitchToApplyDeg) || not FMath::IsFinite(RecoilRollToApplyDeg))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyPreservedRecoilRotation cancelled because recoil values are invalid."));
		M_LastAppliedRecoilPitchDeg = 0.0f;
		M_LastAppliedRecoilRollDeg = 0.0f;
		return;
	}

	const FRotator NewRotation(
		BasePitchWithoutRecoil + RecoilPitchToApplyDeg,
		CurrentRelativeRotation.Yaw,
		BaseRollWithoutRecoil + RecoilRollToApplyDeg);
	if (not IsFiniteRotator(NewRotation))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyPreservedRecoilRotation cancelled because resulting rotation is invalid."));
		M_LastAppliedRecoilPitchDeg = 0.0f;
		M_LastAppliedRecoilRollDeg = 0.0f;
		return;
	}

	M_AnimatedMesh->SetRelativeRotation(NewRotation);
	M_LastAppliedRecoilPitchDeg = RecoilPitchToApplyDeg;
	M_LastAppliedRecoilRollDeg = RecoilRollToApplyDeg;
}

void UAnimMeshFeedbackComponent::GetCurrentRecoilPitchAndRoll(
	float& OutRecoilPitchDeg,
	float& OutRecoilRollDeg) const
{
	OutRecoilPitchDeg = M_RecoilRotDeg.X;
	// Z holds the roll jitter (roll channel only) and the share of the nose-up kick of a traversed barrel.
	OutRecoilRollDeg = M_RecoilRotDeg.Z;
}
