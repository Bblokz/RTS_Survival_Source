// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyFallbackSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"

namespace EnemyFallbackConstants
{
	// A rally point this close to the origin is treated as unset.
	constexpr float UnsetRallyTolerance = 1.f;
}

UEnemyFallbackSubsystem* UEnemyFallbackSubsystem::GetEnemyFallbackSubsystem(const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	return World->GetSubsystem<UEnemyFallbackSubsystem>();
}

void UEnemyFallbackSubsystem::StartFallbackWatch(const FEnemyFallbackSettings& Settings)
{
	if (Settings.RallyLocation.IsNearlyZero(EnemyFallbackConstants::UnsetRallyTolerance))
	{
		RTSFunctionLibrary::ReportError("Enemy fallback watch needs a rally location; not started."
			"\nAt UEnemyFallbackSubsystem::StartFallbackWatch()");
		return;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_Settings = Settings;
	bM_IsWatching = true;
	World->GetTimerManager().SetTimer(M_CheckTimerHandle, this, &UEnemyFallbackSubsystem::CheckSquads,
	                                  M_Settings.CheckIntervalSeconds, true);
}

void UEnemyFallbackSubsystem::StopFallbackWatch()
{
	bM_IsWatching = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CheckTimerHandle);
	}
}

bool UEnemyFallbackSubsystem::ForceUnitToFallBack(AActor* Unit)
{
	AActor* FallbackUnit = ResolveFallbackUnit(Unit);
	if (not IsValid(FallbackUnit) || GetIsFleeing(FallbackUnit))
	{
		return false;
	}
	return SendUnitToRallyPoint(FallbackUnit);
}

bool UEnemyFallbackSubsystem::GetIsFleeing(const AActor* Actor) const
{
	const AActor* FallbackUnit = ResolveFallbackUnit(Actor);
	if (not IsValid(FallbackUnit))
	{
		return false;
	}
	for (const TWeakObjectPtr<AActor>& FleeingUnit : M_FleeingUnits)
	{
		if (FleeingUnit.Get() == FallbackUnit)
		{
			return GetIsUnitAlive(FallbackUnit);
		}
	}
	return false;
}

int32 UEnemyFallbackSubsystem::GetFleeingCount() const
{
	return GetFleeingUnits().Num();
}

TArray<AActor*> UEnemyFallbackSubsystem::GetFleeingUnits() const
{
	TArray<AActor*> AliveFleeingUnits;
	for (const TWeakObjectPtr<AActor>& FleeingUnit : M_FleeingUnits)
	{
		if (FleeingUnit.IsValid() && GetIsUnitAlive(FleeingUnit.Get()))
		{
			AliveFleeingUnits.Add(FleeingUnit.Get());
		}
	}
	return AliveFleeingUnits;
}

void UEnemyFallbackSubsystem::Deinitialize()
{
	StopFallbackWatch();
	M_SquadTracks.Empty();
	M_FleeingUnits.Empty();
	Super::Deinitialize();
}

void UEnemyFallbackSubsystem::CheckSquads()
{
	UWorld* World = GetWorld();
	if (not bM_IsWatching || not IsValid(World))
	{
		return;
	}
	PruneTracks();
	const double NowSeconds = World->GetTimeSeconds();
	// Collect first: sending a squad away changes formations and retreat lists, not the actor list, but stay safe.
	TArray<ASquadController*> Squads;
	for (TActorIterator<ASquadController> It(World); It; ++It)
	{
		Squads.Add(*It);
	}
	for (ASquadController* Squad : Squads)
	{
		CheckOneSquad(Squad, NowSeconds);
	}
}

void UEnemyFallbackSubsystem::CheckOneSquad(ASquadController* Squad, const double NowSeconds)
{
	if (not IsValid(Squad) || GetIsFleeing(Squad))
	{
		return;
	}
	const URTSComponent* RTSComponent = Squad->GetRTSComponent();
	if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != M_Settings.OwningPlayerToWatch)
	{
		return;
	}
	FEnemyFallbackSquadTrack& Track = M_SquadTracks.FindOrAdd(Squad);
	if (not GetShouldSquadFallBack(Squad, Track, NowSeconds))
	{
		return;
	}
	SendUnitToRallyPoint(Squad);
}

bool UEnemyFallbackSubsystem::GetShouldSquadFallBack(const ASquadController* Squad, FEnemyFallbackSquadTrack& Track,
                                                    const double NowSeconds) const
{
	const int32 AliveCount = Squad->GetCurrentAliveSquadUnitCountPure();
	if (AliveCount <= 0)
	{
		return false;
	}
	Track.FullStrength = FMath::Max(Track.FullStrength, AliveCount);
	if (Track.FullStrength < M_Settings.MinSquadSizeToWatch)
	{
		return false;
	}
	if (AliveCount < Track.FullStrength && Track.FirstLossSeconds < 0.0)
	{
		Track.FirstLossSeconds = NowSeconds;
	}
	if (Track.FirstLossSeconds < 0.0 || NowSeconds - Track.FirstLossSeconds < M_Settings.MinCombatSeconds)
	{
		return false;
	}
	const float LostFraction = 1.f - static_cast<float>(AliveCount) / static_cast<float>(Track.FullStrength);
	if (LostFraction < M_Settings.LossFractionToFallBack)
	{
		return false;
	}
	const float DistanceToRally = FVector::Dist2D(Squad->GetActorLocation(), M_Settings.RallyLocation);
	return DistanceToRally > M_Settings.StayAndFightRadius;
}

bool UEnemyFallbackSubsystem::SendUnitToRallyPoint(AActor* UnitActor)
{
	AEnemyController* EnemyController = GetEnemyControllerForUnit(UnitActor);
	if (not IsValid(EnemyController))
	{
		return false;
	}
	const UEnemyRetreatController* RetreatController = EnemyController->GetEnemyRetreatController();
	if (IsValid(RetreatController) && RetreatController->GetIsUnitRetreating(UnitActor))
	{
		// Already retreating for another reason (damaged vehicle, scripted retreat).
		return false;
	}
	if (not EnemyController->RetreatUnitToRallyPoint(UnitActor, M_Settings.RallyLocation))
	{
		return false;
	}
	M_FleeingUnits.Add(UnitActor);
	++M_TotalFledCount;
	return true;
}

AEnemyController* UEnemyFallbackSubsystem::GetEnemyControllerForUnit(const AActor* UnitActor) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(UnitActor) || not IsValid(World))
	{
		return nullptr;
	}
	const URTSComponent* RTSComponent = UnitActor->FindComponentByClass<URTSComponent>();
	const UEnemyControllerSubsystem* ControllerSubsystem = World->GetSubsystem<UEnemyControllerSubsystem>();
	if (not IsValid(RTSComponent) || not IsValid(ControllerSubsystem))
	{
		return nullptr;
	}
	return ControllerSubsystem->GetEnemyControllerForPlayer(RTSComponent->GetOwningPlayer());
}

bool UEnemyFallbackSubsystem::GetIsUnitAlive(const AActor* UnitActor) const
{
	if (not IsValid(UnitActor) || UnitActor->IsActorBeingDestroyed())
	{
		return false;
	}
	if (const ASquadController* Squad = Cast<ASquadController>(UnitActor))
	{
		return Squad->GetCurrentAliveSquadUnitCountPure() > 0;
	}
	return true;
}

void UEnemyFallbackSubsystem::PruneTracks()
{
	for (auto It = M_SquadTracks.CreateIterator(); It; ++It)
	{
		if (not It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (int32 Index = M_FleeingUnits.Num() - 1; Index >= 0; --Index)
	{
		if (not M_FleeingUnits[Index].IsValid())
		{
			M_FleeingUnits.RemoveAtSwap(Index);
		}
	}
}

AActor* UEnemyFallbackSubsystem::ResolveFallbackUnit(const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}
	if (const ASquadUnit* SquadUnit = Cast<ASquadUnit>(Actor))
	{
		return SquadUnit->GetHasSquadController() ? SquadUnit->GetSquadControllerChecked().Get() : nullptr;
	}
	return const_cast<AActor*>(Actor);
}
