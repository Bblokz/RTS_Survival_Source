#include "AnimMeshFeedbackComponent.h"

#include "RTS_Survival/RTSUtilities/RTSFunctionLibrary/RTSFunctionLibrary.h"

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

	M_LastAppliedRecoilPitchDeg = 0.0f;
}

void UAnimMeshFeedbackComponent::ApplyFeedbackKick(
	const float NormalisedMuzzleEnergy,
	const float TurretWorldYawDegrees)
{
	(void)TurretWorldYawDegrees;

	const float ClampedEnergy = FMath::Clamp(NormalisedMuzzleEnergy, 0.0f, 1.0f);
	M_RecoilRotDeg.X += M_FeedbackSettings.M_MaxPitchDeg * ClampedEnergy;
}

void UAnimMeshFeedbackComponent::ApplyHullFeedbackTransform() const
{
	if (not GetIsValidAnimatedMesh())
	{
		return;
	}

	const FRotator CurrentRelativeRotation = M_AnimatedMesh->GetRelativeRotation();
	const float BasePitchWithoutRecoil = CurrentRelativeRotation.Pitch - M_LastAppliedRecoilPitchDeg;
	const float RecoilPitchToApply = M_RecoilRotDeg.X;

	const FRotator NewRotation(
		BasePitchWithoutRecoil + RecoilPitchToApply,
		CurrentRelativeRotation.Yaw,
		CurrentRelativeRotation.Roll);

	M_AnimatedMesh->SetRelativeRotation(NewRotation);
	M_LastAppliedRecoilPitchDeg = RecoilPitchToApply;
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
