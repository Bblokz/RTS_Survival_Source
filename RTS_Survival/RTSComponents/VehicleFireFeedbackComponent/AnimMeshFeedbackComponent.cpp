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

	M_BaseHullRelativeLocation = M_AnimatedMesh->GetRelativeLocation();
	M_BaseHullRelativeRotation = M_AnimatedMesh->GetRelativeRotation();
}

void UAnimMeshFeedbackComponent::ApplyFeedbackKick(
	const float NormalisedMuzzleEnergy,
	const float TurretWorldYawDegrees)
{
	const float ClampedEnergy = FMath::Clamp(NormalisedMuzzleEnergy, 0.0f, 1.0f);
	const float RecoilKickMagnitudeCentimeters = M_FeedbackSettings.M_MaxBackCm * ClampedEnergy;

	FVector HullLocalKickDirection = -FVector::ForwardVector;
	if (FMath::IsFinite(TurretWorldYawDegrees) && GetIsValidAnimatedMesh())
	{
		const FVector TurretYawForwardWorld = FRotator(0.0f, TurretWorldYawDegrees, 0.0f).Vector();
		const FVector HullLocalYawForward = M_AnimatedMesh->GetComponentTransform()
			.InverseTransformVectorNoScale(TurretYawForwardWorld)
			.GetSafeNormal();

		if (not HullLocalYawForward.IsNearlyZero())
		{
			HullLocalKickDirection = -HullLocalYawForward;
		}
	}

	M_RecoilOffsetCm += HullLocalKickDirection * RecoilKickMagnitudeCentimeters;
	M_RecoilRotDeg.X += M_FeedbackSettings.M_MaxPitchDeg * ClampedEnergy;

	const float YawJitter = FMath::FRandRange(
		-M_FeedbackSettings.M_MaxYawJitterDeg,
		M_FeedbackSettings.M_MaxYawJitterDeg) * ClampedEnergy;
	M_RecoilRotDeg.Y += YawJitter;
}

void UAnimMeshFeedbackComponent::ApplyHullFeedbackTransform() const
{
	if (not GetIsValidAnimatedMesh())
	{
		return;
	}

	const FVector NewLocation = M_BaseHullRelativeLocation + M_RecoilOffsetCm;
	const FRotator NewRotation = M_BaseHullRelativeRotation +
		FRotator(M_RecoilRotDeg.X, M_RecoilRotDeg.Y, M_RecoilRotDeg.Z);

	M_AnimatedMesh->SetRelativeLocationAndRotation(NewLocation, NewRotation);
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
