// Copyright (C) Bas Blokzijl - All rights reserved.


#include "CaptureActor.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"

ACaptureActor::ACaptureActor(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_CurrentCapturingPlayer = INDEX_NONE;
}

void ACaptureActor::BeginPlay()
{
	Super::BeginPlay();

	M_CurrentCapturingPlayer = INDEX_NONE;
}

void ACaptureActor::SetupCaptureLocations(UMeshComponent* MeshComp, const FName SocketNameContaining)
{
	M_CaptureLocationsRelativeToWorldLocation.Empty();

	if (not IsValid(MeshComp))
	{
		return;
	}

	TArray<FName> SocketNames = MeshComp->GetAllSocketNames();

	if (SocketNames.Num() == 0)
	{
		return;
	}

	const FString FilterString = SocketNameContaining.ToString();
	const bool bHasFilter = !FilterString.IsEmpty();

	const FVector ActorLocation = GetActorLocation();

	for (const FName& SocketName : SocketNames)
	{
		if (bHasFilter)
		{
			const FString SocketString = SocketName.ToString();
			if (!SocketString.Contains(FilterString))
			{
				continue;
			}
		}

		const FVector SocketWorldLocation = MeshComp->GetSocketLocation(SocketName);
		const FVector RelativeLocation = SocketWorldLocation - ActorLocation;
		M_CaptureLocationsRelativeToWorldLocation.Add(RelativeLocation);
	}
}

int32 ACaptureActor::GetCaptureUnitAmountNeeded() const
{
	return CaptureSettings.AmountCaptureUnitsNeeded;
}

void ACaptureActor::OnCaptureByPlayer(const int32 Player)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_CaptureTimerHandle);

	M_CurrentCapturingPlayer = Player;

	const float CaptureDurationSeconds = CaptureSettings.CaptureDuration;
	if (CaptureDurationSeconds <= 0.f)
	{
		OnCaptureProgressComplete();
		return;
	}

	StartCaptureProgressBarForCaptureActor(*this);

	TWeakObjectPtr<ACaptureActor> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_CaptureTimerHandle,
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnCaptureProgressComplete();
		},
		CaptureDurationSeconds,
		false);
}

FVector ACaptureActor::GetCaptureLocationClosestTo(const FVector& FromLocation)
{
	if (M_CaptureLocationsRelativeToWorldLocation.Num() == 0)
	{
		return GetActorLocation();
	}

	const FVector ActorLocation = GetActorLocation();

	FVector BestWorldLocation = ActorLocation + M_CaptureLocationsRelativeToWorldLocation[0];
	float BestDistanceSq = FVector::DistSquared(FromLocation, BestWorldLocation);

	const int32 NumLocations = M_CaptureLocationsRelativeToWorldLocation.Num();
	for (int32 Index = 1; Index < NumLocations; ++Index)
	{
		const FVector CandidateWorldLocation = ActorLocation + M_CaptureLocationsRelativeToWorldLocation[Index];
		const float CandidateDistanceSq = FVector::DistSquared(FromLocation, CandidateWorldLocation);
		if (CandidateDistanceSq >= BestDistanceSq)
		{
			continue;
		}

		BestDistanceSq = CandidateDistanceSq;
		BestWorldLocation = CandidateWorldLocation;
	}

	return BestWorldLocation;
}

void ACaptureActor::OnCaptureProgressComplete()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_CaptureTimerHandle);
	}

	const int32 CompletedPlayer = M_CurrentCapturingPlayer;
	M_CurrentCapturingPlayer = INDEX_NONE;

	if (CompletedPlayer == INDEX_NONE)
	{
		return;
	}

	BP_OnCapturedByPlayer(CompletedPlayer);
}

void ACaptureActor::StartCaptureProgressBarForCaptureActor(ACaptureActor& CaptureActor) const
{
	const float CaptureDurationSeconds = CaptureActor.CaptureSettings.CaptureDuration;
	if (CaptureDurationSeconds <= 0.f)
	{
		return;
	}

	UWorld* World = CaptureActor.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
		World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();
	if (ProgressSubsystem == nullptr)
	{
		return;
	}

	USceneComponent* AnchorComponent = CaptureActor.GetRootComponent();
	if (!IsValid(AnchorComponent))
	{
		return;
	}

	const FString DescriptionText = CaptureActor.CaptureProgressText.ToString();
	const FVector AttachOffset = FVector::ZeroVector;
	constexpr float RatioStart = 0.f;
	constexpr bool bUsePercentageText = true;
	constexpr bool bUseDescriptionText = true;
	constexpr float ScaleMlt = 1.f;
	const ERTSProgressBarType BarType = ERTSProgressBarType{};

	ProgressSubsystem->ActivateTimedProgressBarAnchored(
		AnchorComponent,
		AttachOffset,
		RatioStart,
		CaptureDurationSeconds,
		bUsePercentageText,
		BarType,
		bUseDescriptionText,
		DescriptionText,
		ScaleMlt);
}
