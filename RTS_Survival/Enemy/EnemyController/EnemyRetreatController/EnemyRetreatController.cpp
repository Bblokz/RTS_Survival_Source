// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyRetreatController.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EnemyRetreatControllerConstants
{
	const float RetreatLocationTolerance = 450.f;
	const float RetreatLocationToleranceSquared = RetreatLocationTolerance * RetreatLocationTolerance;
	const float DebugTextZOffset = 250.f;
	const float DebugTextDuration = 6.f;
	const float CounterattackLocationZeroTolerance = 1.f;
	const int32 DefaultCounterAttackFormationWidth = 2;
	const float DefaultCounterAttackFormationOffset = 1.f;
}

UEnemyRetreatController::UEnemyRetreatController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyRetreatController::InitRetreatController(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyRetreatController::StartRetreat(
	const TArray<FRetreatElement>& RetreatingSquadControllers,
	const TArray<FRetreatElement>& ReverseRetreatUnits,
	const FVector& CounterattackLocation,
	const EPostRetreatCounterStrategy PostRetreatCounterStrategy,
	const float TimeTillCounterAttackAfterLastRetreatingUnitReached,
	const float MaxTimeWaitTillCounterAttack)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	if (RetreatingSquadControllers.IsEmpty() && ReverseRetreatUnits.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Retreat request had no valid units. \nAt UEnemyRetreatController::StartRetreat()");
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"Retreat request could not start because world was invalid. "
			"\nAt UEnemyRetreatController::StartRetreat()");
		return;
	}

	if (PostRetreatCounterStrategy == EPostRetreatCounterStrategy::Attack
		&& CounterattackLocation.IsNearlyZero(EnemyRetreatControllerConstants::CounterattackLocationZeroTolerance))
	{
		RTSFunctionLibrary::ReportError(
			"Counterattack strategy was set to Attack but the counterattack location was near zero. "
			"\nAt UEnemyRetreatController::StartRetreat()");
		DestroyRetreatElements(RetreatingSquadControllers);
		DestroyRetreatElements(ReverseRetreatUnits);
		return;
	}

	FEnemyRetreatData NewRetreatData;
	NewRetreatData.RetreatID = GenerateRetreatID();
	NewRetreatData.RetreatingSquadControllers = RetreatingSquadControllers;
	NewRetreatData.ReverseRetreatUnits = ReverseRetreatUnits;
	NewRetreatData.CounterattackLocation = CounterattackLocation;
	NewRetreatData.PostRetreatCounterStrategy = PostRetreatCounterStrategy;
	NewRetreatData.TimeTillCounterAttackAfterLastRetreatingUnitReached = TimeTillCounterAttackAfterLastRetreatingUnitReached;
	NewRetreatData.MaxTimeWaitTillCounterAttack = MaxTimeWaitTillCounterAttack;
	NewRetreatData.RetreatStartTimeSeconds = World->GetTimeSeconds();

	const int32 RetreatIndex = M_ActiveRetreats.Add(NewRetreatData);
	FEnemyRetreatData& AddedRetreat = M_ActiveRetreats[RetreatIndex];

	if (AddedRetreat.PostRetreatCounterStrategy == EPostRetreatCounterStrategy::Attack)
	{
		TWeakObjectPtr<UEnemyRetreatController> WeakThis(this);
		const int32 RetreatID = AddedRetreat.RetreatID;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([WeakThis, RetreatID]()
		{
			if (UEnemyRetreatController* This = WeakThis.Get())
			{
				This->OnCounterAttackTimerExpired(RetreatID, TEXT("Max wait timer"));
			}
		});

		World->GetTimerManager().SetTimer(
			AddedRetreat.MaxWaitCounterAttackTimerHandle,
			TimerDelegate,
			AddedRetreat.MaxTimeWaitTillCounterAttack,
			false);
	}

	StartRetreatCheckTimer();
}

bool UEnemyRetreatController::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_EnemyController",
		"UEnemyRetreatController::EnsureEnemyControllerIsValid",
		this);
	return false;
}

void UEnemyRetreatController::StartRetreatCheckTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(M_RetreatCheckTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_RetreatCheckTimerHandle,
		this,
		&UEnemyRetreatController::CheckRetreatingUnits,
		EnemyAISettings::EnemyRetreatControllerCheckInterval,
		true);
}

void UEnemyRetreatController::StopRetreatCheckTimerIfIdle()
{
	if (M_ActiveRetreats.Num() > 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RetreatCheckTimerHandle);
	}
}

void UEnemyRetreatController::CheckRetreatingUnits()
{
	if (M_ActiveRetreats.IsEmpty())
	{
		StopRetreatCheckTimerIfIdle();
		return;
	}

	UWorld* World = GetWorld();
	const float CurrentTimeSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.f;

	for (int32 RetreatIndex = M_ActiveRetreats.Num() - 1; RetreatIndex >= 0; --RetreatIndex)
	{
		FEnemyRetreatData& RetreatData = M_ActiveRetreats[RetreatIndex];
		CleanupInvalidRetreatUnits(RetreatData);

		if (RetreatData.RetreatingSquadControllers.IsEmpty() && RetreatData.ReverseRetreatUnits.IsEmpty())
		{
			ClearRetreatTimers(RetreatData);
			M_ActiveRetreats.RemoveAtSwap(RetreatIndex);
			continue;
		}

		bool bAllUnitsReached = true;
		ProcessRetreatElementsForCheck(RetreatData, RetreatData.RetreatingSquadControllers, false, bAllUnitsReached);
		ProcessRetreatElementsForCheck(RetreatData, RetreatData.ReverseRetreatUnits, true, bAllUnitsReached);

		if (RetreatData.RetreatingSquadControllers.IsEmpty() && RetreatData.ReverseRetreatUnits.IsEmpty())
		{
			ClearRetreatTimers(RetreatData);
			M_ActiveRetreats.RemoveAtSwap(RetreatIndex);
			continue;
		}

		if (RetreatData.PostRetreatCounterStrategy != EPostRetreatCounterStrategy::Attack)
		{
			continue;
		}

		if (bAllUnitsReached && not RetreatData.bM_HasAllUnitsReached)
		{
			RetreatData.bM_HasAllUnitsReached = true;
			RetreatData.AllUnitsReachedTimeSeconds = CurrentTimeSeconds;

			if (IsValid(World))
			{
				const float Delay = FMath::Max(0.f, RetreatData.TimeTillCounterAttackAfterLastRetreatingUnitReached);
				TWeakObjectPtr<UEnemyRetreatController> WeakThis(this);
				const int32 RetreatID = RetreatData.RetreatID;
				FTimerDelegate TimerDelegate;
				TimerDelegate.BindLambda([WeakThis, RetreatID]()
				{
					if (UEnemyRetreatController* This = WeakThis.Get())
					{
						This->OnCounterAttackTimerExpired(RetreatID, TEXT("All units reached timer"));
					}
				});

				World->GetTimerManager().SetTimer(
					RetreatData.PostReachCounterAttackTimerHandle,
					TimerDelegate,
					Delay,
					false);
			}
		}
	}

	StopRetreatCheckTimerIfIdle();
}

void UEnemyRetreatController::CleanupInvalidRetreatUnits(FEnemyRetreatData& RetreatData) const
{
	RetreatData.RetreatingSquadControllers.RemoveAllSwap([](const FRetreatElement& Element)
	{
		return not Element.Unit.IsValid();
	});

	RetreatData.ReverseRetreatUnits.RemoveAllSwap([](const FRetreatElement& Element)
	{
		return not Element.Unit.IsValid();
	});
}

void UEnemyRetreatController::ClearRetreatTimers(FEnemyRetreatData& RetreatData) const
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetreatData.MaxWaitCounterAttackTimerHandle);
		World->GetTimerManager().ClearTimer(RetreatData.PostReachCounterAttackTimerHandle);
	}
}

void UEnemyRetreatController::DestroyRetreatUnit(ICommands* Commands) const
{
	if (not Commands)
	{
		return;
	}

	AActor* OwnerActor = Commands->GetOwnerActor();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	const FVector DebugLocation = OwnerActor->GetActorLocation() + FVector(0.f, 0.f,
		EnemyRetreatControllerConstants::DebugTextZOffset);
	DrawDebugTextAtLocation("Destroying retreat unit", DebugLocation, FColor::Red);

	OwnerActor->Destroy();
}

void UEnemyRetreatController::DestroyRetreatElements(const TArray<FRetreatElement>& RetreatElements) const
{
	for (const FRetreatElement& RetreatElement : RetreatElements)
	{
		if (RetreatElement.Unit.IsValid())
		{
			DestroyRetreatUnit(RetreatElement.Unit.Get());
		}
	}
}

bool UEnemyRetreatController::GetHasReachedRetreatLocation(const ICommands* Commands,
                                                           const FVector& RetreatLocation) const
{
	if (not Commands)
	{
		return false;
	}

	const FVector UnitLocation = Commands->GetOwnerLocation();
	const float DistanceSquared = FVector::DistSquared(UnitLocation, RetreatLocation);
	return DistanceSquared <= EnemyRetreatControllerConstants::RetreatLocationToleranceSquared;
}

void UEnemyRetreatController::ProcessRetreatElementsForCheck(
	FEnemyRetreatData& RetreatData,
	TArray<FRetreatElement>& RetreatElements,
	const bool bUseReverseMove,
	bool& bAllUnitsReached)
{
	for (int32 ElementIndex = RetreatElements.Num() - 1; ElementIndex >= 0; --ElementIndex)
	{
		FRetreatElement& RetreatElement = RetreatElements[ElementIndex];
		if (not RetreatElement.Unit.IsValid())
		{
			RetreatElements.RemoveAtSwap(ElementIndex);
			continue;
		}

		ICommands* Commands = RetreatElement.Unit.Get();
		if (not Commands)
		{
			RetreatElements.RemoveAtSwap(ElementIndex);
			continue;
		}

		bool bHasReachedRetreat = false;
		if (Commands->GetIsUnitIdle())
		{
			bHasReachedRetreat = true;
		}
		else if (GetHasReachedRetreatLocation(Commands, RetreatElement.RetreatLocation))
		{
			bHasReachedRetreat = true;
		}
		else
		{
			TryReissueRetreatOrder(Commands, RetreatElement.RetreatLocation, bUseReverseMove);
			bAllUnitsReached = false;
			const FVector DebugLocation = Commands->GetOwnerLocation() + FVector(0.f, 0.f,
				EnemyRetreatControllerConstants::DebugTextZOffset);
			DrawDebugTextAtLocation("Reissuing retreat", DebugLocation, FColor::Orange);
			continue;
		}

		const FVector DebugLocation = Commands->GetOwnerLocation() + FVector(0.f, 0.f,
			EnemyRetreatControllerConstants::DebugTextZOffset);
		DrawDebugTextAtLocation("Reached retreat", DebugLocation, FColor::Green);

		if (RetreatData.PostRetreatCounterStrategy == EPostRetreatCounterStrategy::None
			|| RetreatData.PostRetreatCounterStrategy == EPostRetreatCounterStrategy::RemoveUnits)
		{
			if (RetreatData.PostRetreatCounterStrategy == EPostRetreatCounterStrategy::None
				&& not RetreatData.bM_HasReportedNoneStrategy)
			{
				RTSFunctionLibrary::ReportError(
					"Post retreat strategy was None; destroying units instead. "
					"\nAt UEnemyRetreatController::CheckRetreatingUnits()");
				RetreatData.bM_HasReportedNoneStrategy = true;
			}

			DestroyRetreatUnit(Commands);
			RetreatElements.RemoveAtSwap(ElementIndex);
			continue;
		}

		bAllUnitsReached = bAllUnitsReached && bHasReachedRetreat;
	}
}

bool UEnemyRetreatController::TryReissueRetreatOrder(
	ICommands* Commands,
	const FVector& RetreatLocation,
	const bool bUseReverseMove) const
{
	if (not Commands)
	{
		return false;
	}

	const ECommandQueueError CommandError = bUseReverseMove
		? Commands->ReverseUnitToLocation(RetreatLocation, true)
		: Commands->RetreatToLocation(RetreatLocation, true);

	if (CommandError != ECommandQueueError::NoError)
	{
		const FString UnitName = Commands->GetOwnerName();
		RTSFunctionLibrary::ReportError(
			"Failed to reissue retreat order for unit: " + UnitName
			+ "\nAt UEnemyRetreatController::TryReissueRetreatOrder()");
		return false;
	}

	return true;
}

void UEnemyRetreatController::DrawDebugTextAtLocation(const FString& Message, const FVector& Location,
                                                      const FColor& Color) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugString(World, Location, Message, nullptr, Color,
				EnemyRetreatControllerConstants::DebugTextDuration, false);
		}
	}
}

void UEnemyRetreatController::OnCounterAttackTimerExpired(const int32 RetreatID, const FString& TriggerReason)
{
	const int32 RetreatIndex = M_ActiveRetreats.IndexOfByPredicate(
		[RetreatID](const FEnemyRetreatData& RetreatData)
		{
			return RetreatData.RetreatID == RetreatID;
		});

	if (RetreatIndex == INDEX_NONE)
	{
		return;
	}

	TriggerCounterAttack(M_ActiveRetreats[RetreatIndex], TriggerReason);
	ClearRetreatTimers(M_ActiveRetreats[RetreatIndex]);
	M_ActiveRetreats.RemoveAtSwap(RetreatIndex);
	StopRetreatCheckTimerIfIdle();
}

void UEnemyRetreatController::TriggerCounterAttack(FEnemyRetreatData& RetreatData, const FString& TriggerReason)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	if (RetreatData.CounterattackLocation.IsNearlyZero(EnemyRetreatControllerConstants::CounterattackLocationZeroTolerance))
	{
		RTSFunctionLibrary::ReportError(
			"Counterattack location was near zero when attempting to counterattack. "
			"\nAt UEnemyRetreatController::TriggerCounterAttack()");
		DestroyRetreatElements(RetreatData.RetreatingSquadControllers);
		DestroyRetreatElements(RetreatData.ReverseRetreatUnits);
		return;
	}

	TArray<ASquadController*> SquadControllers;
	TArray<ATankMaster*> TankMasters;

	auto AppendUnitFromElement = [&SquadControllers, &TankMasters](const FRetreatElement& RetreatElement)
	{
		if (not RetreatElement.Unit.IsValid())
		{
			return;
		}

		AActor* OwnerActor = RetreatElement.Unit->GetOwnerActor();
		if (not IsValid(OwnerActor))
		{
			return;
		}

		if (ASquadController* SquadController = Cast<ASquadController>(OwnerActor))
		{
			SquadControllers.Add(SquadController);
			return;
		}

		if (ATankMaster* TankMaster = Cast<ATankMaster>(OwnerActor))
		{
			TankMasters.Add(TankMaster);
		}
	};

	for (const FRetreatElement& RetreatElement : RetreatData.RetreatingSquadControllers)
	{
		AppendUnitFromElement(RetreatElement);
	}
	for (const FRetreatElement& RetreatElement : RetreatData.ReverseRetreatUnits)
	{
		AppendUnitFromElement(RetreatElement);
	}

	if (SquadControllers.IsEmpty() && TankMasters.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No valid retreat units found to counterattack. "
			"\nAt UEnemyRetreatController::TriggerCounterAttack()");
		return;
	}

	const FVector AverageSpawnLocation = GetAverageLocationFromRetreatData(RetreatData);
	TArray<FVector> Waypoints;
	Waypoints.Add(RetreatData.CounterattackLocation);

	DrawDebugTextAtLocation(
		"Triggering counterattack: " + TriggerReason,
		RetreatData.CounterattackLocation + FVector(0.f, 0.f, EnemyRetreatControllerConstants::DebugTextZOffset),
		FColor::Yellow);

	M_EnemyController->MoveFormationToLocation(
		SquadControllers,
		TankMasters,
		Waypoints,
		FRotator::ZeroRotator,
		EnemyRetreatControllerConstants::DefaultCounterAttackFormationWidth,
		EnemyRetreatControllerConstants::DefaultCounterAttackFormationOffset,
		AverageSpawnLocation);
}

FVector UEnemyRetreatController::GetAverageLocationFromRetreatData(const FEnemyRetreatData& RetreatData) const
{
	FVector AccumulatedLocation = FVector::ZeroVector;
	int32 ValidUnitCount = 0;

	auto AccumulateLocation = [&AccumulatedLocation, &ValidUnitCount](const FRetreatElement& RetreatElement)
	{
		if (not RetreatElement.Unit.IsValid())
		{
			return;
		}

		AActor* OwnerActor = RetreatElement.Unit->GetOwnerActor();
		if (not IsValid(OwnerActor))
		{
			return;
		}

		AccumulatedLocation += OwnerActor->GetActorLocation();
		ValidUnitCount++;
	};

	for (const FRetreatElement& RetreatElement : RetreatData.RetreatingSquadControllers)
	{
		AccumulateLocation(RetreatElement);
	}
	for (const FRetreatElement& RetreatElement : RetreatData.ReverseRetreatUnits)
	{
		AccumulateLocation(RetreatElement);
	}

	if (ValidUnitCount <= 0)
	{
		return FVector::ZeroVector;
	}

	return AccumulatedLocation / static_cast<float>(ValidUnitCount);
}

int32 UEnemyRetreatController::GenerateRetreatID()
{
	return ++M_LastRetreatID;
}
