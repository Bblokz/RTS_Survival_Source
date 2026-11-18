// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "GameUnitManager.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"

#include "GetTargetUnitThread/GetAsyncTarget.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace GameUnitManagerDetailedState
{
	void AppendDetailedUnitState(AActor* Actor, TArray<FAsyncDetailedUnitState>& OutStates);

	template <typename TActorType>
	void AppendDetailedUnitStatesForActors(
		const TArray<TActorType*>& Actors,
		TArray<FAsyncDetailedUnitState>& OutStates)
	{
		for (TActorType* Actor : Actors)
		{
			AppendDetailedUnitState(Actor, OutStates);
		}
	}

	void AppendSquadControllers(
		const TArray<ASquadUnit*>& SquadUnits,
		TSet<ASquadController*>& OutSquadControllers)
	{
		for (ASquadUnit* SquadUnit : SquadUnits)
		{
			if (not RTSFunctionLibrary::RTSIsValid(SquadUnit))
			{
				continue;
			}
			ASquadController* SquadController = SquadUnit->GetSquadControllerChecked();
			if (not RTSFunctionLibrary::RTSIsValid(SquadController))
			{
				continue;
			}
			OutSquadControllers.Add(SquadController);
		}
	}

	bool TryGetActorComponents(AActor* Actor, URTSComponent*& OutRTSComponent, UHealthComponent*& OutHealthComponent)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Actor))
		{
			return false;
		}

		if (const AHpPawnMaster* HpPawnMaster = Cast<AHpPawnMaster>(Actor))
		{
			OutRTSComponent = HpPawnMaster->GetRTSComponent();
			OutHealthComponent = HpPawnMaster->GetHealthComponent();
		}
		else if (const AHPActorObjectsMaster* HpActorMaster = Cast<AHPActorObjectsMaster>(Actor))
		{
			OutRTSComponent = HpActorMaster->GetRTSComponent();
			OutHealthComponent = HpActorMaster->GetHealthComponent();
		}
		else
		{
			OutRTSComponent = Cast<URTSComponent>(Actor->GetComponentByClass(URTSComponent::StaticClass()));
			OutHealthComponent = Cast<UHealthComponent>(Actor->GetComponentByClass(UHealthComponent::StaticClass()));
		}

		if (not IsValid(OutRTSComponent) || not IsValid(OutHealthComponent))
		{
			return false;
		}

		return true;
	}

	bool TryBuildDetailedUnitState(AActor* Actor, FAsyncDetailedUnitState& OutState)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Actor))
		{
			return false;
		}

		URTSComponent* RTSComponent = nullptr;
		UHealthComponent* HealthComponent = nullptr;
		if (not TryGetActorComponents(Actor, RTSComponent, HealthComponent))
		{
			return false;
		}

		OutState.OwningPlayer = RTSComponent->GetOwningPlayer();
		OutState.UnitActorPtr = Actor;
		OutState.UnitType = RTSComponent->GetUnitType();
		OutState.UnitSubtypeRaw = static_cast<int32>(RTSComponent->GetUnitSubType());
		OutState.CurrentActiveAbility = EAbilityID::IdNoAbility;
		OutState.bIsInCombat = RTSComponent->GetIsUnitInCombat();
		OutState.HealthRatio = HealthComponent->GetHealthPercentage();
		OutState.UnitLocation = Actor->GetActorLocation();
		OutState.UnitRotation = Actor->GetActorRotation();

		if (ICommands* CommandsInterface = Cast<ICommands>(Actor))
		{
			OutState.CurrentActiveAbility = CommandsInterface->GetActiveCommandID();
		}

		return true;
	}

	void AppendDetailedUnitState(AActor* Actor, TArray<FAsyncDetailedUnitState>& OutStates)
	{
		FAsyncDetailedUnitState DetailedState;
		if (not TryBuildDetailedUnitState(Actor, DetailedState))
		{
			return;
		}

		OutStates.Add(MoveTemp(DetailedState));
	}
}


namespace GameUnitManagerStorage
{
	/**
	 * @brief Takes a unit out of the array that holds it, wherever that is: a unit whose owner was written
	 * without telling the manager still sits in the array of its previous owner.
	 * @param UnitsOfPlayer The manager's storage, one entry per player number.
	 * @param Unit Unit to remove.
	 * @param ExpectedPlayer Player whose array is searched first.
	 * @param ArrayOfKind Which array of FGameUnitsOfPlayer holds this kind of unit.
	 * @return Player number of the array the unit was in, or INDEX_NONE when no array held it.
	 */
	template <typename TUnit>
	int32 RemoveUnitFromPlayerArrays(
		FGameUnitsOfPlayer* UnitsOfPlayer,
		TUnit* Unit,
		const uint8 ExpectedPlayer,
		TArray<TUnit*> FGameUnitsOfPlayer::* ArrayOfKind)
	{
		const int32 ExpectedSlot = FRTSPlayers::NormalizePlayer(ExpectedPlayer);
		if ((UnitsOfPlayer[ExpectedSlot].*ArrayOfKind).Remove(Unit) > 0)
		{
			return ExpectedSlot;
		}

		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			if (Player != ExpectedSlot && (UnitsOfPlayer[Player].*ArrayOfKind).Remove(Unit) > 0)
			{
				return Player;
			}
		}

		return INDEX_NONE;
	}
}

namespace GameUnitManagerUnitTypeCounters
{
	template <typename TSubtype>
	void IncrementCounter(TMap<TSubtype, int32>& Counters, const TSubtype Subtype)
	{
		int32& Count = Counters.FindOrAdd(Subtype);
		++Count;
	}

	template <typename TSubtype>
	void DecrementCounter(
		TMap<TSubtype, int32>& Counters,
		const TSubtype Subtype,
		const TCHAR* CounterName)
	{
		int32& Count = Counters.FindOrAdd(Subtype);
		--Count;
		if (Count >= 0)
		{
			return;
		}

		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("%s became lower than zero in UGameUnitManager. A unit was removed twice or was not added before removal."),
			CounterName));
	}

	template <typename TSubtype>
	bool GetHasUnitOfType(const TMap<TSubtype, int32>& Counters, const TSubtype Subtype)
	{
		return Counters.FindRef(Subtype) > 0;
	}

	/**
	 * @brief "The enemy has one" for every AI player together.
	 * @param UnitsOfPlayer The manager's storage, one entry per player number.
	 * @param CountersOfKind Which counter map of FGameUnitsOfPlayer counts this kind of unit.
	 * @param Subtype Subtype to look for.
	 * @return True when at least one AI player has a unit of the subtype.
	 */
	template <typename TSubtype>
	bool GetHasAnyAIPlayerUnitOfType(
		const FGameUnitsOfPlayer* UnitsOfPlayer,
		TMap<TSubtype, int32> FGameUnitsOfPlayer::* CountersOfKind,
		const TSubtype Subtype)
	{
		for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			if (GetHasUnitOfType(UnitsOfPlayer[Player].*CountersOfKind, Subtype))
			{
				return true;
			}
		}
		return false;
	}
}

namespace GameUnitManagerHealthBarVisibility
{
	bool TryGetPlayerVisibilityStrategy(
		const AActor* OwnerActor,
		const URTSGameUserSettings& GameUserSettings,
		ERTSPlayerHealthBarVisibilityStrategy& OutStrategy)
	{
		if (OwnerActor == nullptr)
		{
			return false;
		}

		if (OwnerActor->IsA(ANomadicVehicle::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetPlayerNomadicHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ATankMaster::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetPlayerTankHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(AAircraftMaster::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetPlayerAircraftHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ASquadController::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetPlayerSquadHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ABuildingExpansion::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetPlayerBxpHpBarStrat();
			return true;
		}

		return false;
	}

	bool TryGetEnemyVisibilityStrategy(
		const AActor* OwnerActor,
		const URTSGameUserSettings& GameUserSettings,
		ERTSEnemyHealthBarVisibilityStrategy& OutStrategy)
	{
		if (OwnerActor == nullptr)
		{
			return false;
		}

		if (OwnerActor->IsA(ANomadicVehicle::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetEnemyNomadicHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ATankMaster::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetEnemyTankHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(AAircraftMaster::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetEnemyAircraftHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ASquadController::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetEnemySquadHpBarStrat();
			return true;
		}

		if (OwnerActor->IsA(ABuildingExpansion::StaticClass()))
		{
			OutStrategy = GameUserSettings.GetEnemyBxpHpBarStrat();
			return true;
		}

		return false;
	}

	UHealthComponent* FindHealthComponent(AActor* Actor)
	{
		URTSComponent* RTSComponent = nullptr;
		UHealthComponent* HealthComponent = nullptr;
		if (not GameUnitManagerDetailedState::TryGetActorComponents(Actor, RTSComponent, HealthComponent))
		{
			return nullptr;
		}
		return HealthComponent;
	}

	template <typename TActorType>
	void ApplyPlayerStrategyToActors(
		const TArray<TActorType*>& Actors,
		const ERTSPlayerHealthBarVisibilityStrategy Strategy)
	{
		for (TActorType* Actor : Actors)
		{
			if (UHealthComponent* HealthComponent = FindHealthComponent(Actor))
			{
				HealthComponent->OnOverwiteHealthbarVisiblityPlayer(Strategy);
			}
		}
	}

	template <typename TActorType>
	void ApplyEnemyStrategyToActors(
		const TArray<TActorType*>& Actors,
		const ERTSEnemyHealthBarVisibilityStrategy Strategy)
	{
		for (TActorType* Actor : Actors)
		{
			if (UHealthComponent* HealthComponent = FindHealthComponent(Actor))
			{
				HealthComponent->OnOverwiteHealthbarVisiblityEnemy(Strategy);
			}
		}
	}

	bool GetIsNomadicVehicle(const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsA(ANomadicVehicle::StaticClass());
	}

	// Looks for nomadic vehicles among the generic actors, as it always did; nomadic vehicles register as tanks.
	void ApplyPlayerStrategyToNomadicActors(
		const TArray<AActor*>& Actors,
		const ERTSPlayerHealthBarVisibilityStrategy Strategy)
	{
		for (AActor* Actor : Actors)
		{
			if (not GetIsNomadicVehicle(Actor))
			{
				continue;
			}
			if (UHealthComponent* HealthComponent = FindHealthComponent(Actor))
			{
				HealthComponent->OnOverwiteHealthbarVisiblityPlayer(Strategy);
			}
		}
	}

	void ApplyEnemyStrategyToNomadicActors(
		const TArray<AActor*>& Actors,
		const ERTSEnemyHealthBarVisibilityStrategy Strategy)
	{
		for (AActor* Actor : Actors)
		{
			if (not GetIsNomadicVehicle(Actor))
			{
				continue;
			}
			if (UHealthComponent* HealthComponent = FindHealthComponent(Actor))
			{
				HealthComponent->OnOverwiteHealthbarVisiblityEnemy(Strategy);
			}
		}
	}
}

namespace GameUnitManagerClosestTarget
{
	/** Keeps the closest alive candidate inside the search radius; compares squared distances. */
	struct FClosestTargetSearch
	{
		FClosestTargetSearch(const FVector& SearchLocation, const float SearchRadius)
			: M_SearchLocation(SearchLocation)
			  , M_SearchRadiusSquared(SearchRadius >= 0.f ? FMath::Square(static_cast<double>(SearchRadius)) : -1.0)
		{
		}

		void Consider(AActor* Candidate)
		{
			if (not RTSFunctionLibrary::RTSIsValid(Candidate))
			{
				return;
			}
			const double DistanceSquared = FVector::DistSquared(M_SearchLocation, Candidate->GetActorLocation());
			if (DistanceSquared <= M_SearchRadiusSquared && DistanceSquared < M_ClosestDistanceSquared)
			{
				M_ClosestDistanceSquared = DistanceSquared;
				Target = Candidate;
			}
		}

		template <typename TActorType>
		void ConsiderAll(const TArray<TActorType*>& Candidates)
		{
			for (TActorType* Candidate : Candidates)
			{
				Consider(Candidate);
			}
		}

		AActor* Target = nullptr;

	private:
		FVector M_SearchLocation;
		// Negative for a negative radius, so nothing is in range.
		double M_SearchRadiusSquared;
		double M_ClosestDistanceSquared = TNumericLimits<double>::Max();
	};

	/**
	 * @brief Lets the search look at one kind of unit of a group of players.
	 * @param Search Search that keeps the closest candidate.
	 * @param UnitsOfPlayer The manager's storage, one entry per player number.
	 * @param PlayerMask Bits of the players whose units are candidates.
	 * @param ArrayOfKind Which array of FGameUnitsOfPlayer holds the kind of unit to look at.
	 */
	template <typename TUnit>
	void ConsiderUnitsOfPlayers(
		FClosestTargetSearch& Search,
		const FGameUnitsOfPlayer* UnitsOfPlayer,
		const uint8 PlayerMask,
		TArray<TUnit*> FGameUnitsOfPlayer::* ArrayOfKind)
	{
		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			if (FRTSPlayers::IsPlayerInMask(Player, PlayerMask))
			{
				Search.ConsiderAll(UnitsOfPlayer[Player].*ArrayOfKind);
			}
		}
	}
}

namespace GameUnitManagerAsyncTargets
{
	/**
	 * @brief Hands the answer of the async thread to the requester, on the game thread.
	 * Leaves out what died since the snapshot was taken and what is no longer hostile to the requester: teams or
	 * the owner of a unit can change while a request is underway.
	 * @param Targets Units the thread found, closest first.
	 * @param Callback The original callback function provided by the requester; called also without targets.
	 * @param OwningPlayer Player that made the request.
	 */
	void DeliverTargets(
		const FAsyncTargetResult& Targets,
		const TFunction<void(const TArray<AActor*>&)>& Callback,
		const int32 OwningPlayer)
	{
		TArray<AActor*> Actors;
		Actors.Reserve(Targets.Num());
		for (const TWeakObjectPtr<AActor>& Target : Targets)
		{
			AActor* Actor = Target.Get();
			if (not RTSFunctionLibrary::RTSIsValid(Actor))
			{
				continue;
			}
			if (not FRTSPlayers::IsActorHostileToPlayer(Actor, OwningPlayer))
			{
				continue;
			}
			Actors.Add(Actor);
		}

		// Invoke the callback with the valid actors
		Callback(Actors);
	}

	/**
	 * @brief Adds a unit to the snapshot when it is alive and at least one player that fights its owner sees it.
	 * @param Actor Registered unit; may be invalid or dead.
	 * @param OwnerPlayer Player number of the array the unit is registered in.
	 * @param TargetType What kind of target weapons see in it.
	 * @param OutSnapshot Snapshot to append to; its per-viewer owner bits are kept up to date.
	 */
	void AddTargetUnit(
		AActor* Actor,
		const int32 OwnerPlayer,
		const ETargetPreference TargetType,
		FAsyncTargetSnapshot& OutSnapshot)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Actor))
		{
			return;
		}
		// Only players that fight the owner can target the unit; a two-player game keeps the snapshot it always
		// had (no fog-hidden AI unit in it). A player whose first unit registers sees it from the next snapshot on.
		const uint8 VisibleToMask = RTSFunctionLibrary::RTSGetVisibleToMask(Actor, OwnerPlayer)
			& FRTSPlayers::GetActiveHostileMask(OwnerPlayer);
		if (VisibleToMask == 0)
		{
			return;
		}

		const uint8 OwnerBit = FRTSPlayers::GetPlayerBit(OwnerPlayer);
		FAsyncTargetUnit& Unit = OutSnapshot.Units.AddDefaulted_GetRef();
		Unit.Location = Actor->GetActorLocation();
		Unit.Actor = Actor;
		Unit.TargetType = TargetType;
		Unit.OwnerBit = OwnerBit;
		Unit.VisibleToMask = VisibleToMask;

		for (int32 Viewer = RTSPlayers::HumanPlayer; Viewer <= RTSPlayers::MaxPlayers; ++Viewer)
		{
			if (FRTSPlayers::IsPlayerInMask(Viewer, VisibleToMask))
			{
				OutSnapshot.VisibleOwnersOfViewer[Viewer] |= OwnerBit;
			}
		}
	}

	template <typename TActorType>
	void AddTargetUnits(
		const TArray<TActorType*>& Actors,
		const int32 OwnerPlayer,
		const ETargetPreference TargetType,
		FAsyncTargetSnapshot& OutSnapshot)
	{
		for (TActorType* Actor : Actors)
		{
			AddTargetUnit(Actor, OwnerPlayer, TargetType, OutSnapshot);
		}
	}
}

UGameUnitManager::UGameUnitManager()
	: M_AsyncTargetProcessor(nullptr)
	  , M_ActorDataUpdateInterval(DeveloperSettings::Async::GameThreadUpdateAsyncWithActorTargetsInterval)
	  , M_DetailedActorDataUpdateInterval(DeveloperSettings::Async::GameThreadUpdateAsyncWithAllDetailedActorDataInterval)
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
	auto DisableOptimizationOnActor = [&DisableOptimization](AActor* Actor)
	{
		if (not Actor)
		{
			return;
		}
		UActorComponent* Component = Actor->GetComponentByClass(URTSOptimizer::StaticClass());
		DisableOptimization(Component);
	};
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		for (ATankMaster* EachTank : UnitsOfPlayer.TankMasters)
		{
			DisableOptimizationOnActor(EachTank);
		}
		for (ASquadUnit* EachSquadUnit : UnitsOfPlayer.SquadUnits)
		{
			DisableOptimizationOnActor(EachSquadUnit);
		}
	}
}

bool UGameUnitManager::TryApplyHealthBarVisibilityFromUserSettings(UHealthComponent* HealthComponent)
{
	if (not IsValid(HealthComponent))
	{
		return false;
	}

	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	if (GameUserSettings == nullptr)
	{
		return false;
	}

	AActor* const OwnerActor = HealthComponent->GetOwner();
	if (not IsValid(OwnerActor))
	{
		return false;
	}

	URTSComponent* const RTSComponent = Cast<URTSComponent>(
		OwnerActor->GetComponentByClass(URTSComponent::StaticClass()));
	if (RTSComponent == nullptr)
	{
		return false;
	}

	// The player settings are for the units the human commands, the enemy settings for those of every AI player.
	const bool bIsPlayerUnit = FRTSPlayers::IsHumanPlayer(RTSComponent->GetOwningPlayer());
	if (bIsPlayerUnit)
	{
		ERTSPlayerHealthBarVisibilityStrategy Strategy = ERTSPlayerHealthBarVisibilityStrategy::NotInitialized;
		if (not GameUnitManagerHealthBarVisibility::TryGetPlayerVisibilityStrategy(
			OwnerActor,
			*GameUserSettings,
			Strategy))
		{
			return false;
		}

		if (Strategy == ERTSPlayerHealthBarVisibilityStrategy::NotInitialized ||
			Strategy == ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults)
		{
			return false;
		}

		HealthComponent->OnOverwiteHealthbarVisiblityPlayer(Strategy);
		return true;
	}

	ERTSEnemyHealthBarVisibilityStrategy EnemyStrategy = ERTSEnemyHealthBarVisibilityStrategy::NotInitialized;
	if (not GameUnitManagerHealthBarVisibility::TryGetEnemyVisibilityStrategy(
		OwnerActor,
		*GameUserSettings,
		EnemyStrategy))
	{
		return false;
	}

	if (EnemyStrategy == ERTSEnemyHealthBarVisibilityStrategy::NotInitialized ||
		EnemyStrategy == ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults)
	{
		return false;
	}

	HealthComponent->OnOverwiteHealthbarVisiblityEnemy(EnemyStrategy);
	return true;
}

void UGameUnitManager::ApplyPlayerTankHealthBarVisibilityStrategy(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	GameUnitManagerHealthBarVisibility::ApplyPlayerStrategyToActors(
		GetUnitsOfPlayer(RTSPlayers::HumanPlayer).TankMasters, Strategy);
}

void UGameUnitManager::ApplyPlayerSquadHealthBarVisibilityStrategy(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	const TSet<ASquadController*> SquadControllers = GatherSquadControllersOfAllPlayers();

	for (ASquadController* SquadController : SquadControllers)
	{
		URTSComponent* RTSComponent = nullptr;
		UHealthComponent* HealthComponent = nullptr;
		if (not GameUnitManagerDetailedState::TryGetActorComponents(SquadController, RTSComponent, HealthComponent))
		{
			continue;
		}

		if (not FRTSPlayers::IsHumanPlayer(RTSComponent->GetOwningPlayer()))
		{
			continue;
		}

		HealthComponent->OnOverwiteHealthbarVisiblityPlayer(Strategy);
	}
}

void UGameUnitManager::ApplyPlayerNomadicHealthBarVisibilityStrategy(
	const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	GameUnitManagerHealthBarVisibility::ApplyPlayerStrategyToNomadicActors(
		GetUnitsOfPlayer(RTSPlayers::HumanPlayer).Actors, Strategy);
}

void UGameUnitManager::ApplyPlayerBxpHealthBarVisibilityStrategy(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	GameUnitManagerHealthBarVisibility::ApplyPlayerStrategyToActors(
		GetUnitsOfPlayer(RTSPlayers::HumanPlayer).Bxps, Strategy);
}

void UGameUnitManager::ApplyPlayerAircraftHealthBarVisibilityStrategy(
	const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	GameUnitManagerHealthBarVisibility::ApplyPlayerStrategyToActors(
		GetUnitsOfPlayer(RTSPlayers::HumanPlayer).AircraftMasters, Strategy);
}

// The "enemy" settings below cover every unit the human cannot select: the units of all AI players, also of one
// that is allied with the human.

void UGameUnitManager::ApplyEnemyTankHealthBarVisibilityStrategy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		GameUnitManagerHealthBarVisibility::ApplyEnemyStrategyToActors(M_UnitsOfPlayer[Player].TankMasters, Strategy);
	}
}

void UGameUnitManager::ApplyEnemySquadHealthBarVisibilityStrategy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	const TSet<ASquadController*> SquadControllers = GatherSquadControllersOfAllPlayers();

	for (ASquadController* SquadController : SquadControllers)
	{
		URTSComponent* RTSComponent = nullptr;
		UHealthComponent* HealthComponent = nullptr;
		if (not GameUnitManagerDetailedState::TryGetActorComponents(SquadController, RTSComponent, HealthComponent))
		{
			continue;
		}

		if (FRTSPlayers::IsHumanPlayer(RTSComponent->GetOwningPlayer()))
		{
			continue;
		}

		HealthComponent->OnOverwiteHealthbarVisiblityEnemy(Strategy);
	}
}

void UGameUnitManager::ApplyEnemyNomadicHealthBarVisibilityStrategy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		GameUnitManagerHealthBarVisibility::ApplyEnemyStrategyToNomadicActors(M_UnitsOfPlayer[Player].Actors, Strategy);
	}
}

void UGameUnitManager::ApplyEnemyBxpHealthBarVisibilityStrategy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		GameUnitManagerHealthBarVisibility::ApplyEnemyStrategyToActors(M_UnitsOfPlayer[Player].Bxps, Strategy);
	}
}

void UGameUnitManager::ApplyEnemyAircraftHealthBarVisibilityStrategy(
	const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		GameUnitManagerHealthBarVisibility::ApplyEnemyStrategyToActors(
			M_UnitsOfPlayer[Player].AircraftMasters, Strategy);
	}
}

TSet<ASquadController*> UGameUnitManager::GatherSquadControllersOfAllPlayers() const
{
	TSet<ASquadController*> SquadControllers;
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		GameUnitManagerDetailedState::AppendSquadControllers(UnitsOfPlayer.SquadUnits, SquadControllers);
	}
	return SquadControllers;
}

FGameUnitsOfPlayer& UGameUnitManager::GetUnitsOfPlayer(const uint8 Player)
{
	return M_UnitsOfPlayer[FRTSPlayers::NormalizePlayer(Player)];
}

const FGameUnitsOfPlayer& UGameUnitManager::GetUnitsOfPlayer(const uint8 Player) const
{
	return M_UnitsOfPlayer[FRTSPlayers::NormalizePlayer(Player)];
}

void UGameUnitManager::Initialize()
{
	// Create the async target processor and start the thread
	if (not M_AsyncTargetProcessor)
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
				M_DetailedActorDataUpdateTimerHandle,
				this,
				&UGameUnitManager::UpdateDetailedActorData,
				M_DetailedActorDataUpdateInterval,
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
		World->GetTimerManager().ClearTimer(M_DetailedActorDataUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(M_CleanUpInvalidRefsTimerHandle);
	}
}

void UGameUnitManager::UpdateActorData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateActorData);

	// Obtain the snapshot where the target type is set together with the location, the owner and the players
	// that see the unit; the unit itself is identified with a weak pointer.
	FAsyncTargetSnapshot Snapshot;
	BuildAsyncTargetSnapshot(Snapshot);

	// safe logs inside UpdateActorData:
	auto SafeName = [](AActor* In) -> FString
	{
		return IsValid(In) ? In->GetName() : TEXT("<invalid>");
	};

	if constexpr (DeveloperSettings::Debugging::GUnitManager_Compile_DebugSymbols)
	{
		for (const FAsyncTargetUnit& Unit : Snapshot.Units)
		{
			const bool bIsPlayerUnit = Unit.OwnerBit == RTSPlayers::HumanPlayerBit;
			const FString Prefix = bIsPlayerUnit
				                       ? TEXT("Player Actor game state: ")
				                       : TEXT("Enemy Actor game state: ");
			RTSFunctionLibrary::PrintString(Prefix + SafeName(Unit.Actor.Get()),
			                                bIsPlayerUnit ? FColor::Green : FColor::Red);
		}
	}

	// Schedule the actor data update on the async thread
	if (M_AsyncTargetProcessor)
	{
		M_AsyncTargetProcessor->ScheduleUpdateActorData(MoveTemp(Snapshot));
	}
}

void UGameUnitManager::UpdateDetailedActorData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateDetailedActorData);

	if (not M_AsyncTargetProcessor)
	{
		return;
	}

	TArray<FAsyncDetailedUnitState> DetailedUnitStates;
	int32 EstimatedSize = 0;
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		EstimatedSize += UnitsOfPlayer.TankMasters.Num()
			+ UnitsOfPlayer.AircraftMasters.Num()
			+ UnitsOfPlayer.Bxps.Num()
			+ UnitsOfPlayer.Actors.Num()
			+ UnitsOfPlayer.SquadUnits.Num();
	}
	DetailedUnitStates.Reserve(EstimatedSize);

	// Per kind of unit first and per player second: the order the strategic AI helpers have always received.
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		GameUnitManagerDetailedState::AppendDetailedUnitStatesForActors(UnitsOfPlayer.TankMasters, DetailedUnitStates);
	}
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		GameUnitManagerDetailedState::AppendDetailedUnitStatesForActors(
			UnitsOfPlayer.AircraftMasters, DetailedUnitStates);
	}
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		GameUnitManagerDetailedState::AppendDetailedUnitStatesForActors(UnitsOfPlayer.Bxps, DetailedUnitStates);
	}
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		GameUnitManagerDetailedState::AppendDetailedUnitStatesForActors(UnitsOfPlayer.Actors, DetailedUnitStates);
	}

	const TSet<ASquadController*> SquadControllers = GatherSquadControllersOfAllPlayers();
	for (ASquadController* SquadController : SquadControllers)
	{
		GameUnitManagerDetailedState::AppendDetailedUnitState(SquadController, DetailedUnitStates);
	}

	M_AsyncTargetProcessor->ScheduleUpdateDetailedActorData(MoveTemp(DetailedUnitStates));
}

void UGameUnitManager::RequestClosestTargets(
	const FVector& SearchLocation,
	float SearchRadius,
	int32 NumTargets,
	int32 OwningPlayer,
	ETargetPreference PrefferedTarget,
	TFunction<void(const TArray<AActor*>&)> Callback,
	const float MinimumSearchRadius)
{
	if (not M_AsyncTargetProcessor)
	{
		// If the async processor is not available, invoke the callback with an empty array
		Callback(TArray<AActor*>());
		return;
	}

	auto WeakThis = TWeakObjectPtr<UGameUnitManager>(this);

	FAsyncTargetRequest Request;
	Request.SearchLocation = SearchLocation;
	Request.SearchRadius = SearchRadius;
	Request.MinimumSearchRadius = MinimumSearchRadius;
	Request.NumTargets = NumTargets;
	Request.TargetPreference = PrefferedTarget;
	// Who asks and who that player fights are copied in here: the thread must not read the relations, which
	// can change while the request waits.
	Request.RequestingPlayer = static_cast<uint8>(FRTSPlayers::NormalizePlayer(OwningPlayer));
	Request.RequesterBit = FRTSPlayers::GetPlayerBit(OwningPlayer);
	Request.HostileMask = FRTSPlayers::GetHostileMask(OwningPlayer);
	Request.Callback = [WeakThis, Callback = MoveTemp(Callback), OwningPlayer](const FAsyncTargetResult& Targets)
	{
		if (WeakThis.IsValid())
		{
			GameUnitManagerAsyncTargets::DeliverTargets(Targets, Callback, OwningPlayer);
		}
	};

	// Forward the request to the async thread
	M_AsyncTargetProcessor->AddTargetRequest(MoveTemp(Request));
}

void UGameUnitManager::RequestStrategicAIRequests(
	const FStrategicAIRequestBatch& RequestBatch,
	TFunction<void(const FStrategicAIResultBatch&)> Callback)
{
	if (M_AsyncTargetProcessor)
	{
		M_AsyncTargetProcessor->AddStrategicAIRequest(RequestBatch, MoveTemp(Callback));
		return;
	}

	Callback(FStrategicAIResultBatch());
}


TArray<ASquadUnit*> UGameUnitManager::GetSquadUnitsOfPlayer(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).SquadUnits;
}

TArray<ASquadUnit*> UGameUnitManager::GetSquadUnitsOfPlayers(const uint8 PlayerMask) const
{
	TArray<ASquadUnit*> SquadUnits;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (FRTSPlayers::IsPlayerInMask(Player, PlayerMask))
		{
			SquadUnits.Append(M_UnitsOfPlayer[Player].SquadUnits);
		}
	}
	return SquadUnits;
}

TArray<ATankMaster*> UGameUnitManager::GetTanksOfPlayers(const uint8 PlayerMask) const
{
	TArray<ATankMaster*> Tanks;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (FRTSPlayers::IsPlayerInMask(Player, PlayerMask))
		{
			Tanks.Append(M_UnitsOfPlayer[Player].TankMasters);
		}
	}
	return Tanks;
}

TConstArrayView<ASquadUnit*> UGameUnitManager::GetSquadUnitsOfPlayerView(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).SquadUnits;
}

TConstArrayView<ATankMaster*> UGameUnitManager::GetTanksOfPlayerView(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).TankMasters;
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

	// De-dup from the array of whichever player held the unit first, and notify the removal if this was a move.
	const int32 PreviousPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, SquadUnit, Player, &FGameUnitsOfPlayer::SquadUnits);
	if (PreviousPlayer != INDEX_NONE)
	{
		DecrementSquadCounter(SquadUnit, static_cast<uint8>(PreviousPlayer));
		GameUnitDelegates.NotifySquad(PreviousPlayer, SquadUnit, false);
	}

	const uint8 PlayerSlot = static_cast<uint8>(FRTSPlayers::NormalizePlayer(Player));
	// The neutral owner has no units.
	if (FRTSPlayers::IsNeutralOwner(PlayerSlot))
	{
		return;
	}

	M_UnitsOfPlayer[PlayerSlot].SquadUnits.Add(SquadUnit);
	IncrementSquadCounter(SquadUnit, PlayerSlot);
	FRTSPlayers::MarkPlayerActive(PlayerSlot);
	GameUnitDelegates.NotifySquad(PlayerSlot, SquadUnit, true);
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

	const int32 RemovedFromPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, SquadUnit, Player, &FGameUnitsOfPlayer::SquadUnits);
	// Also when no array held the unit: the counter knows by itself whether it still counts the unit.
	DecrementSquadCounter(SquadUnit, Player);
	if (RemovedFromPlayer != INDEX_NONE)
	{
		GameUnitDelegates.NotifySquad(RemovedFromPlayer, SquadUnit, false);
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
	const int32 PreviousPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Tank, Player, &FGameUnitsOfPlayer::TankMasters);
	if (PreviousPlayer != INDEX_NONE)
	{
		DecrementTankOrNomadicCounter(Tank, static_cast<uint8>(PreviousPlayer));
		GameUnitDelegates.NotifyTank(PreviousPlayer, Tank, false);
	}

	const uint8 PlayerSlot = static_cast<uint8>(FRTSPlayers::NormalizePlayer(Player));
	// The neutral owner has no units.
	if (FRTSPlayers::IsNeutralOwner(PlayerSlot))
	{
		return;
	}

	M_UnitsOfPlayer[PlayerSlot].TankMasters.Add(Tank);
	IncrementTankOrNomadicCounter(Tank, PlayerSlot);
	FRTSPlayers::MarkPlayerActive(PlayerSlot);
	GameUnitDelegates.NotifyTank(PlayerSlot, Tank, true);
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

	const int32 RemovedFromPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Tank, Player, &FGameUnitsOfPlayer::TankMasters);
	// Also when no array held the tank: the counter knows by itself whether it still counts the tank.
	DecrementTankOrNomadicCounter(Tank, Player);
	if (RemovedFromPlayer != INDEX_NONE)
	{
		GameUnitDelegates.NotifyTank(RemovedFromPlayer, Tank, false);
	}
}

bool UGameUnitManager::AddAircraftForPlayer(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (not IsValid(Aircraft))
	{
		return false;
	}

	// De-dup from the array that held the aircraft and notify prior removal if needed.
	const int32 PreviousPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Aircraft, Player, &FGameUnitsOfPlayer::AircraftMasters);
	if (PreviousPlayer != INDEX_NONE)
	{
		DecrementAircraftCounter(Aircraft, static_cast<uint8>(PreviousPlayer));
		GameUnitDelegates.NotifyAircraft(PreviousPlayer, Aircraft, false);
	}

	const uint8 PlayerSlot = static_cast<uint8>(FRTSPlayers::NormalizePlayer(Player));
	// The neutral owner has no units.
	if (FRTSPlayers::IsNeutralOwner(PlayerSlot))
	{
		return false;
	}

	M_UnitsOfPlayer[PlayerSlot].AircraftMasters.Add(Aircraft);
	IncrementAircraftCounter(Aircraft, PlayerSlot);
	FRTSPlayers::MarkPlayerActive(PlayerSlot);
	GameUnitDelegates.NotifyAircraft(PlayerSlot, Aircraft, true);
	return true;
}

bool UGameUnitManager::RemoveAircraftForPlayer(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (not Aircraft)
	{
		return false;
	}

	const int32 RemovedFromPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Aircraft, Player, &FGameUnitsOfPlayer::AircraftMasters);
	DecrementAircraftCounter(Aircraft, Player);
	if (RemovedFromPlayer == INDEX_NONE)
	{
		return false;
	}
	GameUnitDelegates.NotifyAircraft(RemovedFromPlayer, Aircraft, false);
	return true;
}

bool UGameUnitManager::AddBxpForPlayer(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (not IsValid(Bxp))
	{
		return false;
	}

	// De-dup and notify prior removal if this is effectively a move.
	const int32 PreviousPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Bxp, Player, &FGameUnitsOfPlayer::Bxps);
	if (PreviousPlayer != INDEX_NONE)
	{
		DecrementBxpCounter(Bxp, static_cast<uint8>(PreviousPlayer));
		GameUnitDelegates.NotifyBxp(PreviousPlayer, Bxp, false);
	}

	const uint8 PlayerSlot = static_cast<uint8>(FRTSPlayers::NormalizePlayer(Player));
	// The neutral owner has no units.
	if (FRTSPlayers::IsNeutralOwner(PlayerSlot))
	{
		return false;
	}

	M_UnitsOfPlayer[PlayerSlot].Bxps.Add(Bxp);
	IncrementBxpCounter(Bxp, PlayerSlot);
	FRTSPlayers::MarkPlayerActive(PlayerSlot);
	GameUnitDelegates.NotifyBxp(PlayerSlot, Bxp, true);
	return true;
}

bool UGameUnitManager::RemoveBxpForPlayer(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (not IsValid(Bxp))
	{
		return false;
	}

	const int32 RemovedFromPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Bxp, Player, &FGameUnitsOfPlayer::Bxps);
	DecrementBxpCounter(Bxp, Player);
	if (RemovedFromPlayer == INDEX_NONE)
	{
		return false;
	}
	GameUnitDelegates.NotifyBxp(RemovedFromPlayer, Bxp, false);
	return true;
}


void UGameUnitManager::AddActorForPlayer(AActor* Actor, const uint8 Player)
{
	if (not IsValid(Actor))
	{
		return;
	}

	// De-dup from the array that held the actor and notify prior removal.
	const int32 PreviousPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Actor, Player, &FGameUnitsOfPlayer::Actors);
	if (PreviousPlayer != INDEX_NONE)
	{
		GameUnitDelegates.NotifyActor(PreviousPlayer, Actor, false);
	}

	const uint8 PlayerSlot = static_cast<uint8>(FRTSPlayers::NormalizePlayer(Player));
	// The neutral owner has no units.
	if (FRTSPlayers::IsNeutralOwner(PlayerSlot))
	{
		return;
	}

	M_UnitsOfPlayer[PlayerSlot].Actors.Add(Actor);
	FRTSPlayers::MarkPlayerActive(PlayerSlot);
	GameUnitDelegates.NotifyActor(PlayerSlot, Actor, true);
}

void UGameUnitManager::RemoveActorForPlayer(AActor* Actor, const uint8 Player)
{
	if (not Actor)
	{
		return;
	}

	const int32 RemovedFromPlayer = GameUnitManagerStorage::RemoveUnitFromPlayerArrays(
		M_UnitsOfPlayer, Actor, Player, &FGameUnitsOfPlayer::Actors);
	if (RemovedFromPlayer != INDEX_NONE)
	{
		GameUnitDelegates.NotifyActor(RemovedFromPlayer, Actor, false);
	}
}


TArray<ATankMaster*> UGameUnitManager::GetPlayerTanks(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).TankMasters;
}

#if RTS_WITH_SHIPPING_MAP_TESTS || RTS_WITH_MAP_TESTS
TArray<AAircraftMaster*> UGameUnitManager::ShippingTest_GetAircraftOfPlayer(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).AircraftMasters;
}

TArray<ABuildingExpansion*> UGameUnitManager::MapTest_GetBxpsOfPlayer(const uint8 Player) const
{
	return GetUnitsOfPlayer(Player).Bxps;
}
#endif



void UGameUnitManager::IncrementTankOrNomadicCounter(ATankMaster* Tank, const uint8 Player)
{
	if (not IsValid(Tank))
	{
		return;
	}

	const URTSComponent* const RTSComponent = Tank->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportError("Tank has no valid RTSComponent in IncrementTankOrNomadicCounter.");
		return;
	}

	// Counted once: a vehicle that is registered again first gives up what it was counted as.
	DecrementTankOrNomadicCounter(Tank, Player);
	FCountedUnitSubtype CountedVehicle;
	CountedVehicle.bIsNomadic = RTSComponent->GetUnitType() == EAllUnitType::UNType_Nomadic;
	CountedVehicle.Subtype = RTSComponent->GetUnitSubType();
	CountedVehicle.Player = Player;
	if (CountedVehicle.bIsNomadic)
	{
		TMap<ENomadicSubtype, int32>& Counters = GetUnitsOfPlayer(Player).NomadicCountByType;
		GameUnitManagerUnitTypeCounters::IncrementCounter(Counters, RTSComponent->GetSubtypeAsNomadicSubtype());
	}
	else
	{
		TMap<ETankSubtype, int32>& Counters = GetUnitsOfPlayer(Player).TankCountByType;
		GameUnitManagerUnitTypeCounters::IncrementCounter(Counters, RTSComponent->GetSubtypeAsTankSubtype());
	}
	M_CountedTanksOrNomadics.Add(Tank, CountedVehicle);
}

void UGameUnitManager::DecrementTankOrNomadicCounter(ATankMaster* Tank, const uint8 Player)
{
	if (Tank == nullptr)
	{
		return;
	}

	// Taken off the counter it was put on; a vehicle that was never counted, or was already taken off, has
	// nothing to take off.
	FCountedUnitSubtype CountedVehicle;
	if (not M_CountedTanksOrNomadics.RemoveAndCopyValue(Tank, CountedVehicle))
	{
		return;
	}

	if (CountedVehicle.bIsNomadic)
	{
		TMap<ENomadicSubtype, int32>& Counters = GetUnitsOfPlayer(CountedVehicle.Player).NomadicCountByType;
		GameUnitManagerUnitTypeCounters::DecrementCounter(
			Counters, static_cast<ENomadicSubtype>(CountedVehicle.Subtype), TEXT("Nomadic subtype counter"));
		return;
	}

	TMap<ETankSubtype, int32>& Counters = GetUnitsOfPlayer(CountedVehicle.Player).TankCountByType;
	GameUnitManagerUnitTypeCounters::DecrementCounter(
		Counters, static_cast<ETankSubtype>(CountedVehicle.Subtype), TEXT("Tank subtype counter"));
}

void UGameUnitManager::RecountTankOrNomadicSubtype(ATankMaster* Tank)
{
	if (not IsValid(Tank) || not IsValid(Tank->GetRTSComponent()))
	{
		return;
	}

	const URTSComponent* const RTSComponent = Tank->GetRTSComponent();
	const FCountedUnitSubtype* const CountedVehicle = M_CountedTanksOrNomadics.Find(Tank);
	// Not registered yet: it is counted under its subtype when it registers.
	if (CountedVehicle == nullptr)
	{
		return;
	}
	const bool bIsNomadic = RTSComponent->GetUnitType() == EAllUnitType::UNType_Nomadic;
	if (CountedVehicle->bIsNomadic == bIsNomadic && CountedVehicle->Subtype == RTSComponent->GetUnitSubType())
	{
		return;
	}

	// Copied first: counting the vehicle again removes the map entry this pointer points into.
	const uint8 CountedPlayer = CountedVehicle->Player;
	IncrementTankOrNomadicCounter(Tank, CountedPlayer);
}

void UGameUnitManager::RecountUnitSubtype(AActor* Unit)
{
	if (ATankMaster* Tank = Cast<ATankMaster>(Unit))
	{
		RecountTankOrNomadicSubtype(Tank);
		return;
	}
	if (AAircraftMaster* Aircraft = Cast<AAircraftMaster>(Unit))
	{
		RecountAircraftSubtype(Aircraft);
		return;
	}
	if (ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(Unit))
	{
		RecountBxpSubtype(Bxp);
	}
}

void UGameUnitManager::RecountAircraftSubtype(AAircraftMaster* Aircraft)
{
	if (not IsValid(Aircraft) || not IsValid(Aircraft->GetRTSComponent()))
	{
		return;
	}

	const FCountedUnitSubtype* const CountedAircraft = M_CountedAircraft.Find(Aircraft);
	// Not registered yet: it is counted under its subtype when it registers.
	if (CountedAircraft == nullptr || CountedAircraft->Subtype == Aircraft->GetRTSComponent()->GetUnitSubType())
	{
		return;
	}

	// Copied first: counting the aircraft again removes the map entry this pointer points into.
	const uint8 CountedPlayer = CountedAircraft->Player;
	IncrementAircraftCounter(Aircraft, CountedPlayer);
}

void UGameUnitManager::RecountBxpSubtype(ABuildingExpansion* Bxp)
{
	if (not IsValid(Bxp) || not IsValid(Bxp->GetRTSComponent()))
	{
		return;
	}

	const FCountedUnitSubtype* const CountedBxp = M_CountedBxps.Find(Bxp);
	// Not registered yet: it is counted under its subtype when it registers.
	if (CountedBxp == nullptr || CountedBxp->Subtype == Bxp->GetRTSComponent()->GetUnitSubType())
	{
		return;
	}

	// Copied first: counting the expansion again removes the map entry this pointer points into.
	const uint8 CountedPlayer = CountedBxp->Player;
	IncrementBxpCounter(Bxp, CountedPlayer);
}

void UGameUnitManager::IncrementSquadCounter(ASquadUnit* SquadUnit, const uint8 Player)
{
	if (not IsValid(SquadUnit) || not IsValid(SquadUnit->GetRTSComponent()))
	{
		return;
	}

	// Counted once: a soldier that is registered again first gives up what it was counted as.
	DecrementSquadCounter(SquadUnit, Player);
	const ESquadSubtype CountedSubtype = SquadUnit->GetRTSComponent()->GetSubtypeAsSquadSubtype();
	TMap<ESquadSubtype, int32>& Counters = GetUnitsOfPlayer(Player).SquadCountByType;
	GameUnitManagerUnitTypeCounters::IncrementCounter(Counters, CountedSubtype);
	M_CountedSquadUnits.Add(SquadUnit, FCountedSquadUnit{CountedSubtype, Player});
}

void UGameUnitManager::RecountSquadUnitSubtype(ASquadUnit* SquadUnit)
{
	if (not IsValid(SquadUnit) || not IsValid(SquadUnit->GetRTSComponent()))
	{
		return;
	}
	const FCountedSquadUnit* CountedUnit = M_CountedSquadUnits.Find(SquadUnit);
	// Not registered yet: it is counted under its subtype when it registers.
	if (CountedUnit == nullptr || CountedUnit->Subtype == SquadUnit->GetRTSComponent()->GetSubtypeAsSquadSubtype())
	{
		return;
	}
	IncrementSquadCounter(SquadUnit, CountedUnit->Player);
}

void UGameUnitManager::DecrementSquadCounter(ASquadUnit* SquadUnit, const uint8 Player)
{
	if (not IsValid(SquadUnit) || not IsValid(SquadUnit->GetRTSComponent()))
	{
		return;
	}

	// Taken off the counter it was put on; a soldier that was never counted has nothing to take off.
	FCountedSquadUnit CountedUnit;
	if (not M_CountedSquadUnits.RemoveAndCopyValue(SquadUnit, CountedUnit))
	{
		return;
	}
	TMap<ESquadSubtype, int32>& Counters = GetUnitsOfPlayer(CountedUnit.Player).SquadCountByType;
	GameUnitManagerUnitTypeCounters::DecrementCounter(Counters, CountedUnit.Subtype, TEXT("Squad subtype counter"));
}

void UGameUnitManager::IncrementBxpCounter(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (not IsValid(Bxp) || not IsValid(Bxp->GetRTSComponent()))
	{
		return;
	}

	// Counted once: an expansion that is registered again first gives up what it was counted as.
	DecrementBxpCounter(Bxp, Player);
	const EBuildingExpansionType CountedSubtype = Bxp->GetRTSComponent()->GetSubtypeAsBxpSubtype();
	TMap<EBuildingExpansionType, int32>& Counters = GetUnitsOfPlayer(Player).BxpCountByType;
	GameUnitManagerUnitTypeCounters::IncrementCounter(Counters, CountedSubtype);
	FCountedUnitSubtype CountedBxp;
	CountedBxp.Subtype = static_cast<uint8>(CountedSubtype);
	CountedBxp.Player = Player;
	M_CountedBxps.Add(Bxp, CountedBxp);
}

void UGameUnitManager::DecrementBxpCounter(ABuildingExpansion* Bxp, const uint8 Player)
{
	if (Bxp == nullptr)
	{
		return;
	}

	// Taken off the counter it was put on; an expansion that was never counted, or was already taken off, has
	// nothing to take off.
	FCountedUnitSubtype CountedBxp;
	if (not M_CountedBxps.RemoveAndCopyValue(Bxp, CountedBxp))
	{
		return;
	}
	TMap<EBuildingExpansionType, int32>& Counters = GetUnitsOfPlayer(CountedBxp.Player).BxpCountByType;
	GameUnitManagerUnitTypeCounters::DecrementCounter(
		Counters, static_cast<EBuildingExpansionType>(CountedBxp.Subtype), TEXT("BXP subtype counter"));
}

void UGameUnitManager::IncrementAircraftCounter(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (not IsValid(Aircraft) || not IsValid(Aircraft->GetRTSComponent()))
	{
		return;
	}

	// Counted once: an aircraft that is registered again first gives up what it was counted as.
	DecrementAircraftCounter(Aircraft, Player);
	const EAircraftSubtype CountedSubtype = Aircraft->GetRTSComponent()->GetSubtypeAsAircraftSubtype();
	TMap<EAircraftSubtype, int32>& Counters = GetUnitsOfPlayer(Player).AircraftCountByType;
	GameUnitManagerUnitTypeCounters::IncrementCounter(Counters, CountedSubtype);
	FCountedUnitSubtype CountedAircraft;
	CountedAircraft.Subtype = static_cast<uint8>(CountedSubtype);
	CountedAircraft.Player = Player;
	M_CountedAircraft.Add(Aircraft, CountedAircraft);
}

void UGameUnitManager::DecrementAircraftCounter(AAircraftMaster* Aircraft, const uint8 Player)
{
	if (Aircraft == nullptr)
	{
		return;
	}

	// Taken off the counter it was put on; an aircraft that was never counted, or was already taken off, has
	// nothing to take off.
	FCountedUnitSubtype CountedAircraft;
	if (not M_CountedAircraft.RemoveAndCopyValue(Aircraft, CountedAircraft))
	{
		return;
	}
	TMap<EAircraftSubtype, int32>& Counters = GetUnitsOfPlayer(CountedAircraft.Player).AircraftCountByType;
	GameUnitManagerUnitTypeCounters::DecrementCounter(
		Counters, static_cast<EAircraftSubtype>(CountedAircraft.Subtype), TEXT("Aircraft subtype counter"));
}

bool UGameUnitManager::GetHasPlayerTankOfType(const ETankSubtype TankSubtype) const
{
	return GetHasTankOfTypeForPlayer(RTSPlayers::HumanPlayer, TankSubtype);
}

bool UGameUnitManager::GetHasPlayerSquadOfType(const ESquadSubtype SquadSubtype) const
{
	return GetHasSquadOfTypeForPlayer(RTSPlayers::HumanPlayer, SquadSubtype);
}

bool UGameUnitManager::GetHasPlayerNomadicOfType(const ENomadicSubtype NomadicSubtype) const
{
	return GetHasNomadicOfTypeForPlayer(RTSPlayers::HumanPlayer, NomadicSubtype);
}

bool UGameUnitManager::GetHasPlayerBxpOfType(const EBuildingExpansionType BxpSubtype) const
{
	return GetHasBxpOfTypeForPlayer(RTSPlayers::HumanPlayer, BxpSubtype);
}

bool UGameUnitManager::GetHasPlayerAircraftOfType(const EAircraftSubtype AircraftSubtype) const
{
	return GetHasAircraftOfTypeForPlayer(RTSPlayers::HumanPlayer, AircraftSubtype);
}

bool UGameUnitManager::GetHasEnemyTankOfType(const ETankSubtype TankSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasAnyAIPlayerUnitOfType(
		M_UnitsOfPlayer, &FGameUnitsOfPlayer::TankCountByType, TankSubtype);
}

bool UGameUnitManager::GetHasEnemySquadOfType(const ESquadSubtype SquadSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasAnyAIPlayerUnitOfType(
		M_UnitsOfPlayer, &FGameUnitsOfPlayer::SquadCountByType, SquadSubtype);
}

bool UGameUnitManager::GetHasEnemyNomadicOfType(const ENomadicSubtype NomadicSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasAnyAIPlayerUnitOfType(
		M_UnitsOfPlayer, &FGameUnitsOfPlayer::NomadicCountByType, NomadicSubtype);
}

bool UGameUnitManager::GetHasEnemyBxpOfType(const EBuildingExpansionType BxpSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasAnyAIPlayerUnitOfType(
		M_UnitsOfPlayer, &FGameUnitsOfPlayer::BxpCountByType, BxpSubtype);
}

bool UGameUnitManager::GetHasEnemyAircraftOfType(const EAircraftSubtype AircraftSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasAnyAIPlayerUnitOfType(
		M_UnitsOfPlayer, &FGameUnitsOfPlayer::AircraftCountByType, AircraftSubtype);
}

bool UGameUnitManager::GetHasTankOfTypeForPlayer(const uint8 Player, const ETankSubtype TankSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasUnitOfType(GetUnitsOfPlayer(Player).TankCountByType, TankSubtype);
}

bool UGameUnitManager::GetHasSquadOfTypeForPlayer(const uint8 Player, const ESquadSubtype SquadSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasUnitOfType(GetUnitsOfPlayer(Player).SquadCountByType, SquadSubtype);
}

bool UGameUnitManager::GetHasNomadicOfTypeForPlayer(const uint8 Player, const ENomadicSubtype NomadicSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasUnitOfType(
		GetUnitsOfPlayer(Player).NomadicCountByType, NomadicSubtype);
}

bool UGameUnitManager::GetHasBxpOfTypeForPlayer(const uint8 Player, const EBuildingExpansionType BxpSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasUnitOfType(GetUnitsOfPlayer(Player).BxpCountByType, BxpSubtype);
}

bool UGameUnitManager::GetHasAircraftOfTypeForPlayer(const uint8 Player, const EAircraftSubtype AircraftSubtype) const
{
	return GameUnitManagerUnitTypeCounters::GetHasUnitOfType(
		GetUnitsOfPlayer(Player).AircraftCountByType, AircraftSubtype);
}

int32 UGameUnitManager::GetPlayerSquadCountOfTypes(const uint8 Player, const TArray<ESquadSubtype>& SquadTypes) const
{
	if (SquadTypes.Num() == 0)
	{
		return 0;
	}

	const TSet<ESquadSubtype> RequestedSquadTypes(SquadTypes);
	const TArray<ASquadUnit*>& SquadUnits = GetUnitsOfPlayer(Player).SquadUnits;

	int32 MatchingSquadCount = 0;
	for (const ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		const URTSComponent* SquadRTSComponent = SquadUnit->GetRTSComponent();
		if (not IsValid(SquadRTSComponent))
		{
			continue;
		}

		if (RequestedSquadTypes.Contains(SquadRTSComponent->GetSubtypeAsSquadSubtype()))
		{
			++MatchingSquadCount;
		}
	}

	return MatchingSquadCount;
}

int32 UGameUnitManager::GetPlayerTankCountOfTypes(const uint8 Player, const TArray<ETankSubtype>& TankTypes) const
{
	if (TankTypes.Num() == 0)
	{
		return 0;
	}

	const TSet<ETankSubtype> RequestedTankTypes(TankTypes);
	const TArray<ATankMaster*>& TankUnits = GetUnitsOfPlayer(Player).TankMasters;

	int32 MatchingTankCount = 0;
	for (const ATankMaster* TankUnit : TankUnits)
	{
		if (not IsValid(TankUnit))
		{
			continue;
		}

		const URTSComponent* TankRTSComponent = TankUnit->GetRTSComponent();
		if (not IsValid(TankRTSComponent))
		{
			continue;
		}

		if (TankRTSComponent->GetUnitType() != EAllUnitType::UNType_Tank)
		{
			continue;
		}

		if (RequestedTankTypes.Contains(TankRTSComponent->GetSubtypeAsTankSubtype()))
		{
			++MatchingTankCount;
		}
	}

	return MatchingTankCount;
}

int32 UGameUnitManager::GetPlayerBxpTotalCountOfTypes(const uint8 Player,
                                                      const TArray<EBuildingExpansionType>& BxpTypes) const
{
	if (BxpTypes.Num() == 0)
	{
		return 0;
	}

	const TSet<EBuildingExpansionType> RequestedBxpTypes(BxpTypes);
	const TArray<ABuildingExpansion*>& BuildingExpansions = GetUnitsOfPlayer(Player).Bxps;

	int32 MatchingBxpCount = 0;
	for (const ABuildingExpansion* BuildingExpansion : BuildingExpansions)
	{
		if (not IsValid(BuildingExpansion))
		{
			continue;
		}

		const URTSComponent* BuildingExpansionRTSComponent = BuildingExpansion->GetRTSComponent();
		if (not IsValid(BuildingExpansionRTSComponent))
		{
			continue;
		}

		if (BuildingExpansionRTSComponent->GetUnitType() != EAllUnitType::UNType_BuildingExpansion)
		{
			continue;
		}

		if (RequestedBxpTypes.Contains(BuildingExpansionRTSComponent->GetSubtypeAsBxpSubtype()))
		{
			++MatchingBxpCount;
		}
	}

	return MatchingBxpCount;
}

TMap<EBuildingExpansionType, int32> UGameUnitManager::GetPlayerBxpCountsByType(
	const uint8 Player,
	const TArray<EBuildingExpansionType>& BxpTypes) const
{
	TMap<EBuildingExpansionType, int32> BxpCountsByType;
	for (const EBuildingExpansionType BxpType : BxpTypes)
	{
		BxpCountsByType.FindOrAdd(BxpType) = 0;
	}

	if (BxpCountsByType.Num() == 0)
	{
		return BxpCountsByType;
	}

	const TArray<ABuildingExpansion*>& BuildingExpansions = GetUnitsOfPlayer(Player).Bxps;
	for (const ABuildingExpansion* BuildingExpansion : BuildingExpansions)
	{
		// One still under construction (or packed up) must not unlock its tech yet.
		if (not IsValid(BuildingExpansion) || not BuildingExpansion->GetIsStanding())
		{
			continue;
		}

		const URTSComponent* BuildingExpansionRTSComponent = BuildingExpansion->GetRTSComponent();
		if (not IsValid(BuildingExpansionRTSComponent))
		{
			continue;
		}

		if (BuildingExpansionRTSComponent->GetUnitType() != EAllUnitType::UNType_BuildingExpansion)
		{
			continue;
		}

		int32* const BxpCount = BxpCountsByType.Find(BuildingExpansionRTSComponent->GetSubtypeAsBxpSubtype());
		if (BxpCount == nullptr)
		{
			continue;
		}

		++(*BxpCount);
	}

	return BxpCountsByType;
}

int32 UGameUnitManager::GetPlayerAircraftCountOfTypes(const uint8 Player,
                                                      const TArray<EAircraftSubtype>& AircraftTypes) const
{
	if (AircraftTypes.Num() == 0)
	{
		return 0;
	}

	const TSet<EAircraftSubtype> RequestedAircraftTypes(AircraftTypes);
	const TArray<AAircraftMaster*>& AircraftUnits = GetUnitsOfPlayer(Player).AircraftMasters;

	int32 MatchingAircraftCount = 0;
	for (const AAircraftMaster* AircraftUnit : AircraftUnits)
	{
		if (not IsValid(AircraftUnit))
		{
			continue;
		}

		const URTSComponent* AircraftRTSComponent = AircraftUnit->GetRTSComponent();
		if (not IsValid(AircraftRTSComponent))
		{
			continue;
		}

		if (RequestedAircraftTypes.Contains(AircraftRTSComponent->GetSubtypeAsAircraftSubtype()))
		{
			++MatchingAircraftCount;
		}
	}

	return MatchingAircraftCount;
}

ATankMaster* UGameUnitManager::GetPlayerCommandVehicle() const
{
	for (ATankMaster* EachTank : GetUnitsOfPlayer(RTSPlayers::HumanPlayer).TankMasters)
	{
		if (not IsValid(EachTank))
		{
			continue;
		}

		URTSComponent* TankRTSComponent = EachTank->GetRTSComponent();
		if (not IsValid(TankRTSComponent))
		{
			continue;
		}

		if (TankRTSComponent->GetUnitType() != EAllUnitType::UNType_Tank)
		{
			continue;
		}

		const ETankSubtype TankSubtype = TankRTSComponent->GetSubtypeAsTankSubtype();
		const bool bIsCommanderTank =
			TankSubtype == ETankSubtype::Tank_PzIII_J_Commander
			|| TankSubtype == ETankSubtype::Tank_PzIV_F1_Commander;
		if (bIsCommanderTank)
		{
			return EachTank;
		}
	}

	return nullptr;
}

ANomadicVehicle* UGameUnitManager::GetPlayerHQ() const
{
	for (ATankMaster* EachPlayerTank : GetUnitsOfPlayer(RTSPlayers::HumanPlayer).TankMasters)
	{
		if (not IsValid(EachPlayerTank))
		{
			continue;
		}

		URTSComponent* NomadicRTSComp = EachPlayerTank->GetRTSComponent();
		if (not IsValid(NomadicRTSComp))
		{
			continue;
		}

		if (NomadicRTSComp->GetUnitType() != EAllUnitType::UNType_Nomadic)
		{
			continue;
		}

		const ENomadicSubtype NomadicSubtype = NomadicRTSComp->GetSubtypeAsNomadicSubtype();
		const bool bIsHQ = NomadicSubtype == ENomadicSubtype::Nomadic_GerHq;
		if (bIsHQ)
		{
			return Cast<ANomadicVehicle>(EachPlayerTank);
		}
	}
	return nullptr;
}


AActor* UGameUnitManager::GetClosestTargetPreferSquadUnit(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector SearchLocation) const
{
	using namespace GameUnitManagerClosestTarget;
	// The targets are the units of every player the searching unit fights.
	const uint8 HostileMask = FRTSPlayers::GetHostileMask(PlayerSearch);
	FClosestTargetSearch Search(SearchLocation, SearchRadius);

	// Prefer squads
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::SquadUnits);
	if (Search.Target) { return Search.Target; }

	// Then tanks
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::TankMasters);
	if (Search.Target) { return Search.Target; }

	// NEW: Finally BXPs (buildings)
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::Bxps);

	return Search.Target;
}

AActor* UGameUnitManager::GetClosestTargetPreferTank(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector& SearchLocation) const
{
	using namespace GameUnitManagerClosestTarget;
	// The targets are the units of every player the searching unit fights.
	const uint8 HostileMask = FRTSPlayers::GetHostileMask(PlayerSearch);
	FClosestTargetSearch Search(SearchLocation, SearchRadius);

	// Prefer tanks
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::TankMasters);
	if (Search.Target) { return Search.Target; }

	// Then squads
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::SquadUnits);
	if (Search.Target) { return Search.Target; }

	// NEW: Finally BXPs (buildings)
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::Bxps);

	return Search.Target;
}

AActor* UGameUnitManager::GetClosestTarget(
	const uint8 PlayerSearch,
	const float SearchRadius,
	const FVector& SearchLocation) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindTarget);

	using namespace GameUnitManagerClosestTarget;
	// The targets are the units of every player the searching unit fights.
	const uint8 HostileMask = FRTSPlayers::GetHostileMask(PlayerSearch);
	FClosestTargetSearch Search(SearchLocation, SearchRadius);

	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::TankMasters);
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::SquadUnits);
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::Actors);
	// NEW: BXPs too
	ConsiderUnitsOfPlayers(Search, M_UnitsOfPlayer, HostileMask, &FGameUnitsOfPlayer::Bxps);

	return Search.Target;
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
		return FindBxpUnitOfPlayer(static_cast<EBuildingExpansionType>(TrainingOption.SubtypeValue), OwningPlayer);
	case EAllUnitType::UNType_StandaloneTurret:
		return FindGenericActorUnitForPlayer(TrainingOption, OwningPlayer);
	default:
		RTSFunctionLibrary::ReportError("Unknown unit type in GetFirstUnitOfTypeForPlayer"
			"\n at function GetFirstUnitOfTypeForPlayer in CPPGameState.cpp");
		return nullptr;
	}
	return nullptr;
}

AActor* UGameUnitManager::FindGenericActorUnitForPlayer(
	const FTrainingOption& TrainingOption,
	const int32 OwningPlayer) const
{
	const TArray<AActor*>& Actors = GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).Actors;
	for (AActor* Actor : Actors)
	{
		if (not IsValid(Actor))
		{
			continue;
		}

		const URTSComponent* RTS = Actor->FindComponentByClass<URTSComponent>();
		if (IsValid(RTS) && RTS->GetUnitTrainingOption() == TrainingOption)
		{
			return Actor;
		}
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
	for (FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		CleanupActorArray(UnitsOfPlayer.Actors);
		CleanupActorArray(UnitsOfPlayer.TankMasters);
		CleanupActorArray(UnitsOfPlayer.SquadUnits);

		CleanupActorArray(UnitsOfPlayer.AircraftMasters);
		CleanupActorArray(UnitsOfPlayer.Bxps);
	}
}


void UGameUnitManager::BuildAsyncTargetSnapshot(FAsyncTargetSnapshot& OutSnapshot) const
{
	int32 ExpectedUnits = 0;
	for (const FGameUnitsOfPlayer& UnitsOfPlayer : M_UnitsOfPlayer)
	{
		ExpectedUnits += UnitsOfPlayer.Actors.Num()
			+ UnitsOfPlayer.TankMasters.Num()
			+ UnitsOfPlayer.SquadUnits.Num()
			+ UnitsOfPlayer.AircraftMasters.Num()
			+ UnitsOfPlayer.Bxps.Num();
	}
	OutSnapshot.Units.Reserve(ExpectedUnits);

	// The units of one player sit next to each other, so a request only walks the players it fights.
	for (int32 Player = RTSPlayers::NeutralOwner; Player < RTSPlayers::PlayerArraySize; ++Player)
	{
		OutSnapshot.FirstUnitOfPlayer[Player] = OutSnapshot.Units.Num();
		AppendAsyncTargetUnitsOfPlayer(Player, OutSnapshot);
	}
	OutSnapshot.FirstUnitOfPlayer[RTSPlayers::PlayerArraySize] = OutSnapshot.Units.Num();

	if constexpr (DeveloperSettings::Debugging::GAsyncTargetFinding_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			TEXT("Amount of possible targetable units of all players ") + FString::FromInt(OutSnapshot.Units.Num()),
			FColor::Purple);
	}
}

void UGameUnitManager::AppendAsyncTargetUnitsOfPlayer(const int32 Player, FAsyncTargetSnapshot& OutSnapshot) const
{
	using GameUnitManagerAsyncTargets::AddTargetUnit;
	using GameUnitManagerAsyncTargets::AddTargetUnits;
	const FGameUnitsOfPlayer& UnitsOfPlayer = M_UnitsOfPlayer[Player];

	// Generic actors
	AddTargetUnits(UnitsOfPlayer.Actors, Player, ETargetPreference::Building, OutSnapshot);

	// Tanks
	AddTargetUnits(UnitsOfPlayer.TankMasters, Player, ETargetPreference::Tank, OutSnapshot);

	// Squads
	AddTargetUnits(UnitsOfPlayer.SquadUnits, Player, ETargetPreference::Infantry, OutSnapshot);

	// Aircraft
	for (AAircraftMaster* EachAircraft : UnitsOfPlayer.AircraftMasters)
	{
		// Parked inside its airbase the aircraft is part of the building; anti-air engages it once it lifts off.
		if (IsValid(EachAircraft) && EachAircraft->GetLandedState() == EAircraftLandingState::Landed)
		{
			continue;
		}
		AddTargetUnit(EachAircraft, Player, ETargetPreference::Aircraft, OutSnapshot);
	}

	//  BXPs
	AddTargetUnits(UnitsOfPlayer.Bxps, Player, ETargetPreference::Building, OutSnapshot);
}

AActor* UGameUnitManager::FindTankUnitOfPlayer(const ETankSubtype TankSubtype, const int32 OwningPlayer) const
{
	const TArray<ATankMaster*>* TankMasters = &GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).TankMasters;
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
	const TArray<AAircraftMaster*>* AircraftMasters =
		&GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).AircraftMasters;
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
	const TArray<ASquadUnit*>* SquadUnits = &GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).SquadUnits;
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
	const TArray<ATankMaster*>* TankMasters = &GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).TankMasters;
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
	const TArray<ABuildingExpansion*>* Bxps = &GetUnitsOfPlayer(static_cast<uint8>(OwningPlayer)).Bxps;
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

void UGameUnitManager::ApplyTechToTanksOfPlayer(
	UTechnologyEffect* TechEffect,
	const TArray<ETankSubtype>& TankSubtypes,
	const uint8 Player) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	const TArray<ATankMaster*>& Tanks = GetUnitsOfPlayer(Player).TankMasters;
	for (ATankMaster* Tank : Tanks)
	{
		if (not IsValid(Tank))
		{
			continue;
		}

		const URTSComponent* RTSComponent = Tank->GetRTSComponent();
		if (IsValid(RTSComponent) &&
			RTSComponent->GetUnitType() == EAllUnitType::UNType_Tank &&
			TankSubtypes.Contains(RTSComponent->GetSubtypeAsTankSubtype()))
		{
			TechEffect->ApplyOnTank(Tank);
		}
	}
}

void UGameUnitManager::ApplyTechToNomadicsOfPlayer(
	UTechnologyEffect* TechEffect,
	const TArray<ENomadicSubtype>& NomadicSubtypes,
	const uint8 Player) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	const TArray<ATankMaster*>& Tanks = GetUnitsOfPlayer(Player).TankMasters;
	for (ATankMaster* Tank : Tanks)
	{
		ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Tank);
		if (not IsValid(Nomadic))
		{
			continue;
		}

		const URTSComponent* RTSComponent = Nomadic->GetRTSComponent();
		if (IsValid(RTSComponent) &&
			RTSComponent->GetUnitType() == EAllUnitType::UNType_Nomadic &&
			NomadicSubtypes.Contains(RTSComponent->GetSubtypeAsNomadicSubtype()))
		{
			TechEffect->ApplyOnNomadic(Nomadic);
		}
	}
}

void UGameUnitManager::ApplyTechToSquadsOfPlayer(
	UTechnologyEffect* TechEffect,
	const TArray<ESquadSubtype>& SquadSubtypes,
	const uint8 Player) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	const TArray<ASquadUnit*>& SquadUnits = GetUnitsOfPlayer(Player).SquadUnits;
	TSet<ASquadController*> ProcessedSquads;
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		ASquadController* Squad = SquadUnit->GetSquadControllerChecked();
		if (not IsValid(Squad) || ProcessedSquads.Contains(Squad))
		{
			continue;
		}

		ProcessedSquads.Add(Squad);
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		if (IsValid(RTSComponent) &&
			RTSComponent->GetUnitType() == EAllUnitType::UNType_Squad &&
			SquadSubtypes.Contains(RTSComponent->GetSubtypeAsSquadSubtype()))
		{
			TechEffect->ApplyOnSquad(Squad);
		}
	}
}

void UGameUnitManager::ApplyTechToBuildingExpansionsOfPlayer(
	UTechnologyEffect* TechEffect,
	const TArray<EBuildingExpansionType>& BxpSubtypes,
	const uint8 Player) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	const TArray<ABuildingExpansion*>& BuildingExpansions = GetUnitsOfPlayer(Player).Bxps;
	for (ABuildingExpansion* BuildingExpansion : BuildingExpansions)
	{
		if (not IsValid(BuildingExpansion))
		{
			continue;
		}

		const URTSComponent* RTSComponent = BuildingExpansion->GetRTSComponent();
		if (IsValid(RTSComponent) &&
			RTSComponent->GetUnitType() == EAllUnitType::UNType_BuildingExpansion &&
			BxpSubtypes.Contains(RTSComponent->GetSubtypeAsBxpSubtype()))
		{
			TechEffect->ApplyOnBuildingExpansion(BuildingExpansion);
		}
	}
}

void UGameUnitManager::ApplyTechToAircraftOfPlayer(
	UTechnologyEffect* TechEffect,
	const TArray<EAircraftSubtype>& AircraftSubtypes,
	const uint8 Player) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	const TArray<AAircraftMaster*>& AircraftUnits = GetUnitsOfPlayer(Player).AircraftMasters;
	for (AAircraftMaster* Aircraft : AircraftUnits)
	{
		if (not IsValid(Aircraft))
		{
			continue;
		}

		const URTSComponent* RTSComponent = Aircraft->GetRTSComponent();
		if (IsValid(RTSComponent) &&
			RTSComponent->GetUnitType() == EAllUnitType::UNType_Aircraft &&
			AircraftSubtypes.Contains(RTSComponent->GetSubtypeAsAircraftSubtype()))
		{
			TechEffect->ApplyOnAircraft(Aircraft);
		}
	}
}
