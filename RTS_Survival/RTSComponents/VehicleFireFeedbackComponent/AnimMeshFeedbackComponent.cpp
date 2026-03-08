#include "AnimMeshFeedbackComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

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

	const float ClampedEnergy = FMath::Clamp(NormalisedMuzzleEnergy, 0.0f, 1.0f);
	const FVector BackwardAxis = GetSignedAxisVector(M_AnimMeshRecoilAxesSettings.M_BackwardRecoilAxis);
	const FVector BackwardRecoilOffset =
		BackwardAxis * (M_FeedbackSettings.M_MaxBackCm * ClampedEnergy);
	M_RecoilOffsetCm += BackwardRecoilOffset;

	if (M_AnimMeshRecoilAxesSettings.bM_EnableSideSway)
	{
		const FVector SideSwayAxis = GetSignedAxisVector(M_AnimMeshRecoilAxesSettings.M_SideSwayAxis);
		const float SideSwayJitterCm = FMath::FRandRange(
			-M_AnimMeshRecoilAxesSettings.M_MaxSideSwayCm,
			M_AnimMeshRecoilAxesSettings.M_MaxSideSwayCm) * ClampedEnergy;
		M_RecoilOffsetCm += SideSwayAxis * SideSwayJitterCm;
	}

	M_RecoilRotDeg.X += M_FeedbackSettings.M_MaxPitchDeg * ClampedEnergy;

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

void UAnimMeshFeedbackComponent::ApplyPreservedRecoilLocation() const
{
	const FVector CurrentRelativeLocation = M_AnimatedMesh->GetRelativeLocation();
	const FVector BaseLocationWithoutRecoil = CurrentRelativeLocation - M_LastAppliedRecoilLocationOffsetCm;
	const FVector RecoilLocationToApply = M_RecoilOffsetCm;
	const FVector NewRelativeLocation = BaseLocationWithoutRecoil + RecoilLocationToApply;

	M_AnimatedMesh->SetRelativeLocation(NewRelativeLocation);
	M_LastAppliedRecoilLocationOffsetCm = RecoilLocationToApply;
}

void UAnimMeshFeedbackComponent::ApplyPreservedRecoilRotation() const
{
	const FRotator CurrentRelativeRotation = M_AnimatedMesh->GetRelativeRotation();
	const float BasePitchWithoutRecoil = CurrentRelativeRotation.Pitch - M_LastAppliedRecoilPitchDeg;
	const float BaseRollWithoutRecoil = CurrentRelativeRotation.Roll - M_LastAppliedRecoilRollDeg;

	float RecoilPitchToApplyDeg = 0.0f;
	float RecoilRollToApplyDeg = 0.0f;
	GetCurrentRecoilPitchAndRoll(RecoilPitchToApplyDeg, RecoilRollToApplyDeg);

	const FRotator NewRotation(
		BasePitchWithoutRecoil + RecoilPitchToApplyDeg,
		CurrentRelativeRotation.Yaw,
		BaseRollWithoutRecoil + RecoilRollToApplyDeg);

	M_AnimatedMesh->SetRelativeRotation(NewRotation);
	M_LastAppliedRecoilPitchDeg = RecoilPitchToApplyDeg;
	M_LastAppliedRecoilRollDeg = RecoilRollToApplyDeg;
}

void UAnimMeshFeedbackComponent::GetCurrentRecoilPitchAndRoll(
	float& OutRecoilPitchDeg,
	float& OutRecoilRollDeg) const
{
	OutRecoilPitchDeg = M_RecoilRotDeg.X;

	if (M_AnimMeshRecoilAxesSettings.M_OptionalRollChannel == EAnimMeshOptionalRollChannel::Roll)
	{
		OutRecoilRollDeg = M_RecoilRotDeg.Z;
		return;
	}

	OutRecoilRollDeg = 0.0f;
}
