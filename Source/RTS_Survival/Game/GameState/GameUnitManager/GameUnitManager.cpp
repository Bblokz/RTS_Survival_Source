// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "GameUnitManager.h"

#include "GetTargetUnitThread/GetAsyncTarget.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UGameUnitManager::UGameUnitManager()
	: M_AsyncTargetProcessor(nullptr)
	  , M_ActorDataUpdateInterval(DeveloperSettings::Async::GameThreadUpdateAsyncWithActorTargetsInterval)
{
}

void UGameUnitManager::SetAllUnitOptimizationEnabled(const bool bEnable)
{
	auto DisableOptimization = [bEnable](UActorComponent* Component)
	{
		URTSOptimizer* Optimizer = Cast<URTSOptimizer>(Component);
		if (not Optimizer)
		{
			return;
		}
		Optimizer->SetOptimizationEnabled(bEnable);
	};
	for (const auto EachTank : M_TankMastersAliveEnemy)
	{
		if (EachTank)
		{
			UActorComponent* Component = EachTank->GetComponentByClass(URTSOptimizer::StaticClass());
			DisableOptimization(Component);
		}
	}
	for (const auto EachTank : M_TankMastersAlivePlayer)
	{
		if (EachTank)
		{
			UActorComponent* Component = EachTank->GetComponentByClass(URTSOptimizer::StaticClass());
			DisableOptimization(Component);
		}
	}
	for (const auto EachSquadUnit : M_SquadUnitsAliveEnemy)
	{
		if (EachSquadUnit)
		{
			UActorComponent* Component = EachSquadUnit->GetComponentByClass(URTSOptimizer::StaticClass());
			DisableOptimization(Component);
		}
	}
	for (const auto EachSquadUnit : M_SquadUnitAlivePlayer)
	{
		if (EachSquadUnit)
		{
			UActorComponent* Component = EachSquadUnit->GetComponentByClass(URTSOptimizer::StaticClass());
			DisableOptimization(Component);
		}
	}
}

void UGameUnitManager::Initialize()
{
	// Create the async target processor and start the thread
	if (!M_AsyncTargetProcessor)
	{
		M_AsyncTargetProcessor = new FGetAsyncTarget();

		// Start the periodic actor data updates
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				M_ActorDataUpdateTimerHandle,
				this,
				&UGameUnitManager::UpdateActorData,
				M_ActorDataUpdateInterval,
				true);
			World->GetTimerManager().SetTimer(
				M_CleanUpInvalidRefsTimerHandle,
				this,
				&UGameUnitManager::CleanupInvalidActorReferences,
				DeveloperSettings::Optimization::CleanUp::UnitMngrCleanUpInterval,
				true);
		}
	}
}

void UGameUnitManager::ShutdownAsyncTargetThread()
{
	// Stop the async target processor and thread data updates
	if (M_AsyncTargetProcessor)
	{
		delete M_AsyncTargetProcessor;
		M_AsyncTargetProcessor = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ActorDataUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(M_CleanUpInvalidRefsTimerHandle);
	}
}

void UGameUnitManager::UpdateActorData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateActorData);

	TMap<uint32, TPair<ETargetPreference, FVector>> EnemyActorData;
	TMap<uint32, TPair<ETargetPreference, FVector>> PlayerActorData;

	// Obtain the pair data where the target type is set together with the location and can be identified with the
	// unique actor ID.
	GetAsyncActorData(false, EnemyActorData);
	GetAsyncActorData(true, PlayerActorData);

	// safe logs inside UpdateActorData:
	auto SafeName = [](AActor* In) -> FString
	{
		return IsValid(In) ? In->GetName() : TEXT("<invalid>");
	};

	if (DeveloperSettings::Debugging::GUnitManager_Compile_DebugSymbols)
	{
		for (const auto& EachEnemy : EnemyActorData)
		{
			AActor* Actor = M_EnemyActorIDToActorMap.FindRef(EachEnemy.Key);
			RTSFunctionLibrary::PrintString("Enemy Actor game state: " + SafeName(Actor), FColor::Red);
		}
		for (const auto& EachPlayer : PlayerActorData)
		{
			AActor* Actor = M_PlayerActorIDToActorMap.FindRef(EachPlayer.Key);
			RTSFunctionLibrary::PrintString("Player Actor game state: " + SafeName(Actor), FColor::Green);
		}
	}

	// Schedule the actor data update on the async thread
	if (M_AsyncTargetProcessor)
	{
		M_AsyncTargetProcessor->ScheduleUpdateActorData(EnemyActorData, PlayerActorData);
	}
}

void UGameUnitManager::RequestClosestTargets(
	const FVector& SearchLocation,
	float SearchRadius,
	int32 NumTargets,
	int32 OwningPlayer,
	ETargetPreference PrefferedTarget,
	TFunction<void(const TArray<AActor*>&)> Callback)
{
	if (M_AsyncTargetProcessor)
	{
		// Generate a unique request ID	
		uint32 RequestID = FPlatformTime::Cycles();
		auto WeakThis = TWeakObjectPtr<UGameUnitManager>(this);

		// Forward the request to the async thread
		M_AsyncTargetProcessor->AddTargetRequest(
			RequestID,
			SearchLocation,
			SearchRadius,
			NumTargets,
			OwningPlayer,
			PrefferedTarget,
			[WeakThis, Callback = MoveTemp(Callback), OwningPlayer](const TArray<uint32>& TargetIDs)
			{
				if (WeakThis.IsValid())
				{
					WeakThis.Get()->OnTargetIDsReceived(TargetIDs, Callback, OwningPlayer);
				}
			});
	}
	else
	{
		// If the async processor is not available, invoke the callback with an empty array
		Callback(TArray<AActor*>());
	}
}


TArray<ASquadUnit*> UGameUnitManager::GetSquadUnitsOfPlayer(const uint8 Player) const
{
	if (Player == 1)
	{
		return M_SquadUnitAlivePlayer;
	}
	return M_SquadUnitsAliveEnemy;
}

void UGameUnitManager::AddSquadUnitForPlayer(ASquadUnit* SquadUnit, const uint8 Player)
{
	if (not RTSFunctionLibrary::RTSIsValid(SquadUnit))
	{
		RTSFunctionLibrary::ReportError(
			"UnitMaster either dead is not valid in AddUnitMasterForPlayer"
			"\n at function AddUnitMasterForPlayer in CPPGameState.cpp."
			"UnitMaster will not be added to array in game state.");
		return;
	}

	// De-dup from both sides first, and notify removals if this was a move.
	if (M_SquadUnitsAliveEnemy.Remove(SquadUnit) > 0)
	{
		GameUnitDelegates.NotifySquad(2, SquadUnit, false);
	}
	if (M_SquadUnitAlivePlayer.Remove(SquadUnit) > 0)
	{
		GameUnitDelegates.NotifySquad(1, SquadUnit, false);
	}

	if (Player == 1)
	{
		M_SquadUnitAlivePlayer.Add(SquadUnit);
		GameUnitDelegates.NotifySquad(1, SquadUnit, true);
		return;
	}

	M_SquadUnitsAliveEnemy.Add(SquadUnit);
	GameUnitDelegates.NotifySquad(Player, SquadUnit, true);
}

void UGameUnitManager::RemoveSquadUnitForPlayer(ASquadUnit* SquadUnit, const uint8 Player)
{
	if (not IsValid(SquadUnit))
	{
		RTSFunctionLibrary::ReportError(
			"SquadUnit is allowed to be dead but not valid in RemoveUnitMasterForPlayer"
			"\n at function RemoveUnitMasterForPlayer in CPPGameState.cpp."
			"UnitMaster will not be removed from array in game state.");
		return;
	}

	if (Player == 1)
	{
		if (M_SquadUnitAlivePlayer.Remove(SquadUnit) > 0)
		{
			GameUnitDelegates.NotifySquad(1, SquadUnit, false);
		}
		return;
	}

	if (M_SquadUnitsAliveEnemy.Remove(SquadUnit) > 0)
	{
		GameUnitDelegates.NotifySquad(Player, SquadUnit, false);
	}
}

void UGameUnitManager::AddTankForPlayer(ATankMaster* Tank, const uint8 Player)
{
	if (not RTSFunctionLibrary::RTSIsValid(Tank))
	{
		RTSFunctionLibrary::ReportError(
			"Tank either dead is not valid in AddTankForPlayer"
			"\n at function AddTankForPlayer in CPPGameState.cpp."
			"tank will not be addec to array in game state.");
		return;
	}

	// De-dup + notify removal for prior ownership.
	if (M_TankMastersAliveEnemy.Remove(Tank) > 0)
	{
		GameUnitDelegates.NotifyTank(2, Tank, false);
	}
	if (M_TankMastersAlivePlayer.Remove(Tank) > 0)
	{
		GameUnitDelegates.NotifyTank(1, Tank, false);
	}

	if (Player == 1)
	{
		M_TankMastersAlivePlayer.Add(Tank);
		GameUnitDelegates.NotifyTank(1, Tank, true);
		return;
	}

	M_TankMastersAliveEnemy.Add(Tank);
	GameUnitDelegates.NotifyTank(Player, Tank, true);
}

void UGameUnitManager::RemoveTankForPlayer(ATankMaster* Tank, const uint8 Player)
{
	if (not IsValid(Tank))
	{
		RTSFunctionLibrary::ReportError(
			"Tank is allowed to be dead but not valid in RemoveTankForPlayer"
			"\n at function RemoveTankForPlayer in CPPGameState.cpp."
			"tank will not be removed from array in game state.");
		return;
	}

	if (Player == 1)
	{
		if (M_TankMastersAlivePlayer.Remove(Tank) > 0)
		{
			GameUnitDelegates.NotifyTank(1, Tank, false);
		}
		return;
	}

	if (M_TankMastersAliveEnemy.Remove(Tank) > 0)
	{
		GameUnitDelegates.NotifyTank(Player, Tank, false);
	}
}

bool UGameUnitManager::AddAircraftForPlayer(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (not IsValid(Aircraft))
	{
		return false;
	}

	// De-dup from both arrays and notify prior removal if needed.
	if (M_AircraftMastersAliveEnemy.Remove(Aircraft) > 0)
	{
		GameUnitDelegates.NotifyAircraft(2, Aircraft, false);
	}
	if (M_AircraftMastersAlivePlayer.Remove(Aircraft) > 0)
	{
		GameUnitDelegates.NotifyAircraft(1, Aircraft, false);
	}

	if (Player == 1)
	{
		if (M_AircraftMastersAlivePlayer.Contains(Aircraft))
		{
			return false;
		}
		M_AircraftMastersAlivePlayer.Add(Aircraft);
		GameUnitDelegates.NotifyAircraft(1, Aircraft, true);
		return true;
	}

	if (M_AircraftMastersAliveEnemy.Contains(Aircraft))
	{
		return false;
	}
	M_AircraftMastersAliveEnemy.Add(Aircraft);
	GameUnitDelegates.NotifyAircraft(Player, Aircraft, true);
	return true;
}

bool UGameUnitManager::RemoveAircraftForPlayer(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (not Aircraft)
	{
		return false;
	}

	if (Player == 1)
	{
		if (M_AircraftMastersAlivePlayer.Remove(Aircraft) > 0)
		{
			GameUnitDelegates.NotifyAircraft(1, Aircraft, false);
			return true;
		}
		return false;
	}

	if (M_AircraftMastersAliveEnemy.Remove(Aircraft) > 0)
	{
		GameUnitDelegates.NotifyAircraft(Player, Aircraft, false);
		return true;
	}
	return false;
}

bool UGameUnitManager::AddBxpForPlayer(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (not IsValid(Bxp))
	{
		return false;
	}

	// De-dup on both sides and notify prior removal if this is effectively a move.
	if (M_BxpAliveEnemy.Remove(Bxp) > 0)
	{
		GameUnitDelegates.NotifyBxp(2, Bxp, false);
	}
	if (M_BxpAlivePlayer.Remove(Bxp) > 0)
	{
		GameUnitDelegates.NotifyBxp(1, Bxp, false);
	}

	if (Player == 1)
	{
		if (M_BxpAlivePlayer.Contains(Bxp))
		{
			return false;
		}
		M_BxpAlivePlayer.Add(Bxp);
		GameUnitDelegates.NotifyBxp(1, Bxp, true);
		return true;
	}

	if (M_BxpAliveEnemy.Contains(Bxp))
	{
		return false;
	}
	M_BxpAliveEnemy.Add(Bxp);
	GameUnitDelegates.NotifyBxp(Player, Bxp, true);
	return true;
}

bool UGameUnitManager::RemoveBxpForPlayer(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (not IsValid(Bxp))
	{
		return false;
	}

	if (Player == 1)
	{
		if (M_BxpAlivePlayer.Remove(Bxp) > 0)
		{
			GameUnitDelegates.NotifyBxp(1, Bxp, false);
			return true;
		}
		return false;
	}

	if (M_BxpAliveEnemy.Remove(Bxp) > 0)
	{
		GameUnitDelegates.NotifyBxp(Player, Bxp, false);
		return true;
	}
	return false;
}


void UGameUnitManager::AddActorForPlayer(AActor* Actor, const uint8 Player)
{
	if (not IsValid(Actor))
	{
		return;
	}

	// De-dup from both arrays and notify prior removal.
	if (M_ActorsAliveEnemy.Remove(Actor) > 0)
	{
		GameUnitDelegates.NotifyActor(2, Actor, false);
	}
	if (M_ActorsAlivePlayer.Remove(Actor) > 0)
	{
		GameUnitDelegates.NotifyActor(1, Actor, false);
	}

	if (Player == 1)
	{
		M_ActorsAlivePlayer.Add(Actor);
		GameUnitDelegates.NotifyActor(1, Actor, true);
		return;
	}

	M_ActorsAliveEnemy.Add(Actor);
	GameUnitDelegates.NotifyActor(Player, Actor, true);
}

void UGameUnitManager::RemoveActorForPlayer(AActor* Actor, const uint8 Player)
{
	if (not Actor)
	{
		return;
	}

	if (Player == 1)
	{
		if (M_ActorsAlivePlayer.Remove(Actor) > 0)
		{
			GameUnitDelegates.NotifyActor(1, Actor, false);
		}
		return;
	}

	if (M_ActorsAliveEnemy.Remove(Actor) > 0)
	{
		GameUnitDelegates.NotifyActor(Player, Actor, false);
	}
}


TArray<ATankMaster*> UGameUnitManager::GetPlayerTanks(const uint8 Player) const
{
	if (Player == 1)
	{
		return M_TankMastersAlivePlayer;
	}
	return M_TankMastersAliveEnemy;
}


AActor* UGameUnitManager::GetClosestTargetPreferSquadUnit(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector SearchLocation) const
{
	AActor* Target = nullptr;
	float Closest = MAX_FLT;

	const TArray<ASquadUnit*>*   EnemySquadUnits = &M_SquadUnitsAliveEnemy;
	const TArray<ATankMaster*>*  EnemyTanks      = &M_TankMastersAliveEnemy;
	const TArray<ABuildingExpansion*>* EnemyBxps = &M_BxpAliveEnemy;

	if (PlayerSearch != 1)
	{
		EnemySquadUnits = &M_SquadUnitAlivePlayer;
		EnemyTanks      = &M_TankMastersAlivePlayer;
		EnemyBxps       = &M_BxpAlivePlayer;
	}

	auto Consider = [&](AActor* Candidate)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Candidate)) { return; }
		const float D = FVector::Distance(SearchLocation, Candidate->GetActorLocation());
		if (D <= SearchRadius && D < Closest)
		{
			Closest = D;
			Target = Candidate;
		}
	};

	// Prefer squads
	for (ASquadUnit* U : *EnemySquadUnits) { Consider(U); }
	if (Target) { return Target; }

	// Then tanks
	for (ATankMaster* T : *EnemyTanks) { Consider(T); }
	if (Target) { return Target; }

	// NEW: Finally BXPs (buildings)
	for (ABuildingExpansion* B : *EnemyBxps) { Consider(B); }

	return Target;
}

AActor* UGameUnitManager::GetClosestTargetPreferTank(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector& SearchLocation) const
{
	AActor* Target = nullptr;
	float Closest = MAX_FLT;

	const TArray<ATankMaster*>*  EnemyTanks      = &M_TankMastersAliveEnemy;
	const TArray<ASquadUnit*>*   EnemySquads     = &M_SquadUnitsAliveEnemy;
	const TArray<ABuildingExpansion*>* EnemyBxps = &M_BxpAliveEnemy;

	if (PlayerSearch != 1)
	{
		EnemyTanks  = &M_TankMastersAlivePlayer;
		EnemySquads = &M_SquadUnitAlivePlayer;
		EnemyBxps   = &M_BxpAlivePlayer;
	}

	auto Consider = [&](AActor* Candidate)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Candidate)) { return; }
		const float D = FVector::Distance(SearchLocation, Candidate->GetActorLocation());
		if (D <= SearchRadius && D < Closest)
		{
			Closest = D;
			Target = Candidate;
		}
	};

	// Prefer tanks
	for (ATankMaster* T : *EnemyTanks) { Consider(T); }
	if (Target) { return Target; }

	// Then squads
	for (ASquadUnit* U : *EnemySquads) { Consider(U); }
	if (Target) { return Target; }

	// NEW: Finally BXPs (buildings)
	for (ABuildingExpansion* B : *EnemyBxps) { Consider(B); }

	return Target;
}

AActor* UGameUnitManager::GetClosestTarget(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector& SearchLocation) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindTarget);

	AActor* Target = nullptr;
	float Closest = MAX_FLT;

	const TArray<ASquadUnit*>*   EnemySquads = &M_SquadUnitsAliveEnemy;
	const TArray<ATankMaster*>*  EnemyTanks  = &M_TankMastersAliveEnemy;
	const TArray<AActor*>*       EnemyActors = &M_ActorsAliveEnemy;
	const TArray<ABuildingExpansion*>* EnemyBxps = &M_BxpAliveEnemy;

	if (PlayerSearch != 1)
	{
		EnemySquads = &M_SquadUnitAlivePlayer;
		EnemyTanks  = &M_TankMastersAlivePlayer;
		EnemyActors = &M_ActorsAlivePlayer;
		EnemyBxps   = &M_BxpAlivePlayer;
	}

	auto Consider = [&](AActor* Candidate)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Candidate)) { return; }
		const float D = FVector::Distance(SearchLocation, Candidate->GetActorLocation());
		if (D <= SearchRadius && D < Closest)
		{
			Closest = D;
			Target = Candidate;
		}
	};

	for (ATankMaster* T : *EnemyTanks)   { Consider(T); }
	for (ASquadUnit* U : *EnemySquads)   { Consider(U); }
	for (AActor* A : *EnemyActors)       { Consider(A); }
	// NEW: BXPs too
	for (ABuildingExpansion* B : *EnemyBxps) { Consider(B); }

	return Target;
}


AActor* UGameUnitManager::FindUnitForPlayer(const FTrainingOption& TrainingOption,
                                            const int32 OwningPlayer) const
{
	if (TrainingOption.IsNone())
	{
		RTSFunctionLibrary::ReportError("Cannot find unit of type none!"
			"\n at function GetFirstUnitOfTypeForPlayer in CPPGameState.cpp");
		return nullptr;
	}
	switch (TrainingOption.UnitType)
	{
	case EAllUnitType::UNType_None:
		return nullptr;
	case EAllUnitType::UNType_Squad:
		return FindSquadUnitOfPlayer(static_cast<ESquadSubtype>(TrainingOption.SubtypeValue), OwningPlayer);
	case EAllUnitType::UNType_Harvester:
		break;
	case EAllUnitType::UNType_Tank:
		return FindTankUnitOfPlayer(static_cast<ETankSubtype>(TrainingOption.SubtypeValue), OwningPlayer);
	case EAllUnitType::UNType_Aircraft:
		return FindAircraftUnitOfPlayer(static_cast<EAircraftSubtype>(TrainingOption.SubtypeValue), OwningPlayer);
	case EAllUnitType::UNType_Nomadic:
		return FindNomadicUnitOfPlayer(static_cast<ENomadicSubtype>(TrainingOption.SubtypeValue), OwningPlayer);
	case EAllUnitType::UNType_BuildingExpansion:
		return FindBxpUnitOfPlayer(static_cast<EBuildingExpansionType> (TrainingOption.SubtypeValue), OwningPlayer);
	default:
		RTSFunctionLibrary::ReportError("Unknown unit type in GetFirstUnitOfTypeForPlayer"
			"\n at function GetFirstUnitOfTypeForPlayer in CPPGameState.cpp");
		return nullptr;
	}
	return nullptr;
}

void UGameUnitManager::BeginPlay()
{
	Super::BeginPlay();
	Initialize();
}

void UGameUnitManager::BeginDestroy()
{
	ShutdownAsyncTargetThread();
	Super::BeginDestroy();
}


void UGameUnitManager::CleanupInvalidActorReferences()
{
	CleanupActorArray(M_ActorsAliveEnemy);
	CleanupActorArray(M_ActorsAlivePlayer);
	CleanupActorArray(M_TankMastersAliveEnemy);
	CleanupActorArray(M_TankMastersAlivePlayer);
	CleanupActorArray(M_SquadUnitAlivePlayer);
	CleanupActorArray(M_SquadUnitsAliveEnemy);

	CleanupActorArray(M_AircraftMastersAlivePlayer);
	CleanupActorArray(M_AircraftMastersAliveEnemy);
	CleanupActorArray(M_BxpAlivePlayer);
	CleanupActorArray(M_BxpAliveEnemy);
}


void UGameUnitManager::OnTargetIDsReceived(
	const TArray<uint32>& TargetIDs,
	TFunction<void(const TArray<AActor*>&)> Callback,
	int32 OwningPlayer)
{
	TArray<AActor*> Actors;
	TMap<uint32, AActor*>* CurrentActorIDMapping = OwningPlayer == 1
		                                               ? &M_EnemyActorIDToActorMap
		                                               : &M_PlayerActorIDToActorMap;
	if (not CurrentActorIDMapping)
	{
		return;
	}

	for (uint32 ActorID : TargetIDs)
	{
		if (AActor* const* ActorPtr = CurrentActorIDMapping->Find(ActorID))
		{
			if (IsValid(*ActorPtr))
			{
				Actors.Add(*ActorPtr);
			}
		}
	}

	// Invoke the callback with the valid actors
	Callback(Actors);
}


void
UGameUnitManager::GetAsyncActorData(const bool bGetPlayerUnits,
                                    TMap<uint32, TPair<ETargetPreference, FVector>>& OutActorData)
{
	TArray<ATankMaster*>* TargetTankMasters;
	TArray<ASquadUnit*>*  TargetSquadUnits;
	TArray<AActor*>*      OtherTargetActors;
	TArray<ABuildingExpansion*>* TargetBxps;
	TMap<uint32, AActor*>* CurrentActorIDMapping;

	// If we are checking player units then the enemy is player 2 (CPU)
	// If we are checking CPU units then the enemy is player 1 (Player)
	const int32 EnemyOfCheckedUnit = bGetPlayerUnits ? 2 : 1;

	if (bGetPlayerUnits)
	{
		TargetTankMasters       = &M_TankMastersAlivePlayer;
		TargetSquadUnits        = &M_SquadUnitAlivePlayer;
		OtherTargetActors       = &M_ActorsAlivePlayer;
		TargetBxps              = &M_BxpAlivePlayer;
		CurrentActorIDMapping   = &M_PlayerActorIDToActorMap;
	}
	else
	{
		TargetTankMasters       = &M_TankMastersAliveEnemy;
		TargetSquadUnits        = &M_SquadUnitsAliveEnemy;
		OtherTargetActors       = &M_ActorsAliveEnemy;
		TargetBxps              = &M_BxpAliveEnemy;
		CurrentActorIDMapping   = &M_EnemyActorIDToActorMap;
	}

	// Clear previous mapping.
	CurrentActorIDMapping->Empty();

	// Generic actors
	for (AActor* Actor : *OtherTargetActors)
	{
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(Actor, EnemyOfCheckedUnit))
		{
			continue;
		}
		AddActorData(EnemyOfCheckedUnit, Actor, CurrentActorIDMapping, OutActorData);
	}

	// Tanks
	for (ATankMaster* EachTank : *TargetTankMasters)
	{
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(EachTank, EnemyOfCheckedUnit))
		{
			continue;
		}
		const uint32 ActorID = EachTank->GetUniqueID();
		CurrentActorIDMapping->Add(ActorID, EachTank);
		OutActorData.Add({ ActorID, CreatePair(EachTank->GetActorLocation(), ETargetPreference::Tank) });
	}

	// Squads
	for (ASquadUnit* EachSquadUnit : *TargetSquadUnits)
	{
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(EachSquadUnit, EnemyOfCheckedUnit))
		{
			continue;
		}
		const uint32 ActorID = EachSquadUnit->GetUniqueID();
		CurrentActorIDMapping->Add(ActorID, EachSquadUnit);
		OutActorData.Add({ ActorID, CreatePair(EachSquadUnit->GetActorLocation(), ETargetPreference::Infantry) });
	}

	//  BXPs
	for (ABuildingExpansion* EachBxp : *TargetBxps)
	{
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(EachBxp, EnemyOfCheckedUnit))
		{
			continue;
		}
		// Route through generic AddActorData so preference can be derived (defaults to Building).
		AddActorData(EnemyOfCheckedUnit, EachBxp, CurrentActorIDMapping, OutActorData);
	}

	if (DeveloperSettings::Debugging::GAsyncTargetFinding_Compile_DebugSymbols)
	{
		const FString Owner = bGetPlayerUnits ? TEXT("Player") : TEXT("Enemy");
		RTSFunctionLibrary::PrintString(
			TEXT("Amount of ") + Owner + TEXT(" possible targetable units ") + FString::FromInt(OutActorData.Num()),
			FColor::Purple);
	}
}
void UGameUnitManager::AddActorData(const int32 EnemyOfActor,
                                    AActor* Actor,
                                    TMap<uint32, AActor*>* CurrentActorIDMapping,
                                    TMap<uint32, TPair<ETargetPreference, FVector>>& OutActorData)
{
	ETargetPreference Preference = ETargetPreference::Building;
	uint32 ActorID = Actor->GetUniqueID();
	// todo for tests only
	if (AHpPawnMaster* HpPawnMaster = Cast<AHpPawnMaster>(Actor); IsValid(HpPawnMaster))
	{
		Preference = HpPawnMaster->GetTargetPreference();
	}
	CurrentActorIDMapping->Add(ActorID, Actor);
	OutActorData.Add({
		ActorID, CreatePair(Actor->GetActorLocation(),
		                    Preference)
	});
}

AActor* UGameUnitManager::FindTankUnitOfPlayer(const ETankSubtype TankSubtype, const int32 OwningPlayer) const
{
	const TArray<ATankMaster*>* TankMasters = OwningPlayer == 1 ? &M_TankMastersAlivePlayer : &M_TankMastersAliveEnemy;
	if (not TankMasters)
	{
		return nullptr;
	}
	for (const auto& EachTank : *TankMasters)
	{
		if (not IsValid(EachTank))
		{
			continue;
		}
		if (URTSComponent* TankRTSComp = EachTank->GetRTSComponent(); IsValid(TankRTSComp))
		{
			// Tanks and nomadic vehicles are stored in the same array so we need a double check to prevent
			// overlapping subtypes from being returned.
			if (TankRTSComp->GetUnitType() == EAllUnitType::UNType_Tank && TankRTSComp->GetSubtypeAsTankSubtype() ==
				TankSubtype)
			{
				return EachTank;
			}
		}
	}
	return nullptr;
}

AActor* UGameUnitManager::FindAircraftUnitOfPlayer(const EAircraftSubtype AircraftSubtype,
                                                   const int32 OwningPlayer) const
{
	const TArray<AAircraftMaster*>* AircraftMasters = OwningPlayer == 1
		                                                  ? &M_AircraftMastersAlivePlayer
		                                                  : &M_AircraftMastersAliveEnemy;
	if (not AircraftMasters)
	{
		return nullptr;
	}
	for (const auto& EachAircraft : *AircraftMasters)
	{
		if (not IsValid(EachAircraft))
		{
			continue;
		}
		if (URTSComponent* AircraftRTSComp = EachAircraft->GetRTSComponent(); IsValid(AircraftRTSComp))
		{
			if (AircraftRTSComp->GetSubtypeAsAircraftSubtype() == AircraftSubtype)
			{
				return EachAircraft;
			}
		}
	}
	return nullptr;
}

AActor* UGameUnitManager::FindSquadUnitOfPlayer(const ESquadSubtype SquadSubtype, const int32 OwningPlayer) const
{
	const TArray<ASquadUnit*>* SquadUnits = OwningPlayer == 1 ? &M_SquadUnitAlivePlayer : &M_SquadUnitsAliveEnemy;
	if (not SquadUnits)
	{
		return nullptr;
	}
	for (const auto& EachSquad : *SquadUnits)
	{
		if (not IsValid(EachSquad))
		{
			continue;
		}
		if (URTSComponent* SquadRTSComp = EachSquad->GetRTSComponent(); IsValid(SquadRTSComp))
		{
			if (SquadRTSComp->GetSubtypeAsSquadSubtype() == SquadSubtype)
			{
				return EachSquad;
			}
		}
	}
	return nullptr;
}

AActor* UGameUnitManager::FindNomadicUnitOfPlayer(const ENomadicSubtype NomadicSubtype, const int32 OwningPlayer) const
{
	const TArray<ATankMaster*>* TankMasters = OwningPlayer == 1 ? &M_TankMastersAlivePlayer : &M_TankMastersAliveEnemy;
	if (not TankMasters)
	{
		return nullptr;
	}
	for (const auto& EachTank : *TankMasters)
	{
		if (not IsValid(EachTank))
		{
			continue;
		}
		if (URTSComponent* TankRTSComp = EachTank->GetRTSComponent(); IsValid(TankRTSComp))
		{
			// Tanks and nomadic vehicles are stored in the same array so we need a double check to prevent
			// overlapping subtypes from being returned.
			if (TankRTSComp->GetUnitType() == EAllUnitType::UNType_Nomadic &&
				TankRTSComp->GetSubtypeAsNomadicSubtype() == NomadicSubtype)
			{
				return EachTank;
			}
		}
	}
	return nullptr;
}

AActor* UGameUnitManager::FindBxpUnitOfPlayer(const EBuildingExpansionType BxpSubtype,
                                              const int32 OwningPlayer) const
{
	const TArray<ABuildingExpansion*>* Bxps = (OwningPlayer == 1) ? &M_BxpAlivePlayer : &M_BxpAliveEnemy;
	if (not Bxps)
	{
		return nullptr;
	}

	for (ABuildingExpansion* EachBxp : *Bxps)
	{
		if (not IsValid(EachBxp))
		{
			continue;
		}
		if (const URTSComponent* Rts = EachBxp->GetRTSComponent(); IsValid(Rts))
		{
			if (Rts->GetUnitType() == EAllUnitType::UNType_BuildingExpansion &&
				Rts->GetSubtypeAsBxpSubtype() == BxpSubtype)
			{
				return EachBxp;
			}
		}
	}
	return nullptr;
}


TPair<ETargetPreference, FVector> UGameUnitManager::CreatePair(const FVector& ActorLocation,
                                                               const ETargetPreference& Preference)
{
	return {Preference, ActorLocation};
}
