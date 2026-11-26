// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SecureBaseMission.h"

#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/GameUI/InfoTakeover/InfoTakeoverSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Missions/MissionProps/FuelCrane/MissionFuelCrane.h"
#include "RTS_Survival/Missions/MissionProps/FuelTruck/MissionFuelTruckSetup.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SecureBaseMissionPrivate
{
	constexpr float TargetPollSeconds = 0.5f;
	constexpr float TruckPollSeconds = 0.5f;
	constexpr float TourStartPollSeconds = 0.25f;
	constexpr float TourVoiceLinePollSeconds = 0.1f;
	constexpr float AssaultTriggerPollSeconds = 0.5f;
	constexpr int32 PlayerIndex = 1;
	// A spawned squad that dropped its march order this soon after spawning gets it again (spawn settling).
	constexpr float MarchOrderRetrySeconds = 10.f;
	// The truck counts as stopped below this speed.
	constexpr float ParkedMaxSpeed = 60.f;
	// The truck gets its park order again this often while it has not arrived (orders can fail while it settles).
	constexpr float TruckReorderSeconds = 6.f;
	const FName SetTextEventName(TEXT("SetText"));

	struct FMarkerSetTextParams
	{
		FText NewText;
	};

	FName MakeTargetIconId(const UObject* Mission, const int32 TargetIndex)
	{
		return FName(*FString::Printf(TEXT("SecureBase_%u_%d"), Mission->GetUniqueID(), TargetIndex));
	}
}

TArray<AActor*> USecureBaseMission::GetLivingTargets() const
{
	TArray<AActor*> Living;
	for (int32 Index = 0; Index < M_Targets.Num(); ++Index)
	{
		if (not GetIsTargetDead(Index))
		{
			Living.Add(M_Targets[Index].Get());
		}
	}
	return Living;
}

int32 USecureBaseMission::GetDestroyedTargetCount() const
{
	int32 Count = 0;
	for (const bool bCounted : M_TargetCounted)
	{
		Count += bCounted ? 1 : 0;
	}
	return Count;
}

FSecureBaseFuelDelivery::FSecureBaseFuelDelivery()
{
	TruckHullHitboxMesh = FMissionFuelTruckSetup::GetDefaultZilHullHitboxMesh();
	TruckWreckDeath = FMissionFuelTruckSetup::MakeDefaultZilWreckDeath();
}

TArray<AActor*> USecureBaseMission::GetBaseLifeActors() const
{
	TArray<AActor*> Actors;
	for (const TWeakObjectPtr<AActor>& Squad : M_SpawnedSquads)
	{
		if (Squad.IsValid())
		{
			Actors.Add(Squad.Get());
		}
	}
	if (M_FuelTruck.IsValid())
	{
		Actors.Add(M_FuelTruck.Get());
	}
	return Actors;
}

AMissionFuelCrane* USecureBaseMission::GetFuelCrane() const
{
	return M_FuelCrane.Get();
}

int32 USecureBaseMission::GetMarchingSquadCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Squad : M_SpawnedSquads)
	{
		ICommands* const Commands = Cast<ICommands>(Squad.Get());
		if (Commands != nullptr && Squad->IsUnitAlive() && Commands->GetActiveCommandID() == EAbilityID::IdMarch)
		{
			++Count;
		}
	}
	return Count;
}

void USecureBaseMission::OnMissionStartedNative()
{
	Super::OnMissionStartedNative();
	ClearRuntime();
	Start_CollectTargets();
	Start_RevealBase();
	Start_MarkTargets();
	RefreshObjective();
	Start_SpawnBaseLife();

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TargetPollTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PollTargets();
		}
	}), SecureBaseMissionPrivate::TargetPollSeconds, true);
	World->GetTimerManager().SetTimer(M_TourStartTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PollTourStart();
		}
	}), SecureBaseMissionPrivate::TourStartPollSeconds, true);
}

void USecureBaseMission::Start_CollectTargets()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* const Actor = *It;
		if (not IsValid(Actor) || not Actor->IsUnitAlive() || not GetIsInTargetArea(Actor->GetActorLocation()))
		{
			continue;
		}
		const bool bIsTargetClass = TargetClasses.ContainsByPredicate([Actor](const TSubclassOf<AActor>& TargetClass)
		{
			return TargetClass != nullptr && Actor->IsA(TargetClass);
		});
		if (bIsTargetClass)
		{
			M_Targets.Add(Actor);
		}
	}
	// Stable order (west to east) so the markers and the test see the same indices every run.
	M_Targets.Sort([](const TWeakObjectPtr<AActor>& A, const TWeakObjectPtr<AActor>& B)
	{
		return A->GetActorLocation().X < B->GetActorLocation().X;
	});
	M_TargetCounted.Init(false, M_Targets.Num());
	M_TargetMarkers.SetNum(M_Targets.Num());
	M_TargetIconIds.Init(NAME_None, M_Targets.Num());
	if (M_Targets.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + " found no living target in its area; it "
			"completes after its tour.");
	}
}

void USecureBaseMission::Start_RevealBase()
{
	for (const FVector& Centre : RevealCentres)
	{
		RevealAreaForSeconds(Centre, RevealRadius, RevealSeconds);
	}
}

void USecureBaseMission::Start_MarkTargets()
{
	UWorld* const World = GetWorld();
	for (int32 Index = 0; Index < M_Targets.Num(); ++Index)
	{
		AActor* const Target = M_Targets[Index].Get();
		if (not IsValid(Target))
		{
			continue;
		}
		const FName IconId = SecureBaseMissionPrivate::MakeTargetIconId(this, Index);
		M_TargetIconIds[Index] = AddCustomMiniMapIconAttachedToActor(IconId, TargetMiniMapIcon,
		                                                             Target->GetActorLocation(), Target,
		                                                             FRotator::ZeroRotator, true,
		                                                             FMinimapIconTextPayload());
		if (TargetMarkerClass == nullptr || World == nullptr)
		{
			continue;
		}
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(true, Origin, Extent);
		const FVector MarkerLocation(Origin.X, Origin.Y, Origin.Z + Extent.Z + TargetMarkerHeightAboveTarget);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* const Marker = World->SpawnActor<AActor>(TargetMarkerClass, MarkerLocation, FRotator::ZeroRotator,
		                                                 SpawnParameters);
		if (not IsValid(Marker))
		{
			continue;
		}
		M_TargetMarkers[Index] = Marker;
		if (TargetMarkerText.IsEmpty())
		{
			continue;
		}
		if (UFunction* const SetTextEvent = Marker->FindFunction(SecureBaseMissionPrivate::SetTextEventName))
		{
			SecureBaseMissionPrivate::FMarkerSetTextParams Params{TargetMarkerText};
			Marker->ProcessEvent(SetTextEvent, &Params);
		}
	}
}

void USecureBaseMission::Start_SpawnBaseLife()
{
	M_PendingLifeSpawns = 0;
	for (int32 Index = 0; Index < MarchingSquads.Num(); ++Index)
	{
		SpawnMarchingSquad(Index);
	}
	if (not FuelDelivery.bEnabled)
	{
		return;
	}
	SpawnFuelCrane();
	if (FuelDelivery.TruckClass.IsNull())
	{
		return;
	}
	if (FuelDelivery.TruckSpawnDelaySeconds <= 0.f || TourShots.IsEmpty())
	{
		++M_PendingLifeSpawns;
		SpawnFuelTruck();
	}
	// Otherwise StartTour spawns it TruckSpawnDelaySeconds after the tour starts, so it arrives on camera.
}

void USecureBaseMission::ScheduleDelayedFuelTruck()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not FuelDelivery.bEnabled || FuelDelivery.TruckClass.IsNull()
		|| FuelDelivery.TruckSpawnDelaySeconds <= 0.f)
	{
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TruckSpawnTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			++WeakThis->M_PendingLifeSpawns;
			WeakThis->SpawnFuelTruck();
		}
	}), FuelDelivery.TruckSpawnDelaySeconds, false);
}

void USecureBaseMission::SpawnMarchingSquad(const int32 SquadIndex)
{
	const FSecureBaseMarchingSquad& Squad = MarchingSquads[SquadIndex];
	ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner) || Squad.SquadClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + " cannot spawn marching squad "
			+ FString::FromInt(SquadIndex) + ": no async spawner or no squad class.");
		return;
	}
	++M_PendingLifeSpawns;
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	const bool bStarted = Spawner->AsyncSpawnClassAtLocation(
		Squad.SquadClass, FTrainingOption(), Squad.SpawnLocation, this, SquadIndex,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 ID)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnMarchingSquadSpawned(ID, SpawnedActor);
			}
		}, FRotator(0.f, Squad.SpawnYaw, 0.f));
	if (not bStarted)
	{
		// The callback may already have run with nullptr; count the spawn as reported only when it did not.
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + " could not start loading marching squad "
			+ FString::FromInt(SquadIndex) + ".");
	}
}

void USecureBaseMission::OnMarchingSquadSpawned(const int32 SquadIndex, AActor* SpawnedSquad)
{
	OnLifeSpawnReported();
	if (not IsValid(SpawnedSquad) || not MarchingSquads.IsValidIndex(SquadIndex))
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + ": marching squad "
			+ FString::FromInt(SquadIndex) + " failed to spawn.");
		return;
	}
	M_SpawnedSquads.Add(SpawnedSquad);
	ICommands* const Commands = Cast<ICommands>(SpawnedSquad);
	if (Commands == nullptr)
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission: spawned squad " + SpawnedSquad->GetName()
			+ " takes no commands; it cannot march.");
		return;
	}
	const FVector MarchTo = MarchingSquads[SquadIndex].MarchToLocation;
	Commands->MarchToLocation(MarchTo, true);

	// A squad whose soldiers are still settling can drop the order; give it again for a short while.
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const TWeakObjectPtr<AActor> WeakSquad(SpawnedSquad);
	const float RetryEnd = World->GetTimeSeconds() + SecureBaseMissionPrivate::MarchOrderRetrySeconds;
	const TSharedRef<FTimerHandle> RetryHandle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(*RetryHandle, FTimerDelegate::CreateWeakLambda(
		                                  this, [WeakThis, WeakSquad, MarchTo, RetryEnd, RetryHandle]()
		                                  {
			                                  UWorld* const TimerWorld = WeakThis.IsValid()
				                                                             ? WeakThis->GetWorld()
				                                                             : nullptr;
			                                  ICommands* const SquadCommands = Cast<ICommands>(WeakSquad.Get());
			                                  if (TimerWorld == nullptr || SquadCommands == nullptr
				                                  || not WeakSquad->IsUnitAlive()
				                                  || TimerWorld->GetTimeSeconds() > RetryEnd)
			                                  {
				                                  if (TimerWorld != nullptr)
				                                  {
					                                  TimerWorld->GetTimerManager().ClearTimer(*RetryHandle);
				                                  }
				                                  return;
			                                  }
			                                  const EAbilityID ActiveCommand = SquadCommands->GetActiveCommandID();
			                                  if (ActiveCommand == EAbilityID::IdNoAbility
				                                  || ActiveCommand == EAbilityID::IdIdle)
			                                  {
				                                  SquadCommands->MarchToLocation(MarchTo, true);
			                                  }
		                                  }), 1.f, true);
	M_MarchRetryTimers.Add(RetryHandle);
}

void USecureBaseMission::SpawnFuelCrane()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || FuelDelivery.CraneClass == nullptr)
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_FuelCrane = World->SpawnActor<AMissionFuelCrane>(FuelDelivery.CraneClass, FuelDelivery.CraneLocation,
	                                                   FRotator(0.f, FuelDelivery.CraneYaw, 0.f), SpawnParameters);
	if (not M_FuelCrane.IsValid())
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + " could not spawn the fuel crane.");
	}
}

void USecureBaseMission::SpawnFuelTruck()
{
	ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + " cannot spawn the fuel truck: no async "
			"spawner.");
		OnLifeSpawnReported();
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	Spawner->AsyncSpawnClassAtLocation(
		FuelDelivery.TruckClass, FTrainingOption(), FuelDelivery.TruckSpawnLocation, this, 0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnFuelTruckSpawned(SpawnedActor);
			}
		}, FRotator(0.f, FuelDelivery.TruckSpawnYaw, 0.f));
}

void USecureBaseMission::OnFuelTruckSpawned(AActor* SpawnedTruck)
{
	OnLifeSpawnReported();
	if (not IsValid(SpawnedTruck))
	{
		RTSFunctionLibrary::ReportError("SecureBaseMission " + GetName() + ": the fuel truck failed to spawn.");
		return;
	}
	M_FuelTruck = SpawnedTruck;
	SetupFuelTruckHitboxAndDeath(SpawnedTruck);
	if (ICommands* const Commands = Cast<ICommands>(SpawnedTruck))
	{
		Commands->MoveToLocation(FuelDelivery.ParkLocation, true, FRotator(0.f, FuelDelivery.ParkYaw, 0.f), true);
	}
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TruckPollTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PollFuelTruck();
		}
	}), SecureBaseMissionPrivate::TruckPollSeconds, true);
}

void USecureBaseMission::SetupFuelTruckHitboxAndDeath(AActor* Truck)
{
	FMissionFuelTruckSetup::ApplyHitboxAndWreckDeath(Truck, FuelDelivery.TruckHullHitboxMesh,
	                                                 FuelDelivery.TruckWreckDeath, M_PreloadedTruckDeathAssets,
	                                                 "SecureBaseMission " + GetName());
}

void USecureBaseMission::PollFuelTruck()
{
	UWorld* const World = GetWorld();
	AActor* const Truck = M_FuelTruck.Get();
	if (World == nullptr || not IsValid(Truck) || not Truck->IsUnitAlive())
	{
		if (World != nullptr)
		{
			World->GetTimerManager().ClearTimer(M_TruckPollTimer);
		}
		return;
	}
	const float DistanceToPark = FVector::Dist2D(Truck->GetActorLocation(), FuelDelivery.ParkLocation);
	ICommands* const Commands = Cast<ICommands>(Truck);
	const bool bIsMoving = Commands != nullptr && Commands->GetActiveCommandID() == EAbilityID::IdMove;
	const bool bHasStopped = Truck->GetVelocity().Size2D() < SecureBaseMissionPrivate::ParkedMaxSpeed;
	if (DistanceToPark <= FuelDelivery.ParkedRadius && not bIsMoving && bHasStopped)
	{
		World->GetTimerManager().ClearTimer(M_TruckPollTimer);
		bM_HasTruckParked = true;
		if (M_FuelCrane.IsValid())
		{
			M_FuelCrane->StartUnloading(Truck);
		}
		return;
	}
	// Re-issue the park order now and then when the truck stands idle short of the spot.
	static constexpr int32 PollsPerReorder = static_cast<int32>(
		SecureBaseMissionPrivate::TruckReorderSeconds / SecureBaseMissionPrivate::TruckPollSeconds);
	const int32 PollIndex = FMath::FloorToInt(World->GetTimeSeconds() / SecureBaseMissionPrivate::TruckPollSeconds);
	if (Commands != nullptr && not bIsMoving && bHasStopped && PollIndex % PollsPerReorder == 0)
	{
		Commands->MoveToLocation(FuelDelivery.ParkLocation, true, FRotator(0.f, FuelDelivery.ParkYaw, 0.f), true);
	}
}

void USecureBaseMission::OnLifeSpawnReported()
{
	M_PendingLifeSpawns = FMath::Max(0, M_PendingLifeSpawns - 1);
}

void USecureBaseMission::PollTourStart()
{
	if (bM_HasTourStarted)
	{
		return;
	}
	M_SecondsWaitedForBaseLife += SecureBaseMissionPrivate::TourStartPollSeconds;
	if (M_PendingLifeSpawns > 0 && M_SecondsWaitedForBaseLife < MaxSecondsWaitingForBaseLife)
	{
		return;
	}
	StartTour();
}

void USecureBaseMission::StartTour()
{
	bM_HasTourStarted = true;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TourStartTimer);
	}
	if (TourShots.IsEmpty())
	{
		// No tour: report it as finished next frame through the regular path.
		bM_HasTourFinished = true;
		TryCompleteMission();
		return;
	}
	ScheduleDelayedFuelTruck();
	if (PlayCameraTour(TourShots, TourSettings, 0))
	{
		ScheduleTourVoiceLine();
	}
}

void USecureBaseMission::ScheduleTourVoiceLine()
{
	UWorld* const World = GetWorld();
	if (TourVoiceLine == nullptr || bM_HasTourVoiceLinePlayed || World == nullptr)
	{
		return;
	}
	// The tour may still wait for a busy screen; its takeover stops portraits, so the line waits for the first shot.
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TourVoiceLineTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PollTourVoiceLineShot();
		}
	}), SecureBaseMissionPrivate::TourVoiceLinePollSeconds, true);
}

void USecureBaseMission::PollTourVoiceLineShot()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (not GetIsCameraTourPlaying())
	{
		// Ended without a shot: OnCameraTourFinishedNative plays the line.
		World->GetTimerManager().ClearTimer(M_TourVoiceLineTimer);
		return;
	}
	if (GetCameraTourShotIndex() == INDEX_NONE)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_TourVoiceLineTimer);
	if (TourVoiceLineDelaySeconds <= 0.f)
	{
		PlayTourVoiceLine();
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TourVoiceLineTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PlayTourVoiceLine();
		}
	}), TourVoiceLineDelaySeconds, false);
}

void USecureBaseMission::PlayTourVoiceLine()
{
	if (TourVoiceLine == nullptr || bM_HasTourVoiceLinePlayed)
	{
		return;
	}
	bM_HasTourVoiceLinePlayed = true;
	PlayPortrait(TourVoiceLinePortrait, TourVoiceLine);
}

void USecureBaseMission::OnCameraTourFinishedNative(const int32 ID, const bool bWasShown)
{
	Super::OnCameraTourFinishedNative(ID, bWasShown);
	bM_HasTourFinished = true;
	RefreshObjective();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TourVoiceLineTimer);
	}
	// A tour that never showed (or ended before the delay ran out) still gets its line, ahead of the objective line.
	PlayTourVoiceLine();
	if (ObjectiveVoiceLine != nullptr)
	{
		PlayPortrait(ObjectivePortrait, ObjectiveVoiceLine);
	}
	ArmAssaultTrigger();
	TryCompleteMission();
}

// ------------------------------------------------------------------
// Assault response
// ------------------------------------------------------------------

TArray<AEnemyAirDrop*> USecureBaseMission::GetResponseAirDrops() const
{
	TArray<AEnemyAirDrop*> AirDrops;
	for (const TWeakObjectPtr<AEnemyAirDrop>& AirDrop : M_ResponseAirDrops)
	{
		if (AirDrop.IsValid())
		{
			AirDrops.Add(AirDrop.Get());
		}
	}
	return AirDrops;
}

void USecureBaseMission::ArmAssaultTrigger()
{
	UWorld* const World = GetWorld();
	if (not AssaultResponse.bEnabled || bM_HasAssaultResponseStarted || World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_AssaultTriggerPollTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PollAssaultTrigger();
		}
	}), SecureBaseMissionPrivate::AssaultTriggerPollSeconds, true, 0.f);
}

void USecureBaseMission::PollAssaultTrigger()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || bM_HasAssaultResponseStarted)
	{
		return;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* const Pawn = *It;
		if (not IsValid(Pawn) || not GetIsInAssaultTrigger(Pawn->GetActorLocation()))
		{
			continue;
		}
		const URTSComponent* const RTSComponent = Pawn->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != SecureBaseMissionPrivate::PlayerIndex)
		{
			continue;
		}
		if (not Pawn->IsUnitAlive())
		{
			continue;
		}
		StartAssaultResponse();
		return;
	}
}

bool USecureBaseMission::GetIsInAssaultTrigger(const FVector& Location) const
{
	const FVector Delta = (Location - AssaultResponse.TriggerCentre).GetAbs();
	return Delta.X <= AssaultResponse.TriggerHalfExtent.X
		&& Delta.Y <= AssaultResponse.TriggerHalfExtent.Y
		&& Delta.Z <= AssaultResponse.TriggerHalfExtent.Z;
}

void USecureBaseMission::StartAssaultResponse()
{
	UWorld* const World = GetWorld();
	if (bM_HasAssaultResponseStarted || World == nullptr)
	{
		return;
	}
	bM_HasAssaultResponseStarted = true;
	World->GetTimerManager().ClearTimer(M_AssaultTriggerPollTimer);

	if (AssaultResponse.EnemyAlarmLine != nullptr)
	{
		PlayPortrait(AssaultResponse.EnemyAlarmPortrait, AssaultResponse.EnemyAlarmLine);
	}
	for (int32 DropIndex = 0; DropIndex < AssaultResponse.AirDrops.Num(); ++DropIndex)
	{
		SpawnResponseAirDrop(DropIndex);
	}

	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	World->GetTimerManager().SetTimer(M_AirSupportLineTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->PlayAirSupportLine();
		}
	}), FMath::Max(0.01f, AssaultResponse.AirSupportLineDelaySeconds), false);
	if (AssaultResponse.AirSupportAbility != EGlobalAbility::GA_None)
	{
		World->GetTimerManager().SetTimer(M_AirSupportStrikeTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->LaunchAirSupport();
			}
		}), FMath::Max(0.01f, AssaultResponse.AirSupportStrikeDelaySeconds), false);
	}
	BP_OnAssaultResponseStarted();
}

void USecureBaseMission::SpawnResponseAirDrop(const int32 DropIndex)
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not AssaultResponse.AirDrops.IsValidIndex(DropIndex))
	{
		return;
	}
	const FSecureBaseResponseAirDrop& DropSettings = AssaultResponse.AirDrops[DropIndex];
	const FTransform SpawnTransform(DropSettings.LandingPoint);
	AEnemyAirDrop* const AirDrop = World->SpawnActorDeferred<AEnemyAirDrop>(
		AEnemyAirDrop::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(AirDrop))
	{
		RTSFunctionLibrary::ReportError("Secure base mission " + GetName() + " could not spawn response air drop "
			+ FString::FromInt(DropIndex) + ".");
		return;
	}
	const FName AirDropId(*FString::Printf(TEXT("SecureBaseResponse_%u_%d"), GetUniqueID(), DropIndex));
	AirDrop->InitRuntimeAirDrop(AirDropId, DropSettings.Units, DropSettings.Delivery, DropSettings.DelaySeconds);
	AirDrop->FinishSpawning(SpawnTransform);
	M_ResponseAirDrops.Add(AirDrop);
}

void USecureBaseMission::PlayAirSupportLine()
{
	if (AssaultResponse.AirSupportLine != nullptr)
	{
		PlayPortrait(AssaultResponse.AirSupportPortrait, AssaultResponse.AirSupportLine);
	}
}

void USecureBaseMission::LaunchAirSupport()
{
	if (bM_HasAirSupportLaunched)
	{
		return;
	}
	const URTSGlobalAbilitySettings* const Settings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* const AbilitySet = IsValid(Settings)
		                                               ? Settings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	UGlobalAbilitiesManager* const Manager = IsValid(PlayerController)
		                                         ? PlayerController->GetGlobalAbilitiesManager()
		                                         : nullptr;
	UGlobalAbility* const Ability = IsValid(AbilitySet)
		                                ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
			                                AssaultResponse.AirSupportAbility).Get()
		                                : nullptr;
	if (not IsValid(Ability) || not IsValid(Manager))
	{
		RTSFunctionLibrary::ReportError("Secure base mission " + GetName() + ": no air support; ability "
			+ UEnum::GetValueAsString(AssaultResponse.AirSupportAbility) + " is not in the global ability data asset "
			"or the player has no global abilities manager.");
		return;
	}
	if (UGA_AircraftStrafing* const Strafing = Cast<UGA_AircraftStrafing>(Ability))
	{
		Strafing->SetMissionStrafeOverrides(AssaultResponse.AirSupportAircraftCount,
		                                    AssaultResponse.AirSupportStrafeSeconds,
		                                    AssaultResponse.bAirSupportStaysOnTarget);
	}
	bM_HasAirSupportLaunched = true;
	M_AirSupportAbility = Ability;
	Ability->InitGlobalAbility(SecureBaseMissionPrivate::PlayerIndex, Manager, PlayerController);
	Ability->ExecuteAbilityAtLocation(AssaultResponse.AirSupportTarget);
	ScheduleFollowUpsAfterAirSupport();
}

void USecureBaseMission::ScheduleFollowUpsAfterAirSupport()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<USecureBaseMission> WeakThis(this);
	if (AssaultResponse.IntelSquadSubtype != ESquadSubtype::Squad_None
		&& not AssaultResponse.IntelSquadLibraryEntry.IsNone())
	{
		// The squad's drop may still be in the air: look once a second for up to a minute.
		constexpr int32 IntelSquadMaxPolls = 60;
		constexpr float IntelSquadPollSeconds = 1.f;
		M_IntelSquadPollsLeft = IntelSquadMaxPolls;
		World->GetTimerManager().SetTimer(M_IntelSquadTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->TryShowIntelSquadTakeover();
			}
		}), IntelSquadPollSeconds, true, FMath::Max(0.01f, AssaultResponse.IntelSquadDelaySeconds));
	}
	if (AssaultResponse.GrantedAbility.Ability != EGlobalAbility::GA_None)
	{
		World->GetTimerManager().SetTimer(M_GrantedAbilityTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->GrantAbilityToPlayer();
			}
		}), FMath::Max(0.01f, AssaultResponse.GrantedAbilityDelaySeconds), false);
	}
}

void USecureBaseMission::TryShowIntelSquadTakeover()
{
	AActor* const Squad = FindLivingIntelSquad();
	M_IntelSquadPollsLeft -= 1;
	const bool bShouldStopPolling = Squad != nullptr || M_IntelSquadPollsLeft <= 0;
	if (bShouldStopPolling)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_IntelSquadTimer);
		}
	}
	if (Squad == nullptr)
	{
		if (bShouldStopPolling)
		{
			// The squad never stood on the map; the field manual still teaches crushing.
			QueueWallBreachManualAfterIntel();
		}
		return;
	}
	bM_HasShownIntelSquadTakeover = true;
	// Returns false while the player has unit intel turned off; the field manual shows regardless.
	ShowSquadInfoTakeover(Squad, AssaultResponse.IntelSquadLibraryEntry);
	// Queued behind the squad intel, so it opens as soon as that one closes.
	QueueWallBreachManualAfterIntel();
}

void USecureBaseMission::QueueWallBreachManualAfterIntel()
{
	if (not AssaultResponse.bShowWallBreachManualAfterIntelSquad || bM_HasQueuedWallBreachManual)
	{
		return;
	}
	UWorld* const World = GetWorld();
	UInfoTakeoverSubsystem* const InfoTakeovers = World != nullptr
		                                              ? World->GetSubsystem<UInfoTakeoverSubsystem>()
		                                              : nullptr;
	if (InfoTakeovers == nullptr)
	{
		RTSFunctionLibrary::ReportError("Secure base mission " + GetName()
			+ ": no info takeover subsystem; the crushing field manual cannot open.");
		return;
	}
	bM_HasQueuedWallBreachManual = true;
	InfoTakeovers->QueueWallBreachTakeover();
}

AActor* USecureBaseMission::FindLivingIntelSquad() const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ASquadController> It(World); It; ++It)
	{
		ASquadController* const Squad = *It;
		const URTSComponent* const RTSComponent = IsValid(Squad) ? Squad->FindComponentByClass<URTSComponent>() : nullptr;
		if (RTSComponent == nullptr || RTSComponent->GetOwningPlayer() == SecureBaseMissionPrivate::PlayerIndex)
		{
			continue;
		}
		if (RTSComponent->GetSubtypeAsSquadSubtype() == AssaultResponse.IntelSquadSubtype
			&& not Squad->GetSquadUnitsChecked().IsEmpty())
		{
			return Squad;
		}
	}
	return nullptr;
}

void USecureBaseMission::GrantAbilityToPlayer()
{
	if (bM_HasGrantedAbility)
	{
		return;
	}
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	UGlobalAbilitiesManager* const Manager = IsValid(PlayerController)
		                                         ? PlayerController->GetGlobalAbilitiesManager()
		                                         : nullptr;
	if (not IsValid(Manager) || Manager->GrantMissionGlobalAbility(AssaultResponse.GrantedAbility) == nullptr)
	{
		RTSFunctionLibrary::ReportError("Secure base mission " + GetName() + ": could not grant "
			+ UEnum::GetValueAsString(AssaultResponse.GrantedAbility.Ability) + " to the player.");
		return;
	}
	bM_HasGrantedAbility = true;
	if (AssaultResponse.GrantedAbilityLine != nullptr)
	{
		PlayPortrait(AssaultResponse.GrantedAbilityPortrait, AssaultResponse.GrantedAbilityLine);
	}
}

void USecureBaseMission::PollTargets()
{
	for (int32 Index = 0; Index < M_Targets.Num(); ++Index)
	{
		if (not M_TargetCounted[Index] && GetIsTargetDead(Index))
		{
			OnTargetDestroyed(Index);
		}
	}
}

void USecureBaseMission::OnTargetDestroyed(const int32 TargetIndex)
{
	M_TargetCounted[TargetIndex] = true;
	RemoveTargetMarker(TargetIndex);
	RefreshObjective();
	BP_OnSecureBaseTargetDestroyed(M_Targets[TargetIndex].Get(), GetDestroyedTargetCount(), M_Targets.Num());
	TryCompleteMission();
}

void USecureBaseMission::RemoveTargetMarker(const int32 TargetIndex)
{
	if (M_TargetMarkers.IsValidIndex(TargetIndex) && M_TargetMarkers[TargetIndex].IsValid())
	{
		M_TargetMarkers[TargetIndex]->Destroy();
	}
	if (M_TargetIconIds.IsValidIndex(TargetIndex) && M_TargetIconIds[TargetIndex] != NAME_None)
	{
		RemoveCustomMiniMapIcon(M_TargetIconIds[TargetIndex]);
		M_TargetIconIds[TargetIndex] = NAME_None;
	}
}

void USecureBaseMission::RefreshObjective()
{
	if (ObjectiveTitle.IsEmpty() && ObjectiveDescription.IsEmpty())
	{
		return;
	}
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Destroyed"), GetDestroyedTargetCount());
	Arguments.Add(TEXT("Total"), M_Targets.Num());
	SetMissionObjectiveText(
		ObjectiveTitle, FText::Format(ObjectiveDescription, Arguments));
}

void USecureBaseMission::TryCompleteMission()
{
	if (bM_HasCompletedTargets || not bM_HasTourFinished || GetIsMissionComplete())
	{
		return;
	}
	if (GetDestroyedTargetCount() < M_Targets.Num())
	{
		return;
	}
	bM_HasCompletedTargets = true;
	OnMissionComplete();
}

void USecureBaseMission::OnMissionComplete()
{
	ClearRuntime();
	Super::OnMissionComplete();
}

void USecureBaseMission::OnMissionFailed()
{
	ClearRuntime();
	Super::OnMissionFailed();
}

void USecureBaseMission::OnCleanUpMission()
{
	ClearRuntime();
	if (UWorld* const World = GetWorld())
	{
		// The response (lines, strike) outlives a completed mission; only a map unload stops it.
		World->GetTimerManager().ClearTimer(M_AirSupportLineTimer);
		World->GetTimerManager().ClearTimer(M_AirSupportStrikeTimer);
		World->GetTimerManager().ClearTimer(M_IntelSquadTimer);
		World->GetTimerManager().ClearTimer(M_GrantedAbilityTimer);
	}
	Super::OnCleanUpMission();
}

void USecureBaseMission::ClearRuntime()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TargetPollTimer);
		World->GetTimerManager().ClearTimer(M_TruckPollTimer);
		World->GetTimerManager().ClearTimer(M_TruckSpawnTimer);
		World->GetTimerManager().ClearTimer(M_TourStartTimer);
		World->GetTimerManager().ClearTimer(M_TourVoiceLineTimer);
		World->GetTimerManager().ClearTimer(M_AssaultTriggerPollTimer);
		for (const TSharedRef<FTimerHandle>& RetryTimer : M_MarchRetryTimers)
		{
			World->GetTimerManager().ClearTimer(*RetryTimer);
		}
	}
	M_MarchRetryTimers.Empty();
	for (int32 Index = 0; Index < M_TargetMarkers.Num(); ++Index)
	{
		RemoveTargetMarker(Index);
	}
	// The base life (squads, truck, crane) stays: it is part of the map now and the player fights it.
}

bool USecureBaseMission::GetIsTargetDead(const int32 TargetIndex) const
{
	if (not M_Targets.IsValidIndex(TargetIndex))
	{
		return true;
	}
	AActor* const Target = M_Targets[TargetIndex].Get();
	return not IsValid(Target) || not Target->IsUnitAlive();
}

bool USecureBaseMission::GetIsInTargetArea(const FVector& Location) const
{
	return FMath::Abs(Location.X - TargetAreaCentre.X) <= TargetAreaHalfExtent.X
		&& FMath::Abs(Location.Y - TargetAreaCentre.Y) <= TargetAreaHalfExtent.Y;
}
