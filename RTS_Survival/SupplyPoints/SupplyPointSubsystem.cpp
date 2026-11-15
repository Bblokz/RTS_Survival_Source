// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SupplyPointSubsystem.h"

#include "Algo/RandomShuffle.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/SupplyPoints/SupplyPointDeveloperSettings.h"
#include "RTS_Survival/SupplyPoints/SupplyTruckComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace SupplyPointSubsystemPrivate
{
	// The base has no HQ to send a truck from (destroyed, packed up, not delivered yet): look again this often.
	constexpr float NoBaseRetryInterval = 5.f;
	// A packed-up or socket-less HQ has no spawn socket: the truck starts this far in front of it.
	constexpr float FallbackSpawnDistanceFromHq = 1500.f;
	// Lift the spawn off the navmesh point so the truck does not start inside the ground.
	constexpr float SpawnHeightOffset = 60.f;
	const TArray<ERTSResourceType> CargoResourceTypes = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};
}

int32 FSupplyAnnouncementDeck::DrawNextLineIndex(const int32 LineCount)
{
	if (LineCount <= 0)
	{
		return INDEX_NONE;
	}
	const bool bDeckIsStale = M_RemainingLineIndices.ContainsByPredicate([LineCount](const int32 LineIndex)
	{
		return LineIndex >= LineCount;
	});
	if (M_RemainingLineIndices.IsEmpty() or bDeckIsStale)
	{
		M_RemainingLineIndices.Reset();
		for (int32 LineIndex = 0; LineIndex < LineCount; ++LineIndex)
		{
			M_RemainingLineIndices.Add(LineIndex);
		}
		Algo::RandomShuffle(M_RemainingLineIndices);
		// The deck is dealt from the back: never open a fresh deck with the line that was just heard.
		if (LineCount > 1 and M_RemainingLineIndices.Last() == M_LastPlayedLineIndex)
		{
			M_RemainingLineIndices.Swap(0, M_RemainingLineIndices.Num() - 1);
		}
	}
	M_LastPlayedLineIndex = M_RemainingLineIndices.Pop();
	return M_LastPlayedLineIndex;
}

USupplyPointSubsystem* USupplyPointSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<USupplyPointSubsystem>() : nullptr;
}

void USupplyPointSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		for (FSupplyPointRun& Run : M_SupplyPointRuns)
		{
			World->GetTimerManager().ClearTimer(Run.DispatchTimerHandle);
		}
	}
	M_SupplyPointRuns.Reset();
	Super::Deinitialize();
}

void USupplyPointSubsystem::RegisterSupplyPoint(ASupplyPoint* SupplyPoint)
{
	if (not IsValid(SupplyPoint) or FindRun(SupplyPoint) != nullptr)
	{
		return;
	}
	FSupplyPointRun& Run = M_SupplyPointRuns.AddDefaulted_GetRef();
	Run.SupplyPoint = SupplyPoint;
	SupplyPoint->OnSupplyPointOwnerChanged.AddUObject(this, &USupplyPointSubsystem::OnSupplyPointOwnerChanged);
}

void USupplyPointSubsystem::UnregisterSupplyPoint(ASupplyPoint* SupplyPoint)
{
	if (SupplyPoint == nullptr)
	{
		return;
	}
	SupplyPoint->OnSupplyPointOwnerChanged.RemoveAll(this);
	const int32 RunIndex = M_SupplyPointRuns.IndexOfByPredicate([SupplyPoint](const FSupplyPointRun& Run)
	{
		return Run.SupplyPoint.Get() == SupplyPoint;
	});
	if (RunIndex == INDEX_NONE)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SupplyPointRuns[RunIndex].DispatchTimerHandle);
	}
	// A truck on the road notices its point is gone and drives home by itself.
	M_SupplyPointRuns.RemoveAt(RunIndex);
}

FSupplyPointRun* USupplyPointSubsystem::FindRun(const ASupplyPoint* SupplyPoint)
{
	return M_SupplyPointRuns.FindByPredicate([SupplyPoint](const FSupplyPointRun& Run)
	{
		return Run.SupplyPoint.Get() == SupplyPoint;
	});
}

USupplyTruckComponent* USupplyPointSubsystem::GetSupplyTruckOfPoint(const ASupplyPoint* SupplyPoint) const
{
	const FSupplyPointRun* Run = M_SupplyPointRuns.FindByPredicate([SupplyPoint](const FSupplyPointRun& Candidate)
	{
		return Candidate.SupplyPoint.Get() == SupplyPoint;
	});
	return Run != nullptr ? Run->SupplyTruck.Get() : nullptr;
}

void USupplyPointSubsystem::OnSupplyPointOwnerChanged(ASupplyPoint* SupplyPoint, const uint8 PreviousOwner)
{
	FSupplyPointRun* Run = FindRun(SupplyPoint);
	const UWorld* World = GetWorld();
	if (Run == nullptr or World == nullptr)
	{
		return;
	}
	// The previous owner's truck and pending dispatch are off.
	World->GetTimerManager().ClearTimer(Run->DispatchTimerHandle);
	if (Run->SupplyTruck.IsValid())
	{
		Run->SupplyTruck->ReturnToBase();
		Run->SupplyTruck.Reset();
	}
	const uint8 NewOwner = SupplyPoint->GetOwningPlayer();
	if (NewOwner == SupplyPointConstants::NeutralOwner)
	{
		return;
	}
	FTransform BaseTransform;
	if (NewOwner != SupplyPointConstants::PlayerOwner and not GetSupplyBaseTransform(NewOwner, BaseTransform))
	{
		// AI holders without a truck base (all of them, for now) send no trucks.
		return;
	}
	PlaySupplyAnnouncement(ESupplyAnnouncement::SupplyPointCaptured, NewOwner);
	const USupplyPointDeveloperSettings* Settings = GetDefault<USupplyPointDeveloperSettings>();
	ScheduleTruckDispatch(SupplyPoint, Settings->DispatchDelayAfterCapture);
}

void USupplyPointSubsystem::ScheduleTruckDispatch(ASupplyPoint* SupplyPoint, const float Delay)
{
	FSupplyPointRun* Run = FindRun(SupplyPoint);
	const UWorld* World = GetWorld();
	if (Run == nullptr or World == nullptr)
	{
		return;
	}
	if (Delay <= 0.f)
	{
		DispatchSupplyTruck(SupplyPoint);
		return;
	}
	TWeakObjectPtr<USupplyPointSubsystem> WeakThis(this);
	TWeakObjectPtr<ASupplyPoint> WeakSupplyPoint(SupplyPoint);
	World->GetTimerManager().SetTimer(Run->DispatchTimerHandle, FTimerDelegate::CreateLambda(
		                                  [WeakThis, WeakSupplyPoint]()
		                                  {
			                                  if (WeakThis.IsValid() and WeakSupplyPoint.IsValid())
			                                  {
				                                  WeakThis->DispatchSupplyTruck(WeakSupplyPoint.Get());
			                                  }
		                                  }), Delay, false);
}

void USupplyPointSubsystem::DispatchSupplyTruck(ASupplyPoint* SupplyPoint)
{
	FSupplyPointRun* Run = FindRun(SupplyPoint);
	if (Run == nullptr or Run->SupplyTruck.IsValid())
	{
		return;
	}
	const uint8 OwningPlayer = SupplyPoint->GetOwningPlayer();
	const bool bHasSuppliesComing = SupplyPoint->GetSupplyPointState() != ESupplyPointState::Inactive;
	if (OwningPlayer == SupplyPointConstants::NeutralOwner or not bHasSuppliesComing)
	{
		return;
	}
	FTransform BaseTransform;
	if (not GetSupplyBaseTransform(OwningPlayer, BaseTransform))
	{
		ScheduleTruckDispatch(SupplyPoint, SupplyPointSubsystemPrivate::NoBaseRetryInterval);
		return;
	}
	USupplyTruckComponent* SupplyTruck = SpawnSupplyTruck(OwningPlayer, BaseTransform);
	if (not IsValid(SupplyTruck))
	{
		return;
	}
	// Spawning can run arbitrary BeginPlay code; the run array may have moved.
	Run = FindRun(SupplyPoint);
	if (Run == nullptr)
	{
		SupplyTruck->GetOwner()->Destroy();
		return;
	}
	Run->SupplyTruck = SupplyTruck;
	BindSupplyTruck(SupplyTruck);
	SupplyTruck->StartSupplyRun(SupplyPoint);
	PlaySupplyAnnouncement(ESupplyAnnouncement::SupplyTruckDispatched, OwningPlayer);
}

UClass* USupplyPointSubsystem::LoadSupplyTruckClass(const uint8 OwningPlayer) const
{
	const USupplyPointDeveloperSettings* Settings = GetDefault<USupplyPointDeveloperSettings>();
	TSoftClassPtr<ATankMaster> TruckClass = Settings->EnemySupplyTruckClass;
	if (OwningPlayer == SupplyPointConstants::PlayerOwner)
	{
		const TSoftClassPtr<ATankMaster>* FactionTruckClass =
			Settings->SupplyTruckClassPerFaction.Find(FRTS_Statics::GetPlayerFaction(this));
		TruckClass = FactionTruckClass != nullptr and not FactionTruckClass->IsNull()
			             ? *FactionTruckClass
			             : Settings->DefaultSupplyTruckClass;
	}
	if (TruckClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"No supply truck class set for player " + FString::FromInt(OwningPlayer)
			+ "; set one under Project Settings > RTS Supply Points.");
		return nullptr;
	}
	return TruckClass.LoadSynchronous();
}

USupplyTruckComponent* USupplyPointSubsystem::SpawnSupplyTruck(const uint8 OwningPlayer,
                                                               const FTransform& BaseTransform) const
{
	UWorld* World = GetWorld();
	UClass* TruckClass = LoadSupplyTruckClass(OwningPlayer);
	if (World == nullptr or TruckClass == nullptr)
	{
		return nullptr;
	}
	FTransform SpawnTransform = BaseTransform;
	SpawnTransform.AddToTranslation(FVector(0.f, 0.f, SupplyPointSubsystemPrivate::SpawnHeightOffset));
	AActor* Truck = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, TruckClass, SpawnTransform, OwningPlayer);
	if (not IsValid(Truck))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn the supply truck " + TruckClass->GetName());
		return nullptr;
	}
	USupplyTruckComponent* SupplyTruck = Truck->FindComponentByClass<USupplyTruckComponent>();
	if (SupplyTruck == nullptr)
	{
		// Any tank master Blueprint can serve as a supply truck: it gets its driver here.
		SupplyTruck = NewObject<USupplyTruckComponent>(Truck, TEXT("SupplyTruckComponent"));
		SupplyTruck->RegisterComponent();
	}
	return SupplyTruck;
}

void USupplyPointSubsystem::BindSupplyTruck(USupplyTruckComponent* SupplyTruck)
{
	SupplyTruck->OnSupplyTruckLoaded.AddUObject(this, &USupplyPointSubsystem::OnSupplyTruckLoaded);
	SupplyTruck->OnSupplyTruckRunEnded.AddUObject(this, &USupplyPointSubsystem::OnSupplyTruckRunEnded);
}

void USupplyPointSubsystem::OnSupplyTruckLoaded(USupplyTruckComponent* SupplyTruck)
{
	if (not IsValid(SupplyTruck))
	{
		return;
	}
	PlaySupplyAnnouncement(ESupplyAnnouncement::SupplyTruckReturning, SupplyTruck->GetTruckOwningPlayer());
}

void USupplyPointSubsystem::OnSupplyTruckRunEnded(USupplyTruckComponent* SupplyTruck, const bool bDelivered)
{
	FSupplyPointRun* Run = M_SupplyPointRuns.FindByPredicate([SupplyTruck](const FSupplyPointRun& Candidate)
	{
		return Candidate.SupplyTruck.Get() == SupplyTruck;
	});
	if (Run == nullptr)
	{
		// A recalled truck of a previous owner: its point already moved on.
		return;
	}
	Run->SupplyTruck.Reset();
	ASupplyPoint* SupplyPoint = Run->SupplyPoint.Get();
	if (not IsValid(SupplyPoint))
	{
		return;
	}
	// The dispatch itself checks whether the point still has supplies coming (single-run points stop here).
	ScheduleTruckDispatch(SupplyPoint, SupplyPoint->GetSupplyPointSettings().TimeBetweenTrucks);
}

void USupplyPointSubsystem::SetSupplyBaseOverride(const uint8 OwningPlayer, const FTransform& BaseTransform)
{
	M_SupplyBaseOverrides.Add(OwningPlayer, BaseTransform);
}

void USupplyPointSubsystem::ClearSupplyBaseOverride(const uint8 OwningPlayer)
{
	M_SupplyBaseOverrides.Remove(OwningPlayer);
}

bool USupplyPointSubsystem::GetSupplyBaseTransform(const uint8 OwningPlayer, FTransform& OutBaseTransform) const
{
	if (const FTransform* BaseOverride = M_SupplyBaseOverrides.Find(OwningPlayer))
	{
		OutBaseTransform = *BaseOverride;
		return true;
	}
	if (OwningPlayer == SupplyPointConstants::PlayerOwner)
	{
		return GetPlayerBaseTransform(OutBaseTransform);
	}
	// AI players have no base of their own yet: give them one with SetSupplyBaseOverride.
	return false;
}

bool USupplyPointSubsystem::GetPlayerBaseTransform(FTransform& OutBaseTransform) const
{
	ANomadicVehicle* PlayerHq = FRTS_Statics::GetPlayerHq(this);
	if (not RTSFunctionLibrary::RTSIsValid(PlayerHq))
	{
		return false;
	}
	const UTrainerComponent* Trainer = PlayerHq->GetTrainerComponent();
	const bool bIsDeployed = PlayerHq->GetNomadicStatus() == ENomadStatus::Building;
	if (bIsDeployed and IsValid(Trainer) and Trainer->GetSpawnTransform(OutBaseTransform))
	{
		return true;
	}
	const FVector InFrontOfHq = PlayerHq->GetActorLocation()
		+ PlayerHq->GetActorForwardVector() * SupplyPointSubsystemPrivate::FallbackSpawnDistanceFromHq;
	bool bWasProjected = false;
	const FVector ProjectedLocation = RTSFunctionLibrary::GetLocationProjected(this, InFrontOfHq, true, bWasProjected);
	OutBaseTransform = FTransform(PlayerHq->GetActorRotation(), bWasProjected ? ProjectedLocation : InFrontOfHq);
	return true;
}

void USupplyPointSubsystem::DeliverCargo(const uint8 OwningPlayer, const FSupplyTruckCargo& Cargo) const
{
	if (OwningPlayer != SupplyPointConstants::PlayerOwner)
	{
		// AI supply runs: credit the enemy economy here.
		return;
	}
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		RTSFunctionLibrary::ReportError("Supply truck cargo lost: no player resource manager.");
		return;
	}
	for (const ERTSResourceType ResourceType : SupplyPointSubsystemPrivate::CargoResourceTypes)
	{
		const int32 Amount = Cargo.GetAmount(ResourceType);
		if (Amount > 0)
		{
			// Clamps to the storage; the part that does not fit is lost by design.
			ResourceManager->AddResource(ResourceType, Amount);
		}
	}
}

const FSupplyAnnouncementLines& USupplyPointSubsystem::GetAnnouncementLines(
	const ESupplyAnnouncement Announcement) const
{
	const USupplyPointDeveloperSettings* Settings = GetDefault<USupplyPointDeveloperSettings>();
	switch (Announcement)
	{
	case ESupplyAnnouncement::SupplyTruckDispatched: return Settings->SupplyTruckDispatchedLines;
	case ESupplyAnnouncement::SupplyTruckReturning: return Settings->SupplyTruckReturningLines;
	default: return Settings->SupplyPointCapturedLines;
	}
}

void USupplyPointSubsystem::PlaySupplyAnnouncement(const ESupplyAnnouncement Announcement, const uint8 OwningPlayer)
{
	if (OwningPlayer != SupplyPointConstants::PlayerOwner)
	{
		return;
	}
	const FSupplyAnnouncementLines& Lines = GetAnnouncementLines(Announcement);
	FSupplyAnnouncementDeck& Deck = M_AnnouncementDecks.FindOrAdd(Announcement);
	const int32 LineIndex = Deck.DrawNextLineIndex(Lines.VoiceLines.Num());
	if (not Lines.VoiceLines.IsValidIndex(LineIndex))
	{
		// No lines set (yet): this moment stays silent.
		return;
	}
	USoundBase* VoiceLine = Lines.VoiceLines[LineIndex].LoadSynchronous();
	UPlayerPortraitManager* PortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	if (not IsValid(VoiceLine) or not IsValid(PortraitManager))
	{
		return;
	}
	// Always through the portrait (with subtitles), never as a bare announcer line.
	PortraitManager->PlayPortrait(Lines.PortraitType, VoiceLine);
}
