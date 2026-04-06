#include "VehicleFireFeedbackComponent.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

UVehicleFireFeedbackComponent::UVehicleFireFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVehicleFireFeedbackComponent::InitializeFeedbackComponent(
	USkeletalMeshComponent* InTrackRootMesh,
	UStaticMeshComponent* InHullMesh,
	ACPPTurretsMaster* InTurretMaster)
{
	M_TrackRootMesh = InTrackRootMesh;
	M_HullMesh = InHullMesh;
	M_TurretMaster = InTurretMaster;

	(void)GetIsValidTrackRootMesh();

	if (GetIsValidTurretMaster())
	{
		M_TurretMaster->SetVehicleFireFeedbackComponent(this);
	}

	CacheHullBaseTransform();
	EvaluateAllCurrentTurretWeapons();
}

void UVehicleFireFeedbackComponent::OnTurretWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
	TryTrackWeapon(WeaponIndex, Weapon);
}

void UVehicleFireFeedbackComponent::NotifyWeaponFired(
	const int32 WeaponIndex,
	const int32 WeaponCalibre,
	const float TurretWorldYawDegrees)
{
	if (WeaponIndex != M_TrackedWeaponIndex)
	{
		return;
	}

	if (GetIsValidOptimizationComponent() && not M_OptimizationComponent->GetIsInFOV())
	{
		return;
	}

	if (M_TrackedWeaponCalibre != WeaponCalibre)
	{
		M_TrackedWeaponCalibre = WeaponCalibre;
		M_CachedTrackedEnergy01 = GetNormalisedWeaponEnergy01(M_TrackedWeaponCalibre);
	}

	ApplyFeedbackKick(M_CachedTrackedEnergy01, TurretWorldYawDegrees);
	SetTickEnabledForActiveRecoil();
}

void UVehicleFireFeedbackComponent::SetOptimizationComponent(URTSOptimizer* InOptimizer)
{
	M_OptimizationComponent = InOptimizer;
}

void UVehicleFireFeedbackComponent::ForceResetRecoilAndSleep()
{
	M_RecoilOffsetCm = FVector::ZeroVector;
	M_RecoilVelocity = FVector::ZeroVector;
	M_RecoilRotDeg = FVector::ZeroVector;
	M_RecoilRotVelocity = FVector::ZeroVector;

	if (GetIsValidHullMesh())
	{
		ApplyHullFeedbackTransform();
	}

	SetComponentTickEnabled(false);
}

void UVehicleFireFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UVehicleFireFeedbackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (not GetIsValidHullMesh())
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (GetShouldCancelRecoilTickForUnsafeState(DeltaTime))
	{
		CancelRecoilAndRestoreBase(TEXT("TickComponent detected invalid/extreme recoil state."));
		return;
	}

	ApplyCriticallyDampedSpring(
		DeltaTime,
		M_FeedbackSettings.M_TranslationStiffness,
		M_FeedbackSettings.M_TranslationDamping,
		M_RecoilOffsetCm,
		M_RecoilVelocity);

	ApplyCriticallyDampedSpring(
		DeltaTime,
		M_FeedbackSettings.M_RotationStiffness,
		M_FeedbackSettings.M_RotationDamping,
		M_RecoilRotDeg,
		M_RecoilRotVelocity);

	if (GetShouldCancelRecoilTickForUnsafeState(DeltaTime))
	{
		CancelRecoilAndRestoreBase(TEXT("TickComponent spring update produced invalid/extreme recoil state."));
		return;
	}

	ApplyHullFeedbackTransform();
	TryResetRuntimeOffsetsToRestAndSleep();
}

bool UVehicleFireFeedbackComponent::GetIsValidHullMesh() const
{
	if (IsValid(M_HullMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_HullMesh",
		"GetIsValidHullMesh",
		GetOwner());
	return false;
}

bool UVehicleFireFeedbackComponent::GetIsValidTrackRootMesh() const
{
	if (IsValid(M_TrackRootMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrackRootMesh",
		"GetIsValidTrackRootMesh",
		this);
	return false;
}

bool UVehicleFireFeedbackComponent::GetIsValidTurretMaster() const
{
	if (M_TurretMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TurretMaster",
		"GetIsValidTurretMaster",
		this);
	return false;
}

bool UVehicleFireFeedbackComponent::GetHasValidTrackedWeapon() const
{
	return M_TrackedWeapon.IsValid() && M_TrackedWeaponIndex != INDEX_NONE;
}

bool UVehicleFireFeedbackComponent::GetIsValidOptimizationComponent() const
{
	if (M_OptimizationComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OptimizationComponent",
		"GetIsValidOptimizationComponent",
		this);
	return false;
}

void UVehicleFireFeedbackComponent::CacheHullBaseTransform()
{
	if (not GetIsValidHullMesh())
	{
		return;
	}

	M_BaseHullRelativeLocation = M_HullMesh->GetRelativeLocation();
	M_BaseHullRelativeRotation = M_HullMesh->GetRelativeRotation();
}

void UVehicleFireFeedbackComponent::EvaluateAllCurrentTurretWeapons()
{
	M_TrackedWeapon = nullptr;
	M_TrackedWeaponIndex = INDEX_NONE;
	M_TrackedWeaponCalibre = 0;
	M_CachedTrackedEnergy01 = 0.0f;

	if (not GetIsValidTurretMaster())
	{
		return;
	}

	const TArray<UWeaponState*> TurretWeapons = M_TurretMaster->GetWeapons();
	for (int32 WeaponIndex = 0; WeaponIndex < TurretWeapons.Num(); ++WeaponIndex)
	{
		TryTrackWeapon(WeaponIndex, TurretWeapons[WeaponIndex]);
	}
}

void UVehicleFireFeedbackComponent::TryTrackWeapon(const int32 WeaponIndex, UWeaponState* Weapon)
{
	if (not IsValid(Weapon))
	{
		return;
	}

	const int32 WeaponCalibre = FMath::RoundToInt(Weapon->GetRawWeaponData().WeaponCalibre);
	if (WeaponCalibre < DeveloperSettings::Optimization::MinCalibreWeaponFeedback)
	{
		return;
	}

	if (GetHasValidTrackedWeapon() && WeaponCalibre <= M_TrackedWeaponCalibre)
	{
		return;
	}

	M_TrackedWeapon = Weapon;
	M_TrackedWeaponIndex = WeaponIndex;
	M_TrackedWeaponCalibre = WeaponCalibre;
	M_CachedTrackedEnergy01 = GetNormalisedWeaponEnergy01(M_TrackedWeaponCalibre);
}

float UVehicleFireFeedbackComponent::GetNormalisedWeaponEnergy01(const int32 WeaponCalibre) const
{
	const int32 MaxCalibreForNormalization = FMath::Max(1, M_FeedbackSettings.M_MaxCalibreForNormalization);
	const float NormalisedEnergy = static_cast<float>(WeaponCalibre)
		/ static_cast<float>(MaxCalibreForNormalization);
	return FMath::Clamp(NormalisedEnergy, 0.0f, 1.0f);
}

void UVehicleFireFeedbackComponent::ApplyFeedbackKick(
	const float NormalisedMuzzleEnergy,
	const float TurretWorldYawDegrees)
{
	const float ClampedEnergy = FMath::Clamp(NormalisedMuzzleEnergy, 0.0f, 1.0f);
	const float RecoilKickMagnitudeCentimeters = M_FeedbackSettings.M_MaxBackCm * ClampedEnergy;

	FVector HullLocalKickDirection = -FVector::ForwardVector;
	if (FMath::IsFinite(TurretWorldYawDegrees) && GetIsValidHullMesh())
	{
		const FVector TurretYawForwardWorld = FRotator(0.0f, TurretWorldYawDegrees, 0.0f).Vector();
		const FVector HullLocalYawForward = M_HullMesh->GetComponentTransform()
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

void UVehicleFireFeedbackComponent::ApplyCriticallyDampedSpring(
	const float DeltaTime,
	const float Stiffness,
	const float Damping,
	FVector& InOutValue,
	FVector& InOutVelocity) const
{
	const FVector Acceleration = (-Stiffness * InOutValue) - (Damping * InOutVelocity);
	InOutVelocity += Acceleration * DeltaTime;
	InOutValue += InOutVelocity * DeltaTime;
}

void UVehicleFireFeedbackComponent::ApplyHullFeedbackTransform() const
{
	const FVector NewLocation = M_BaseHullRelativeLocation + M_RecoilOffsetCm;
	const FRotator NewRotation = M_BaseHullRelativeRotation +
		FRotator(M_RecoilRotDeg.X, M_RecoilRotDeg.Y, M_RecoilRotDeg.Z);

	M_HullMesh->SetRelativeLocationAndRotation(NewLocation, NewRotation);
}

void UVehicleFireFeedbackComponent::SetTickEnabledForActiveRecoil()
{
	if (IsComponentTickEnabled())
	{
		return;
	}

	SetComponentTickEnabled(true);
}

void UVehicleFireFeedbackComponent::TryResetRuntimeOffsetsToRestAndSleep()
{
	if (M_RecoilOffsetCm.SizeSquared() > M_FeedbackSettings.M_SetToRestEpsilon)
	{
		return;
	}

	if (M_RecoilVelocity.SizeSquared() > M_FeedbackSettings.M_SetToRestEpsilon)
	{
		return;
	}

	if (M_RecoilRotDeg.SizeSquared() > M_FeedbackSettings.M_SetToRestEpsilon)
	{
		return;
	}

	if (M_RecoilRotVelocity.SizeSquared() > M_FeedbackSettings.M_SetToRestEpsilon)
	{
		return;
	}

	M_RecoilOffsetCm = FVector::ZeroVector;
	M_RecoilVelocity = FVector::ZeroVector;
	M_RecoilRotDeg = FVector::ZeroVector;
	M_RecoilRotVelocity = FVector::ZeroVector;
	ApplyHullFeedbackTransform();

	if (not IsComponentTickEnabled())
	{
		return;
	}

	SetComponentTickEnabled(false);
}

bool UVehicleFireFeedbackComponent::GetShouldCancelRecoilTickForUnsafeState(const float DeltaTime) const
{
	const float MaxSafeDeltaTimeSeconds = FMath::Max(0.0f, M_FeedbackSettings.M_MaxSafeDeltaTimeSeconds);
	if (not FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
	{
		return true;
	}

	if (DeltaTime > MaxSafeDeltaTimeSeconds)
	{
		return true;
	}

	if (not FMath::IsFinite(M_FeedbackSettings.M_TranslationStiffness)
		|| not FMath::IsFinite(M_FeedbackSettings.M_TranslationDamping)
		|| not FMath::IsFinite(M_FeedbackSettings.M_RotationStiffness)
		|| not FMath::IsFinite(M_FeedbackSettings.M_RotationDamping))
	{
		return true;
	}

	const float MaxSafeOffsetCm = FMath::Max(0.0f, M_FeedbackSettings.M_MaxSafeOffsetCm);
	if (not GetIsFiniteAndWithinAbsLimit(M_RecoilOffsetCm, MaxSafeOffsetCm))
	{
		return true;
	}

	const float MaxSafeVelocityCmPerSecond = FMath::Max(0.0f, M_FeedbackSettings.M_MaxSafeVelocityCmPerSecond);
	if (not GetIsFiniteAndWithinAbsLimit(M_RecoilVelocity, MaxSafeVelocityCmPerSecond))
	{
		return true;
	}

	const float MaxSafeRotationDeg = FMath::Max(0.0f, M_FeedbackSettings.M_MaxSafeRotationDeg);
	if (not GetIsFiniteAndWithinAbsLimit(M_RecoilRotDeg, MaxSafeRotationDeg))
	{
		return true;
	}

	const float MaxSafeRotationVelocityDegPerSecond = FMath::Max(
		0.0f,
		M_FeedbackSettings.M_MaxSafeRotationVelocityDegPerSecond);
	return not GetIsFiniteAndWithinAbsLimit(M_RecoilRotVelocity, MaxSafeRotationVelocityDegPerSecond);
}

void UVehicleFireFeedbackComponent::CancelRecoilAndRestoreBase(const TCHAR* const ReportReason)
{
	RTSFunctionLibrary::ReportError(ReportReason);
	ForceResetRecoilAndSleep();
}

bool UVehicleFireFeedbackComponent::GetIsFiniteAndWithinAbsLimit(
	const FVector& Vector,
	const float MaxAbsValue) const
{
	return GetIsFiniteAndWithinAbsLimit(Vector.X, MaxAbsValue)
		&& GetIsFiniteAndWithinAbsLimit(Vector.Y, MaxAbsValue)
		&& GetIsFiniteAndWithinAbsLimit(Vector.Z, MaxAbsValue);
}

bool UVehicleFireFeedbackComponent::GetIsFiniteAndWithinAbsLimit(
	const float Value,
	const float MaxAbsValue) const
{
	if (not FMath::IsFinite(Value))
	{
		return false;
	}

	if (not FMath::IsFinite(MaxAbsValue))
	{
		return false;
	}

	return FMath::Abs(Value) <= MaxAbsValue;
}
