// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GeometryDamageComponent.h"

#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Pawn.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Player/Camera/CameraPawn.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundCue.h"

namespace
{
	constexpr float FieldWeightMagnitude = 1.f;

	FVector GetTorqueAxis(const FGeometryDamageActiveImpact& Impact)
	{
		return Impact.ImpactNormal.IsNearlyZero()
			? FVector::UpVector
			: Impact.ImpactNormal.GetSafeNormal();
	}

	UFieldNodeVector* CreateArchetypeVectorField(
		UGeometryCollectionComponent* GeometryCollection,
		const FGeometryDamageActiveImpact& Impact,
		const EGeometryDamageForceArchetype Archetype,
		const float Magnitude)
	{
		switch (Archetype)
		{
		case EGeometryDamageForceArchetype::RadialBurst:
			return NewObject<URadialVector>(GeometryCollection)->SetRadialVector(Magnitude, Impact.WorldLocation);
		case EGeometryDamageForceArchetype::DirectionalPunch:
			return NewObject<UUniformVector>(GeometryCollection)->SetUniformVector(
				Magnitude, Impact.ResolvedShotDirection);
		case EGeometryDamageForceArchetype::Implosion:
			return NewObject<URadialVector>(GeometryCollection)->SetRadialVector(-Magnitude, Impact.WorldLocation);
		case EGeometryDamageForceArchetype::ChaoticJitter:
			return NewObject<URandomVector>(GeometryCollection)->SetRandomVector(Magnitude);
		case EGeometryDamageForceArchetype::TorqueTwist:
			return NewObject<UUniformVector>(GeometryCollection)->SetUniformVector(Magnitude, GetTorqueAxis(Impact));
		default:
			return nullptr;
		}
	}
}

UGeometryDamageComponent::UGeometryDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = true;
}

bool UGeometryDamageComponent::InitGeometryDamage(
	UGeometryCollectionComponent* InGeometryCollection,
	UHealthComponent* InHealthComponent)
{
	StopGeometryDamage();
	bM_IsInitialised = false;
	M_GeometryCollection = InGeometryCollection;
	M_HealthComponent.Reset();

	if (not ValidateAndPrepareGeometryCollectionForInitialisation())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	M_HealthComponent = IsValid(InHealthComponent)
		? InHealthComponent
		: Owner->FindComponentByClass<UHealthComponent>();
	M_FiredThresholdStages.Init(false, ThresholdStages.Num());
	M_LastThresholdFXTime = GetWorld()
		? GetWorld()->GetTimeSeconds() - SimulationBudget.MinSecondsBetweenThresholdFX
		: -SimulationBudget.MinSecondsBetweenThresholdFX;
	bM_IsInitialised = true;
	ValidateConfigurationWarnings();
	return true;
}

bool UGeometryDamageComponent::ValidateAndPrepareGeometryCollectionForInitialisation()
{
	if (not GetIsValidGeometryCollection())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError("Geometry damage initialisation failed: the component has no valid owner.");
		return false;
	}

	if (M_GeometryCollection->GetOwner() != Owner)
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage initialisation failed: the Geometry Collection must belong to the same actor.");
		return false;
	}

	if (not IsValid(M_GeometryCollection->GetRestCollection()))
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage initialisation failed: the Geometry Collection has no Rest Collection asset.");
		return false;
	}

	if (M_GeometryCollection->ObjectType != EObjectStateTypeEnum::Chaos_Object_Kinematic)
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage initialisation failed: ObjectType must be Chaos_Object_Kinematic, but is "
			+ UEnum::GetValueAsString(M_GeometryCollection->ObjectType) + ".");
		return false;
	}

	if (not M_GeometryCollection->IsSimulatingPhysics())
	{
		RTSFunctionLibrary::ReportWarning(
			"Geometry damage enabled simulation on a non-simulating Geometry Collection; fields require a physics proxy.");
		M_GeometryCollection->SetSimulatePhysics(true);
	}

	if (M_GeometryCollection->IsSimulatingPhysics())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Geometry damage initialisation failed: the Geometry Collection could not enable physics simulation.");
	return false;
}

void UGeometryDamageComponent::OnDamageTaken(const FGeometryDamageHit& Hit)
{
	HandleDamageTaken(Hit, nullptr);
}

void UGeometryDamageComponent::OnDamageTakenFromEvent(
	const float DamageAmount,
	const FPointDamageEvent& DamageEvent,
	AActor* DamageCauser)
{
	FGeometryDamageHit Hit;
	Hit.DamageAmount = DamageAmount;
	Hit.WorldHitLocation = DamageEvent.HitInfo.ImpactPoint.IsNearlyZero()
		? DamageEvent.HitInfo.Location
		: DamageEvent.HitInfo.ImpactPoint;
	Hit.ImpactNormal = DamageEvent.HitInfo.ImpactNormal;
	Hit.ShotDirection = DamageEvent.ShotDirection;
	HandleDamageTaken(Hit, DamageCauser);
}

void UGeometryDamageComponent::StopGeometryDamage()
{
	M_PendingImpacts.Reset();
	M_ActiveImpacts.Reset();
	M_WindowExpiryTime = 0.f;
	SetComponentTickEnabled(false);
}

void UGeometryDamageComponent::Debug_FireTestImpact(
	const FVector WorldLocation,
	const float DamageAmount)
{
	FGeometryDamageHit TestHit;
	TestHit.DamageAmount = DamageAmount;
	TestHit.WorldHitLocation = WorldLocation;
	TestHit.MaxHealthOverride = DebugMaxHealthOverride;
	TestHit.HealthPercentAfterOverride = 1.f;
	HandleDamageTaken(TestHit, nullptr);

	UWorld* World = GetWorld();
	if (IsValid(World) )
	{
		Tick_ResolvePendingImpacts();
	}
}

void UGeometryDamageComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (not bM_IsInitialised || not GetIsValidGeometryCollection())
	{
		SetComponentTickEnabled(false);
		return;
	}

	Tick_ReissueSustainedForces(DeltaTime);
	Tick_ResolvePendingImpacts();

	const UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimeSeconds() <= M_WindowExpiryTime)
	{
		return;
	}

	QuiesceSimulation();
}

void UGeometryDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopGeometryDamage();
	bM_IsInitialised = false;
	M_GeometryCollection = nullptr;
	M_HealthComponent.Reset();
	M_FiredThresholdStages.Reset();
	Super::EndPlay(EndPlayReason);
}

void UGeometryDamageComponent::HandleDamageTaken(
	const FGeometryDamageHit& Hit,
	const AActor* DamageCauser)
{
	if (not bM_IsInitialised)
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage impact ignored: InitGeometryDamage must succeed before impacts are submitted.");
		return;
	}

	if (not GetIsValidGeometryCollection() || Hit.DamageAmount <= 0.f)
	{
		return;
	}

	float MaxHealth = 0.f;
	float HealthPercentAfter = 0.f;
	if (not ResolveHealthContext(Hit, MaxHealth, HealthPercentAfter))
	{
		return;
	}

	const float Severity = CalculateSeverity(Hit.DamageAmount, MaxHealth);
	if (Severity <= 0.f)
	{
		return;
	}

	FGeometryDamageActiveImpact Impact;
	Impact.WorldLocation = Hit.WorldHitLocation;
	Impact.ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
	Impact.Severity = Severity;
	Impact.ImpactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Impact.bForceRadialFallback = not ResolveShotDirection(Hit, DamageCauser, Impact.ResolvedShotDirection);
	Impact.SustainedForceMultiplier = GetSustainedForceMultiplier(HealthPercentAfter);

	EvaluateThresholdCrossings(Hit, Impact, MaxHealth, HealthPercentAfter);
	M_PendingImpacts.Add(Impact);
	OpenOrResetSimulationWindow();
}

bool UGeometryDamageComponent::ResolveHealthContext(
	const FGeometryDamageHit& Hit,
	float& OutMaxHealth,
	float& OutHealthPercentAfter) const
{
	const bool bNeedsLiveMaxHealth = Hit.MaxHealthOverride < 0.f;
	const bool bNeedsLiveHealthPercent = Hit.HealthPercentAfterOverride < 0.f;
	if ((bNeedsLiveMaxHealth || bNeedsLiveHealthPercent) && not GetIsValidHealthComponent())
	{
		return false;
	}

	OutMaxHealth = bNeedsLiveMaxHealth
		? M_HealthComponent->GetMaxHealth()
		: Hit.MaxHealthOverride;
	if (OutMaxHealth < GeometryDamageConstants::MinValidMaxHealth)
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage impact ignored: max health must be at least 1 for severity normalisation.");
		return false;
	}

	if (bNeedsLiveHealthPercent
		&& M_HealthComponent->GetMaxHealth() < GeometryDamageConstants::MinValidMaxHealth)
	{
		RTSFunctionLibrary::ReportError(
			"Geometry damage impact ignored: the health component cannot provide a valid health percentage.");
		return false;
	}

	OutHealthPercentAfter = bNeedsLiveHealthPercent
		? M_HealthComponent->GetHealthPercentage()
		: Hit.HealthPercentAfterOverride;
	OutHealthPercentAfter = FMath::Clamp(OutHealthPercentAfter, 0.f, 1.f);
	return true;
}

float UGeometryDamageComponent::CalculateSeverity(
	const float DamageAmount,
	const float MaxHealth) const
{
	const float DamageFraction = DamageAmount / MaxHealth;
	const float Deadzone = FMath::Clamp(ScalingSettings.DeadzoneFraction, 0.f, 1.f);
	if (DamageFraction <= Deadzone)
	{
		return 0.f;
	}

	const float FullResponse = FMath::Clamp(ScalingSettings.FullResponseFraction, 0.f, 1.f);
	const float ResponseRange = FullResponse - Deadzone;
	const float NormalisedDamage = ResponseRange <= UE_SMALL_NUMBER
		? 1.f
		: FMath::Clamp((DamageFraction - Deadzone) / ResponseRange, 0.f, 1.f);

	const float ShapedResponse = IsValid(ScalingSettings.ResponseCurve)
		? ScalingSettings.ResponseCurve->GetFloatValue(NormalisedDamage)
		: FMath::Pow(NormalisedDamage, FMath::Max(ScalingSettings.ResponseExponent, UE_SMALL_NUMBER));
	return FMath::Clamp(ShapedResponse * ScalingSettings.GlobalSeverityMultiplier, 0.f, 1.f);
}

bool UGeometryDamageComponent::ResolveShotDirection(
	const FGeometryDamageHit& Hit,
	const AActor* DamageCauser,
	FVector& OutDirection) const
{
	if (not Hit.ShotDirection.IsNearlyZero())
	{
		OutDirection = Hit.ShotDirection.GetSafeNormal();
		return true;
	}

	if (not Hit.ImpactNormal.IsNearlyZero())
	{
		OutDirection = -Hit.ImpactNormal.GetSafeNormal();
		return true;
	}

	if (IsValid(DamageCauser))
	{
		OutDirection = (Hit.WorldHitLocation - DamageCauser->GetActorLocation()).GetSafeNormal();
		if (not OutDirection.IsNearlyZero())
		{
			return true;
		}
	}

	OutDirection = FVector::ZeroVector;
	return false;
}

void UGeometryDamageComponent::DispatchStrainField(
	const FGeometryDamageActiveImpact& Impact,
	const FGeometryDamageForceProfile& Profile)
{
	if (not GetIsValidGeometryCollection())
	{
		return;
	}

	const float StrainMagnitude = FMath::Lerp(Profile.MinStrain, Profile.MaxStrain, Impact.Severity)
		* GlobalStrainMultiplier;
	const float ForceRadius = FMath::Lerp(Profile.MinRadius, Profile.MaxRadius, Impact.Severity);
	const float StrainRadius = FMath::Min(ForceRadius, Profile.MaxStrainRadius);
	if (StrainMagnitude <= 0.f || StrainRadius <= 0.f)
	{
		return;
	}

	URadialFalloff* StrainFalloff = NewObject<URadialFalloff>(M_GeometryCollection);
	if (not IsValid(StrainFalloff))
	{
		return;
	}

	StrainFalloff->SetRadialFalloff(
		StrainMagnitude,
		0.f,
		1.f,
		0.f,
		StrainRadius,
		Impact.WorldLocation,
		Profile.FalloffType);
	M_GeometryCollection->ApplyPhysicsField(
		true,
		EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
		nullptr,
		StrainFalloff);
}

void UGeometryDamageComponent::DispatchForceFields(
	const FGeometryDamageActiveImpact& Impact,
	const FGeometryDamageForceProfile& Profile,
	const float DecayWeight)
{
	const float ForceMagnitude = FMath::Lerp(Profile.MinForce, Profile.MaxForce, Impact.Severity)
		* GlobalForceMultiplier
		* Impact.SustainedForceMultiplier
		* FMath::Clamp(DecayWeight, 0.f, 1.f);
	if (ForceMagnitude <= 0.f)
	{
		return;
	}

	if (Impact.bForceRadialFallback || Profile.Archetype == EGeometryDamageForceArchetype::RadialBurst)
	{
		DispatchArchetypeGraph(
			Impact, Profile, EGeometryDamageForceArchetype::RadialBurst, ForceMagnitude);
		return;
	}

	const float RadialBlend = FMath::Clamp(Profile.RadialVsDirectionalBlend, 0.f, 1.f);
	const float ArchetypeMagnitude = ForceMagnitude * (1.f - RadialBlend);
	const float RadialMagnitude = ForceMagnitude * RadialBlend;
	if (ArchetypeMagnitude > 0.f)
	{
		DispatchArchetypeGraph(Impact, Profile, Profile.Archetype, ArchetypeMagnitude);
	}

	if (RadialMagnitude > 0.f)
	{
		DispatchArchetypeGraph(
			Impact, Profile, EGeometryDamageForceArchetype::RadialBurst, RadialMagnitude);
	}
}

void UGeometryDamageComponent::DispatchArchetypeGraph(
	const FGeometryDamageActiveImpact& Impact,
	const FGeometryDamageForceProfile& Profile,
	const EGeometryDamageForceArchetype Archetype,
	const float Magnitude)
{
	if (not GetIsValidGeometryCollection())
	{
		return;
	}

	UFieldNodeVector* VectorField = CreateArchetypeVectorField(
		M_GeometryCollection, Impact, Archetype, Magnitude);
	if (not IsValid(VectorField))
	{
		return;
	}

	const float Radius = FMath::Lerp(Profile.MinRadius, Profile.MaxRadius, Impact.Severity);
	URadialFalloff* Falloff = NewObject<URadialFalloff>(M_GeometryCollection);
	if (not IsValid(Falloff))
	{
		return;
	}

	Falloff->SetRadialFalloff(
		FieldWeightMagnitude,
		0.f,
		1.f,
		0.f,
		Radius,
		Impact.WorldLocation,
		Profile.FalloffType);

	UOperatorField* Multiply = NewObject<UOperatorField>(M_GeometryCollection);
	if (not IsValid(Multiply))
	{
		return;
	}

	Multiply->SetOperatorField(
		FieldWeightMagnitude,
		VectorField,
		Falloff,
		EFieldOperationType::Field_Multiply);
	const EGeometryCollectionPhysicsTypeEnum PhysicsType =
		Archetype == EGeometryDamageForceArchetype::TorqueTwist
			? EGeometryCollectionPhysicsTypeEnum::Chaos_AngularTorque
			: EGeometryCollectionPhysicsTypeEnum::Chaos_LinearForce;
	M_GeometryCollection->ApplyPhysicsField(true, PhysicsType, nullptr, Multiply);
}

void UGeometryDamageComponent::EvaluateThresholdCrossings(
	const FGeometryDamageHit& Hit,
	const FGeometryDamageActiveImpact& Impact,
	const float MaxHealth,
	const float HealthPercentAfter)
{
	if (M_FiredThresholdStages.Num() != ThresholdStages.Num())
	{
		M_FiredThresholdStages.Init(false, ThresholdStages.Num());
	}

	TArray<int32> OrderedStageIndices;
	OrderedStageIndices.Reserve(ThresholdStages.Num());
	for (int32 StageIndex = 0; StageIndex < ThresholdStages.Num(); ++StageIndex)
	{
		OrderedStageIndices.Add(StageIndex);
	}

	OrderedStageIndices.Sort([this](const int32 FirstIndex, const int32 SecondIndex)
	{
		return GetHealthLevelThreshold(ThresholdStages[FirstIndex].TriggerLevel)
			< GetHealthLevelThreshold(ThresholdStages[SecondIndex].TriggerLevel);
	});

	const float HealthPercentBefore = FMath::Clamp(
		HealthPercentAfter + Hit.DamageAmount / MaxHealth, 0.f, 1.f);
	for (const int32 StageIndex : OrderedStageIndices)
	{
		EvaluateThresholdStage(StageIndex, Hit, Impact, HealthPercentBefore, HealthPercentAfter);
	}
}

void UGeometryDamageComponent::EvaluateThresholdStage(
	const int32 StageIndex,
	const FGeometryDamageHit& Hit,
	const FGeometryDamageActiveImpact& Impact,
	const float HealthPercentBefore,
	const float HealthPercentAfter)
{
	const FGeometryDamageThresholdStage& Stage = ThresholdStages[StageIndex];
	const float Threshold = GetHealthLevelThreshold(Stage.TriggerLevel);
	if (Threshold < 0.f)
	{
		return;
	}

	if (M_FiredThresholdStages[StageIndex] && Stage.bRearmOnHeal
		&& HealthPercentBefore > Threshold)
	{
		M_FiredThresholdStages[StageIndex] = false;
	}

	if (M_FiredThresholdStages[StageIndex] || HealthPercentAfter > Threshold)
	{
		return;
	}

	M_FiredThresholdStages[StageIndex] = true;
	FireThresholdStage(StageIndex, Hit, Impact);
}

void UGeometryDamageComponent::FireThresholdStage(
	const int32 StageIndex,
	const FGeometryDamageHit& Hit,
	const FGeometryDamageActiveImpact& Impact)
{
	if (not ThresholdStages.IsValidIndex(StageIndex))
	{
		return;
	}

	const FGeometryDamageThresholdStage& Stage = ThresholdStages[StageIndex];
	if (Stage.bFireCrossingBurst && GetShouldDispatchFieldsAtLocation(Impact.WorldLocation))
	{
		FGeometryDamageActiveImpact BurstImpact = Impact;
		BurstImpact.SustainedForceMultiplier = 1.f;
		DispatchStrainField(BurstImpact, Stage.CrossingBurst);
		DispatchForceFields(BurstImpact, Stage.CrossingBurst, 1.f);
	}

	PlayThresholdFX(Stage.CrossingFX, Hit, Impact.Severity);
}

void UGeometryDamageComponent::PlayThresholdFX(
	const FGeometryDamageFX& FX,
	const FGeometryDamageHit& Hit,
	const float Severity)
{
	const bool bHasSound = IsValid(FX.ImpactSound);
	const bool bHasVfx = IsValid(FX.ImpactVfx);
	if (not bHasSound && not bHasVfx)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - M_LastThresholdFXTime < SimulationBudget.MinSecondsBetweenThresholdFX)
	{
		return;
	}
	M_LastThresholdFXTime = Now;

	const FVector SafeImpactNormal = Hit.ImpactNormal.IsNearlyZero()
		? FVector::UpVector
		: Hit.ImpactNormal.GetSafeNormal();
	const FRotator ImpactRotation = FX.bAlignVfxToImpactNormal
		? FRotationMatrix::MakeFromZ(SafeImpactNormal).Rotator()
		: FRotator::ZeroRotator;
	if (bHasSound)
	{
		const float Volume = FMath::Lerp(FX.MinVolumeMultiplier, 1.f, Severity);
		const float MinPitch = FMath::Min(FX.PitchRange.X, FX.PitchRange.Y);
		const float MaxPitch = FMath::Max(FX.PitchRange.X, FX.PitchRange.Y);
		UGameplayStatics::PlaySoundAtLocation(
			World,
			FX.ImpactSound,
			Hit.WorldHitLocation,
			ImpactRotation,
			Volume,
			FMath::FRandRange(MinPitch, MaxPitch),
			0.f,
			FX.SoundAttenuation,
			FX.SoundConcurrency);
	}

	if (bHasVfx)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			FX.ImpactVfx,
			Hit.WorldHitLocation,
			ImpactRotation,
			FX.VfxScale);
	}
}

float UGeometryDamageComponent::GetSustainedForceMultiplier(
	const float HealthPercentAfter) const
{
	float SustainedMultiplier = 1.f;
	for (const FGeometryDamageThresholdStage& Stage : ThresholdStages)
	{
		const float Threshold = GetHealthLevelThreshold(Stage.TriggerLevel);
		if (Threshold >= 0.f && HealthPercentAfter <= Threshold)
		{
			SustainedMultiplier = FMath::Max(SustainedMultiplier, Stage.SustainedForceMultiplier);
		}
	}
	return SustainedMultiplier;
}

float UGeometryDamageComponent::GetHealthLevelThreshold(const EHealthLevel HealthLevel)
{
	switch (HealthLevel)
	{
	case EHealthLevel::Level_100Percent:
		return 1.f;
	case EHealthLevel::Level_75Percent:
		return 0.75f;
	case EHealthLevel::Level_66Percent:
		return 0.66f;
	case EHealthLevel::Level_50Percent:
		return 0.5f;
	case EHealthLevel::Level_33Percent:
		return 0.33f;
	case EHealthLevel::Level_25Percent:
		return 0.25f;
	case EHealthLevel::Level_NoAction:
	default:
		return -1.f;
	}
}

void UGeometryDamageComponent::OpenOrResetSimulationWindow()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_WindowExpiryTime = World->GetTimeSeconds()
		+ FMath::Max(SimulationBudget.SimulateAfterImpactSeconds, 0.f);
	SetComponentTickEnabled(true);
}

void UGeometryDamageComponent::QuiesceSimulation()
{
	if (GetIsValidGeometryCollection())
	{
		UUniformScalar* SleepField = NewObject<UUniformScalar>(M_GeometryCollection);
		if (IsValid(SleepField))
		{
			SleepField->SetUniformScalar(SimulationBudget.QuiesceSleepThreshold);
			M_GeometryCollection->ApplyPhysicsField(
				true,
				EGeometryCollectionPhysicsTypeEnum::Chaos_SleepingThreshold,
				nullptr,
				SleepField);
		}
	}

	M_ActiveImpacts.Reset();
	M_PendingImpacts.Reset();
	M_WindowExpiryTime = 0.f;
	SetComponentTickEnabled(false);
}

void UGeometryDamageComponent::Tick_ResolvePendingImpacts()
{
	if (M_PendingImpacts.IsEmpty())
	{
		return;
	}

	TArray<FGeometryDamageActiveImpact> MergedImpacts;
	MergedImpacts.Reserve(M_PendingImpacts.Num());
	for (int32 PendingIndex = 0; PendingIndex < M_PendingImpacts.Num(); ++PendingIndex)
	{
		MergePendingImpact(MergedImpacts, M_PendingImpacts[PendingIndex]);
	}
	M_PendingImpacts.Reset();

	MergedImpacts.Sort([](const FGeometryDamageActiveImpact& First, const FGeometryDamageActiveImpact& Second)
	{
		return First.Severity > Second.Severity;
	});

	const int32 MaxDispatchedImpacts = FMath::Max(1, SimulationBudget.MaxFieldDispatchesPerTick);
	const float EffectiveImpulseWindow = FMath::Min(
		SimulationBudget.ImpulseWindowSeconds,
		SimulationBudget.SimulateAfterImpactSeconds);
	for (int32 ImpactIndex = 0; ImpactIndex < MergedImpacts.Num(); ++ImpactIndex)
	{
		ActivateMergedImpact(
			MergedImpacts[ImpactIndex], ImpactIndex < MaxDispatchedImpacts, EffectiveImpulseWindow);
	}
}

void UGeometryDamageComponent::ActivateMergedImpact(
	const FGeometryDamageActiveImpact& Impact,
	const bool bDispatchInitialFields,
	const float EffectiveImpulseWindow)
{
	if (not GetShouldDispatchFieldsAtLocation(Impact.WorldLocation))
	{
		return;
	}

	if (bDispatchInitialFields)
	{
		DispatchStrainField(Impact, DefaultForceProfile);
		DispatchForceFields(Impact, DefaultForceProfile, 1.f);
	}

	if (EffectiveImpulseWindow > 0.f)
	{
		M_ActiveImpacts.Add(Impact);
	}
}

void UGeometryDamageComponent::Tick_ReissueSustainedForces(const float DeltaTime)
{
	static_cast<void>(DeltaTime);
	if (M_ActiveImpacts.IsEmpty())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float ImpulseWindow = FMath::Min(
		SimulationBudget.ImpulseWindowSeconds,
		SimulationBudget.SimulateAfterImpactSeconds);
	const float Now = World->GetTimeSeconds();
	for (int32 ImpactIndex = M_ActiveImpacts.Num() - 1; ImpactIndex >= 0; --ImpactIndex)
	{
		if (ImpulseWindow <= 0.f || Now - M_ActiveImpacts[ImpactIndex].ImpactTime >= ImpulseWindow)
		{
			M_ActiveImpacts.RemoveAt(ImpactIndex);
		}
	}

	M_ActiveImpacts.Sort([](const FGeometryDamageActiveImpact& First, const FGeometryDamageActiveImpact& Second)
	{
		return First.Severity > Second.Severity;
	});

	const int32 DispatchCount = FMath::Min(
		M_ActiveImpacts.Num(),
		FMath::Max(1, SimulationBudget.MaxFieldDispatchesPerTick));
	for (int32 ImpactIndex = 0; ImpactIndex < DispatchCount; ++ImpactIndex)
	{
		const FGeometryDamageActiveImpact& Impact = M_ActiveImpacts[ImpactIndex];
		const float ElapsedFraction = FMath::Clamp(
			(Now - Impact.ImpactTime) / ImpulseWindow, 0.f, 1.f);
		const float DecayWeight = IsValid(DefaultForceProfile.ForceDecayCurve)
			? FMath::Clamp(DefaultForceProfile.ForceDecayCurve->GetFloatValue(ElapsedFraction), 0.f, 1.f)
			: 1.f - ElapsedFraction;
		DispatchForceFields(Impact, DefaultForceProfile, DecayWeight);
	}
}

void UGeometryDamageComponent::MergePendingImpact(
	TArray<FGeometryDamageActiveImpact>& InOutMergedImpacts,
	const FGeometryDamageActiveImpact& PendingImpact) const
{
	const float MergeDistanceSquared = FMath::Square(GeometryDamageConstants::ImpactMergeDistance);
	for (int32 MergedIndex = 0; MergedIndex < InOutMergedImpacts.Num(); ++MergedIndex)
	{
		if (FVector::DistSquared(InOutMergedImpacts[MergedIndex].WorldLocation, PendingImpact.WorldLocation)
			> MergeDistanceSquared)
		{
			continue;
		}

		MergeImpact(InOutMergedImpacts[MergedIndex], PendingImpact);
		return;
	}

	InOutMergedImpacts.Add(PendingImpact);
}

void UGeometryDamageComponent::MergeImpact(
	FGeometryDamageActiveImpact& InOutImpact,
	const FGeometryDamageActiveImpact& NewImpact)
{
	const float ExistingSeverity = InOutImpact.Severity;
	const float AddedSeverity = NewImpact.Severity;
	const float TotalCentroidWeight = ExistingSeverity + AddedSeverity;
	if (TotalCentroidWeight > UE_SMALL_NUMBER)
	{
		InOutImpact.WorldLocation = (
			InOutImpact.WorldLocation * ExistingSeverity
			+ NewImpact.WorldLocation * AddedSeverity) / TotalCentroidWeight;
	}

	InOutImpact.ResolvedShotDirection = (
		InOutImpact.ResolvedShotDirection * ExistingSeverity
		+ NewImpact.ResolvedShotDirection * AddedSeverity).GetSafeNormal();
	InOutImpact.ImpactNormal = (
		InOutImpact.ImpactNormal * ExistingSeverity
		+ NewImpact.ImpactNormal * AddedSeverity).GetSafeNormal();
	InOutImpact.Severity = 1.f - (1.f - ExistingSeverity) * (1.f - AddedSeverity);
	InOutImpact.ImpactTime = FMath::Max(InOutImpact.ImpactTime, NewImpact.ImpactTime);
	InOutImpact.SustainedForceMultiplier = FMath::Max(
		InOutImpact.SustainedForceMultiplier,
		NewImpact.SustainedForceMultiplier);
	InOutImpact.bForceRadialFallback = InOutImpact.ResolvedShotDirection.IsNearlyZero();
}

bool UGeometryDamageComponent::GetShouldDispatchFieldsAtLocation(
	const FVector& WorldLocation) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const ACameraPawn* CameraPawn = Cast<ACameraPawn>(PlayerPawn);
	if (not IsValid(CameraPawn))
	{
		return true;
	}

	const UCameraComponent* CameraComponent = CameraPawn->GetCameraComponent();
	if (not IsValid(CameraComponent))
	{
		return true;
	}

	return FVector::DistSquared(CameraComponent->GetComponentLocation(), WorldLocation)
		<= FMath::Square(SimulationBudget.MaxSimulationDistanceFromView);
}

void UGeometryDamageComponent::ValidateConfigurationWarnings() const
{
	if (SimulationBudget.ImpulseWindowSeconds > SimulationBudget.SimulateAfterImpactSeconds)
	{
		RTSFunctionLibrary::ReportWarning(
			"Geometry damage ImpulseWindowSeconds exceeds SimulateAfterImpactSeconds and will be clamped at runtime.");
	}

	if (ScalingSettings.FullResponseFraction <= ScalingSettings.DeadzoneFraction)
	{
		RTSFunctionLibrary::ReportWarning(
			"Geometry damage FullResponseFraction is not above DeadzoneFraction; response becomes a hard step.");
	}

	for (const FGeometryDamageThresholdStage& Stage : ThresholdStages)
	{
		ValidateThresholdFXWarnings(Stage);
	}
}

void UGeometryDamageComponent::ValidateThresholdFXWarnings(
	const FGeometryDamageThresholdStage& Stage) const
{
	if (IsValid(Stage.CrossingFX.ImpactSound) && not IsValid(Stage.CrossingFX.SoundAttenuation))
	{
		RTSFunctionLibrary::ReportWarning(
			"Geometry damage threshold sound has no attenuation and will not localise to the impact point.");
	}

	if (IsValid(Stage.CrossingFX.ImpactSound) && not IsValid(Stage.CrossingFX.SoundConcurrency))
	{
		RTSFunctionLibrary::ReportWarning(
			"Geometry damage threshold sound has no concurrency asset and may clip during focused fire.");
	}
}

bool UGeometryDamageComponent::GetIsValidGeometryCollection() const
{
	if (IsValid(M_GeometryCollection))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_GeometryCollection",
		"UGeometryDamageComponent::GetIsValidGeometryCollection",
		this);
	return false;
}

bool UGeometryDamageComponent::GetIsValidHealthComponent() const
{
	if (M_HealthComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_HealthComponent",
		"UGeometryDamageComponent::GetIsValidHealthComponent",
		this);
	return false;
}
