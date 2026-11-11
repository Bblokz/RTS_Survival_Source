// Copyright Bas Blokzijl - All rights reserved.
#include "NomadicDeployAnimationComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NomadicDeployAnimationData.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/ImpactVfxAuthoring/ImpactVfxSpriteScale.h"

namespace NomadicDeployAnimationConstants
{
	// The VAT bakes hold 181 frames; 30 Hz progress updates are smooth at every deploy duration in use (12-30 s).
	constexpr float TickIntervalSeconds = 1.f / 30.f;
	constexpr float MinDurationSeconds = 0.1f;
}

UNomadicDeployAnimationComponent::UNomadicDeployAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = NomadicDeployAnimationConstants::TickIntervalSeconds;
}

void UNomadicDeployAnimationComponent::InitDeployAnimation(UStaticMeshComponent* BuildingMeshComponent,
                                                           UStaticMeshComponent* DeployMeshComponent,
                                                           UNomadicDeployAnimationData* DeployAnimationData)
{
	M_BuildingMeshComponent = BuildingMeshComponent;
	M_DeployMeshComponent = DeployMeshComponent;
	M_DeployAnimationData = DeployAnimationData;
	if (not GetIsValidDeployMeshComponent() || not IsValid(BuildingMeshComponent))
	{
		return;
	}
	DeployMeshComponent->AttachToComponent(BuildingMeshComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
	DeployMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeployMeshComponent->SetCanEverAffectNavigation(false);
	DeployMeshComponent->SetGenerateOverlapEvents(false);
	DeployMeshComponent->SetVisibility(false);
	if (IsValid(DeployAnimationData))
	{
		DeployMeshComponent->SetStaticMesh(DeployAnimationData->DeployMesh);
		DeployMeshComponent->SetBoundsScale(DeployAnimationData->DeployMeshBoundsScale);
	}
}

bool UNomadicDeployAnimationComponent::GetHasDeployAnimation() const
{
	return IsValid(M_DeployAnimationData) && IsValid(M_DeployAnimationData->DeployMesh) && M_DeployMeshComponent.
		IsValid();
}

void UNomadicDeployAnimationComponent::PlayDeploy(const float DurationSeconds,
                                                  const FOnNomadicDeployAnimationFinished& OnFinished)
{
	constexpr float PackedTruckProgress = 0.f;
	constexpr float DeployDirection = 1.f;
	StartPlaying(PackedTruckProgress, DeployDirection, DurationSeconds, OnFinished);
}

void UNomadicDeployAnimationComponent::PlayPackUp(const float DurationSeconds,
                                                  const FOnNomadicDeployAnimationFinished& OnFinished)
{
	constexpr float DeployedBuildingProgress = 1.f;
	constexpr float PackUpDirection = -1.f;
	StartPlaying(DeployedBuildingProgress, PackUpDirection, DurationSeconds, OnFinished);
}

void UNomadicDeployAnimationComponent::PlayFromProgress(const float StartProgress, const bool bTowardBuilt,
                                                        const float FullRangeDurationSeconds,
                                                        const FOnNomadicDeployAnimationFinished& OnFinished)
{
	const float Direction = bTowardBuilt ? 1.f : -1.f;
	StartPlaying(FMath::Clamp(StartProgress, 0.f, 1.f), Direction, FullRangeDurationSeconds, OnFinished);
}

void UNomadicDeployAnimationComponent::StopAndHide()
{
	M_Direction = 0.f;
	M_OnFinished.Unbind();
	SetComponentTickEnabled(false);
	if (M_DeployMeshComponent.IsValid())
	{
		M_DeployMeshComponent->SetVisibility(false);
	}
}

void UNomadicDeployAnimationComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (M_Direction == 0.f)
	{
		SetComponentTickEnabled(false);
		return;
	}
	const float PreviousProgress = M_Progress;
	M_Progress = FMath::Clamp(M_Progress + M_Direction * DeltaTime / M_DurationSeconds, 0.f, 1.f);
	WriteProgress(M_Progress);
	FireCrossedBursts(PreviousProgress, M_Progress);
	const bool bReachedEnd = M_Direction > 0.f ? M_Progress >= 1.f : M_Progress <= 0.f;
	if (bReachedEnd)
	{
		FinishPlaying();
	}
}

void UNomadicDeployAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	M_OnFinished.Unbind();
	OnPlayConstructionSound.Unbind();
	Super::EndPlay(EndPlayReason);
}

bool UNomadicDeployAnimationComponent::GetIsValidDeployMeshComponent() const
{
	if (M_DeployMeshComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DeployMeshComponent",
	                                                             "GetIsValidDeployMeshComponent", GetOwner());
	return false;
}

void UNomadicDeployAnimationComponent::StartPlaying(const float StartProgress, const float Direction,
                                                    const float DurationSeconds,
                                                    const FOnNomadicDeployAnimationFinished& OnFinished)
{
	M_OnFinished = OnFinished;
	M_DurationSeconds = FMath::Max(DurationSeconds, NomadicDeployAnimationConstants::MinDurationSeconds);
	M_Progress = StartProgress;
	M_Direction = Direction;
	if (not GetHasDeployAnimation())
	{
		// No baked deploy for this building: the owner still gets its callback after the same time.
		SetComponentTickEnabled(true);
		return;
	}
	CopyBuildingPrimitiveData();
	WriteProgress(StartProgress);
	M_DeployMeshComponent->SetVisibility(true);
	FireBurstsAtStartProgress(StartProgress);
	SetComponentTickEnabled(true);
}

void UNomadicDeployAnimationComponent::FireBurstsAtStartProgress(const float StartProgress)
{
	// Crossing alone never reaches a burst placed exactly on the start value (0 on deploy, 1 on pack-up).
	constexpr float StartProgressTolerance = 0.0001f;
	if (not IsValid(M_DeployAnimationData))
	{
		return;
	}
	for (const FNomadicDeployEffectBurst& Burst : M_DeployAnimationData->EffectBursts)
	{
		if (FMath::IsNearlyEqual(Burst.Progress, StartProgress, StartProgressTolerance))
		{
			SpawnBurst(Burst);
		}
	}
}

void UNomadicDeployAnimationComponent::CopyBuildingPrimitiveData() const
{
	if (not M_BuildingMeshComponent.IsValid() || not M_DeployMeshComponent.IsValid())
	{
		return;
	}
	const TArray<float>& BuildingData = M_BuildingMeshComponent->GetCustomPrimitiveData().Data;
	for (int32 DataIndex = 0; DataIndex < BuildingData.Num(); ++DataIndex)
	{
		if (DataIndex == NomadicDeployPrimitiveData::ProgressIndex)
		{
			continue;
		}
		M_DeployMeshComponent->SetCustomPrimitiveDataFloat(DataIndex, BuildingData[DataIndex]);
	}
}

void UNomadicDeployAnimationComponent::WriteProgress(const float Progress) const
{
	if (not M_DeployMeshComponent.IsValid())
	{
		return;
	}
	M_DeployMeshComponent->SetCustomPrimitiveDataFloat(NomadicDeployPrimitiveData::ProgressIndex, Progress);
}

void UNomadicDeployAnimationComponent::FireCrossedBursts(const float FromProgress, const float ToProgress)
{
	if (not IsValid(M_DeployAnimationData) || FromProgress == ToProgress)
	{
		return;
	}
	const float Low = FMath::Min(FromProgress, ToProgress);
	const float High = FMath::Max(FromProgress, ToProgress);
	const bool bRising = ToProgress > FromProgress;
	for (const FNomadicDeployEffectBurst& Burst : M_DeployAnimationData->EffectBursts)
	{
		// Rising fires on (Low, High], falling on [Low, High); together with FireBurstsAtStartProgress a burst at
		// exactly 0 or 1 plays in both directions (at the swap from and to the truck / building).
		const bool bCrossed = bRising
			                      ? Burst.Progress > Low && Burst.Progress <= High
			                      : Burst.Progress >= Low && Burst.Progress < High;
		if (bCrossed)
		{
			SpawnBurst(Burst);
		}
	}
}

void UNomadicDeployAnimationComponent::SpawnBurst(const FNomadicDeployEffectBurst& Burst)
{
	if (Burst.bPlayConstructionSound)
	{
		OnPlayConstructionSound.ExecuteIfBound();
	}
	if (not IsValid(Burst.System) || not M_BuildingMeshComponent.IsValid())
	{
		return;
	}
	const FTransform& BuildingTransform = M_BuildingMeshComponent->GetComponentTransform();
	const FVector WorldLocation = BuildingTransform.TransformPosition(Burst.LocalLocation);
	UNiagaraComponent* const Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, Burst.System, WorldLocation, BuildingTransform.Rotator(), FVector(Burst.Scale), true, true,
		ENCPoolMethod::AutoRelease, true);
	if (not IsValid(Spawned))
	{
		return;
	}
	ImpactVfxSpriteScale::ApplyImpactScale(*Spawned, FVector(Burst.Scale));
}

void UNomadicDeployAnimationComponent::FinishPlaying()
{
	M_Direction = 0.f;
	SetComponentTickEnabled(false);
	if (M_DeployMeshComponent.IsValid())
	{
		M_DeployMeshComponent->SetVisibility(false);
	}
	// Copy first: the callback may start the next animation and rebind M_OnFinished.
	const FOnNomadicDeployAnimationFinished FinishedCallback = M_OnFinished;
	M_OnFinished.Unbind();
	FinishedCallback.ExecuteIfBound();
}
