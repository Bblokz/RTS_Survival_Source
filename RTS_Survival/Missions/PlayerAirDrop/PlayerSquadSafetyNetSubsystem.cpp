// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerSquadSafetyNetSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PlayerAirDropSubsystem.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace PlayerSquadSafetyNetConstants
{
	// The human player's owner index (see PlayerAirDropConstants::LocalPlayerNumber).
	constexpr int32 LocalPlayerNumber = 1;
}

void UPlayerSquadSafetyNetSubsystem::StartSafetyNet(const FPlayerSquadSafetyNetSettings& Settings,
                                                    AMissionManager* MissionManager)
{
	UWorld* const World = GetWorld();
	if (not Settings.bEnabled || World == nullptr)
	{
		return;
	}
	if (Settings.TargetSquadCount <= Settings.HelpAtOrBelowSquadCount)
	{
		RTSFunctionLibrary::ReportError("Squad safety net: TargetSquadCount must be above HelpAtOrBelowSquadCount, "
			"or a drop could never bring the player out of the danger zone. The safety net stays off.");
		return;
	}
	M_Settings = Settings;
	M_MissionManager = MissionManager;
	bM_IsWatching = true;
	bM_IsArmed = false;
	World->GetTimerManager().SetTimer(M_CheckTimer, FTimerDelegate::CreateUObject(
		                                  this, &UPlayerSquadSafetyNetSubsystem::CheckSquads),
	                                  M_Settings.CheckIntervalSeconds, true);
}

UPlayerSquadSafetyNetSubsystem* UPlayerSquadSafetyNetSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UPlayerSquadSafetyNetSubsystem>() : nullptr;
}

void UPlayerSquadSafetyNetSubsystem::Deinitialize()
{
	StopWatching();
	Super::Deinitialize();
}

void UPlayerSquadSafetyNetSubsystem::CheckSquads()
{
	if (GetShouldStop())
	{
		StopWatching();
		return;
	}
	FVector SquadCentre = FVector::ZeroVector;
	M_LastCountedSquads = CountPlayerSquads(SquadCentre);
	if (M_LastCountedSquads > 0)
	{
		M_LastKnownSquadCentre = SquadCentre;
		bM_HasKnownSquadCentre = true;
	}
	if (M_LastCountedSquads > M_Settings.HelpAtOrBelowSquadCount)
	{
		bM_IsArmed = true;
		return;
	}
	if (not bM_IsArmed || GetIsDropOnCooldown())
	{
		return;
	}
	SendDrop(M_Settings.TargetSquadCount - M_LastCountedSquads);
}

bool UPlayerSquadSafetyNetSubsystem::GetShouldStop() const
{
	if (not M_Settings.StopWhenMissionClassCompleted)
	{
		return false;
	}
	const AMissionManager* const MissionManager = M_MissionManager.Get();
	if (not IsValid(MissionManager))
	{
		return true;
	}
	return MissionManager->GetHasCompletedMissionClassExact(M_Settings.StopWhenMissionClassCompleted);
}

void UPlayerSquadSafetyNetSubsystem::StopWatching()
{
	bM_IsWatching = false;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CheckTimer);
	}
}

int32 UPlayerSquadSafetyNetSubsystem::CountPlayerSquads(FVector& OutCentre) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}
	int32 SquadCount = 0;
	FVector LocationSum = FVector::ZeroVector;
	for (TActorIterator<ASquadController> It(World); It; ++It)
	{
		ASquadController* const Squad = *It;
		const URTSComponent* const RTSComponent = IsValid(Squad) ? Squad->FindComponentByClass<URTSComponent>() : nullptr;
		if (RTSComponent == nullptr || RTSComponent->GetOwningPlayer() != PlayerSquadSafetyNetConstants::LocalPlayerNumber
			|| RTSComponent->GetSubtypeAsSquadSubtype() != M_Settings.SquadSubtype)
		{
			continue;
		}
		const TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
		if (Soldiers.IsEmpty())
		{
			continue;
		}
		SquadCount += 1;
		LocationSum += Soldiers[0]->GetActorLocation();
	}
	if (SquadCount > 0)
	{
		OutCentre = LocationSum / SquadCount;
	}
	return SquadCount;
}

bool UPlayerSquadSafetyNetSubsystem::GetIsDropOnCooldown() const
{
	const UWorld* const World = GetWorld();
	if (M_LastDropWorldSeconds < 0.f || World == nullptr)
	{
		return false;
	}
	return World->GetTimeSeconds() - M_LastDropWorldSeconds < M_Settings.MinSecondsBetweenDrops;
}

void UPlayerSquadSafetyNetSubsystem::SendDrop(const int32 SquadsToDrop)
{
	UPlayerAirDropSubsystem* const AirDrop = UPlayerAirDropSubsystem::Get(this);
	const UWorld* const World = GetWorld();
	if (SquadsToDrop <= 0 || AirDrop == nullptr || World == nullptr)
	{
		return;
	}
	FPlayerAirDropPayload Payload;
	Payload.bParachuteSquads = true;
	Payload.ParachuteAbility = M_Settings.ParachuteAbility;
	Payload.Squads.Init(M_Settings.SquadSubtype, SquadsToDrop);
	const FVector LandingPoint = PickLandingPoint();
	// The cooldown starts even when the launch fails, so a missing template reports once a minute, not every check.
	M_LastDropWorldSeconds = World->GetTimeSeconds();
	if (not AirDrop->DeliverPlayerUnitsByAir(LandingPoint, Payload))
	{
		RTSFunctionLibrary::ReportError("Squad safety net: the paratrooper drop could not be launched (missing "
			"ability template or player ability manager).");
		return;
	}
	M_DropCount += 1;
	M_LastDropLocation = LandingPoint;
	UE_LOG(LogTemp, Display, TEXT("SQUAD_SAFETY_NET drop %d: %d squads at %s (player had %d)"), M_DropCount,
	       SquadsToDrop, *LandingPoint.ToCompactString(), M_LastCountedSquads);
}

FVector UPlayerSquadSafetyNetSubsystem::PickLandingPoint() const
{
	// Without a known position of the player's squads the first landing point is the safest guess (map start).
	const FVector Reference = bM_HasKnownSquadCentre ? M_LastKnownSquadCentre : FVector::ZeroVector;
	if (M_Settings.LandingPoints.IsEmpty())
	{
		return Reference;
	}
	if (not bM_HasKnownSquadCentre)
	{
		return M_Settings.LandingPoints[0];
	}
	FVector Best = M_Settings.LandingPoints[0];
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FVector& Point : M_Settings.LandingPoints)
	{
		const float DistanceSquared = FVector::DistSquared2D(Point, Reference);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Point;
		}
	}
	return Best;
}

int32 UPlayerSquadSafetyNetSubsystem::MapTest_KillSquad(AActor* Squad)
{
#if RTS_WITH_MAP_TESTS
	ASquadController* const SquadController = Cast<ASquadController>(Squad);
	if (not IsValid(SquadController))
	{
		return 0;
	}
	int32 Killed = 0;
	for (ASquadUnit* const Soldier : SquadController->GetSquadUnitsChecked())
	{
		if (IsValid(Soldier))
		{
			Soldier->OnRunOverByVehicle(nullptr, Soldier->GetActorForwardVector(), false);
			Killed += 1;
		}
	}
	return Killed;
#else
	return 0;
#endif
}
