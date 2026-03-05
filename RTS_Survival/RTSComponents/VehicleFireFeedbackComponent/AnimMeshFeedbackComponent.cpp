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

	M_BaseHullRelativeRotation = M_AnimatedMesh->GetRelativeRotation();
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

	const FRotator NewRotation = M_BaseHullRelativeRotation +
		FRotator(M_RecoilRotDeg.X, 0.0f, 0.0f);

	M_AnimatedMesh->SetRelativeRotation(NewRotation);
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
