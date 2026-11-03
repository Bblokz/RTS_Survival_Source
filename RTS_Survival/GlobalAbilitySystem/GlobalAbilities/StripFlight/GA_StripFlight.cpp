// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_StripFlight.h"

#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace StripFlightConstants
{
	constexpr float ProgressIntervalSeconds = 0.05f;
}

void UGA_StripFlight::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	const FVector Direction = GetDirectionForPointExecution(TargetLocation);
	const FVector HalfLine = Direction * M_AimSettings.AreaAim.MaxStripLength * 0.5f;
	ExecuteAbilityAlongLine(TargetLocation - HalfLine, TargetLocation + HalfLine);
}

void UGA_StripFlight::ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd)
{
	ResetAbilityEndedBroadcast();
	const FVector Line = FVector(LineEnd.X - LineStart.X, LineEnd.Y - LineStart.Y, 0.0f);
	if (not GetIsValidGlobalAbilityManager() || Line.IsNearlyZero() || M_StripFlightSettings.AircraftClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_StripFlight has no manager, strip or aircraft class."));
		BroadcastAbilityEnded();
		return;
	}

	FStripFlightRun NewRun;
	NewRun.RunId = M_NextRunId++;
	NewRun.LineStart = LineStart;
	NewRun.LineEnd = LineEnd;
	NewRun.Direction = Line.GetSafeNormal();
	NewRun.StripLength = Line.Size();
	const int32 RunId = NewRun.RunId;
	M_Runs.Add(NewRun);
	OnStripFlightStarted(NewRun);

	TArray<FSoftObjectPath> AssetsToLoad;
	AssetsToLoad.Add(M_StripFlightSettings.AircraftClass.ToSoftObjectPath());
	GetAssetsToLoadBeforeFlight(AssetsToLoad);
	TWeakObjectPtr<UGA_StripFlight> WeakThis(this);
	M_LoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateLambda([WeakThis, RunId]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded(RunId);
			}
		})));
	Super::ExecuteAbilityAtLocation((LineStart + LineEnd) * 0.5f);
}

void UGA_StripFlight::BeginDestroy()
{
	ClearProgressTimer();
	M_LoadHandles.Reset();
	M_Runs.Reset();
	Super::BeginDestroy();
}

FVector UGA_StripFlight::GetDirectionForPointExecution(const FVector& TargetLocation) const
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return FVector::ForwardVector;
	}
	const FVector SpawnLocation = GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, TargetLocation);
	const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? FVector::ForwardVector : Direction;
}

FStripFlightRun* UGA_StripFlight::FindRun(const int32 RunId)
{
	return M_Runs.FindByPredicate([RunId](const FStripFlightRun& Run)
	{
		return Run.RunId == RunId;
	});
}

void UGA_StripFlight::OnAssetsLoaded(const int32 RunId)
{
	SpawnAircraftForRun(RunId);
}

void UGA_StripFlight::SpawnAircraftForRun(const int32 RunId)
{
	const FStripFlightRun* Run = FindRun(RunId);
	UClass* AircraftClass = M_StripFlightSettings.AircraftClass.Get();
	UWorld* World = GetWorld();
	if (not Run || not IsValid(AircraftClass) || not IsValid(World) || not GetIsValidGlobalAbilityManager())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_StripFlight failed to load a valid aircraft class."));
		EndRun(RunId);
		return;
	}

	const FVector SpawnLocation = GetGlobalAbilityManager()->GetStraightFlightSpawnLocation(
		Run->LineStart, Run->Direction, M_StripFlightSettings.MaxApproachDistance);
	const FTransform SpawnTransform(Run->Direction.Rotation(), SpawnLocation);
	AAircraftMaster* Aircraft = Cast<AAircraftMaster>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, AircraftClass, SpawnTransform, static_cast<uint8>(GetOwningPlayer())));
	if (not IsValid(Aircraft))
	{
		EndRun(RunId);
		return;
	}

	FStripFlightRun* RunToUpdate = FindRun(RunId);
	if (not RunToUpdate)
	{
		return;
	}
	RunToUpdate->Aircraft = Aircraft;
	RunToUpdate->bM_HasSpawnedAircraft = true;
	Aircraft->SetUnitSelectable(false);
	QueueFlightOrderForNextFrame(Aircraft, RunId);
	StartProgressTimer();
}

void UGA_StripFlight::QueueFlightOrderForNextFrame(AAircraftMaster* Aircraft, const int32 RunId)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<UGA_StripFlight> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(Aircraft);
	// Same as the other aircraft abilities: the aircraft finishes its BeginPlay setup before it gets an order.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakAircraft, RunId]()
	{
		if (not WeakThis.IsValid() || not WeakAircraft.IsValid())
		{
			return;
		}
		const FStripFlightRun* Run = WeakThis->FindRun(RunId);
		if (not Run)
		{
			return;
		}
		const float FlightHeight = WeakAircraft->GetActorLocation().Z;
		const FVector PassOverLocation(Run->LineStart.X, Run->LineStart.Y, FlightHeight);
		const FVector EndOnLine = Run->LineEnd + Run->Direction * WeakThis->M_StripFlightSettings.ExitDistance;
		WeakAircraft->OrderStraightFlightRun(PassOverLocation, FVector(EndOnLine.X, EndOnLine.Y, FlightHeight));
	}));
}

void UGA_StripFlight::StartProgressTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_ProgressTimerHandle))
	{
		return;
	}
	TWeakObjectPtr<UGA_StripFlight> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_ProgressTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnProgressTimer();
			}
		}),
		StripFlightConstants::ProgressIntervalSeconds,
		true);
}

void UGA_StripFlight::OnProgressTimer()
{
	// A copy: progress callbacks spawn actors, and ending a run changes the list.
	TArray<int32> RunIds;
	for (const FStripFlightRun& Run : M_Runs)
	{
		RunIds.Add(Run.RunId);
	}
	for (const int32 RunId : RunIds)
	{
		UpdateRunProgress(RunId);
	}
}

void UGA_StripFlight::UpdateRunProgress(const int32 RunId)
{
	FStripFlightRun* Run = FindRun(RunId);
	if (not Run || not Run->bM_HasSpawnedAircraft)
	{
		return;
	}
	AAircraftMaster* Aircraft = Run->Aircraft.Get();
	if (not IsValid(Aircraft) || not Aircraft->IsUnitAlive())
	{
		EndRun(RunId);
		return;
	}

	const float DistanceOnLine = FVector::DotProduct(Aircraft->GetActorLocation() - Run->LineStart, Run->Direction);
	const float NewDistance = FMath::Min(DistanceOnLine, Run->StripLength);
	if (NewDistance < 0.0f || NewDistance <= Run->HandledDistance)
	{
		return;
	}
	const float PreviousDistance = Run->HandledDistance;
	Run->HandledDistance = NewDistance;
	const FStripFlightRun RunCopy = *Run;
	OnStripFlightProgress(RunCopy, PreviousDistance, NewDistance);
}

void UGA_StripFlight::EndRun(const int32 RunId)
{
	const int32 RunIndex = M_Runs.IndexOfByPredicate([RunId](const FStripFlightRun& Run)
	{
		return Run.RunId == RunId;
	});
	if (M_Runs.IsValidIndex(RunIndex))
	{
		const FStripFlightRun EndedRun = M_Runs[RunIndex];
		M_Runs.RemoveAt(RunIndex);
		OnStripFlightEnded(EndedRun);
	}
	if (M_Runs.IsEmpty())
	{
		ClearProgressTimer();
		BroadcastAbilityEnded();
	}
}

void UGA_StripFlight::ClearProgressTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ProgressTimerHandle);
	}
}
