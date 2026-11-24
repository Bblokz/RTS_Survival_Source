// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FourPlayerSkirmishSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveSettings.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTSMapTests/DamageHooks/DamageMapTestHooks.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/Turret/StandaloneTurret/StandaloneTurret.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFourPlayerSkirmish, Log, All);

namespace FourPlayerSkirmishPrivate
{
	const TCHAR* const SkirmishSwitch = TEXT("FourPlayerSkirmish");
	const TCHAR* const PhaseSwitch = TEXT("FourPlayerSkirmishPhase=");
	const TCHAR* const DirectorySwitch = TEXT("FourPlayerSkirmishDir=");
	const TCHAR* const LocationSwitch = TEXT("FourPlayerSkirmishAt=");
	const TCHAR* const SideSwitch = TEXT("FourPlayerSkirmishSide=");
	const TCHAR* const SecondsSwitch = TEXT("FourPlayerSkirmishSeconds=");
	const TCHAR* const FreeForAllPhaseName = TEXT("FreeForAll");
	const TCHAR* const TeamsPhaseName = TEXT("Teams");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	constexpr double HudBuildSeconds = 5.0;
	// Watchdog: a phase takes a few minutes of real time at most, even on a slow DebugGame build.
	constexpr double MaxRunRealSeconds = 20.0 * 60.0;
	// Default 26 m side: adjacent groups stand 18-34 m apart, inside rifle (~37 m) and cannon (~38-40 m) range.
	constexpr float DefaultSquareSideCm = 2600.f;
	constexpr float MinSquareSideCm = 1000.f;
	constexpr float MaxSquareSideCm = 10000.f;
	constexpr float DefaultFightGameSeconds = 120.f;
	constexpr float MinFightGameSeconds = 10.f;
	// Turrets of tanks and the weapons of standalone turrets are set up a moment after BeginPlay.
	constexpr float SettleGameSeconds = 3.f;
	constexpr float SetupTimeoutGameSeconds = 30.f;
	constexpr float SampleIntervalGameSeconds = 0.25f;
	// A hit counts for the attacker's units that held the victim as target within this many seconds before it.
	constexpr float TargetCreditWindowSeconds = 3.f;
	// Once every player damaged enough hostile players, keep fighting this long so late friendly fire still shows.
	constexpr float CriteriaHoldGameSeconds = 10.f;
	// Shells in flight still land after the last group of the second-to-last player died.
	constexpr float AfterLastPlayerGameSeconds = 3.f;
	constexpr float StatusLogIntervalGameSeconds = 10.f;
	constexpr float NeverTargetedSeconds = -1.0e6f;
	// Player 1 stands at +X+Y, then counter-clockwise: allies of the Teams phase (1+3, 2+4) end up on the diagonals.
	constexpr float FirstCornerYawDegrees = 45.f;
	constexpr float CornerStepYawDegrees = 90.f;
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
	constexpr float CmPerMetre = 100.f;
	constexpr int32 MaxOwnerWalkSteps = 6;
	constexpr int32 FreeForAllHostilePlayersRequired = 2;
	// Section 6 only asks that damage crosses teams; one hostile player per player makes sure a fight happened.
	constexpr int32 TeamsHostilePlayersRequired = 1;
	constexpr int32 MinHostileGroupsInReach = 2;
	constexpr int32 MaxDetailLogsPerKind = 5;
	constexpr int32 TeamOfOddPlayers = 1;
	constexpr int32 TeamOfEvenPlayers = 2;
	// Default arena on RTS_ToCopySmall: the flat field the vehicle duel bench fights on.
	const FVector DefaultArenaLocation(16796.f, -15808.f, 0.f);
	// Probe units of the controllers wait this long for the async load, the spawn and their weapon states.
	constexpr float ControllerProbeTimeoutGameSeconds = 20.f;
	// Beyond the group's corner on the player's own diagonal: ~60 m from the nearest hostile group at the default
	// side, well outside the ~40 m weapon ranges, so the probe never takes part in the fight.
	constexpr float ControllerProbeOutwardMarginCm = 4500.f;
	// Controller waves start this far beyond their group's corner (~43 m from the centre, out of weapon range) and
	// attack-move to the arena centre, so the controller's own orders bring them into the fight.
	constexpr float ControllerWaveOutwardMarginCm = 2500.f;
	// Sideways spacing of the wave's spawn points, across the player's diagonal.
	constexpr float ControllerWaveElementSpacingCm = 700.f;
	constexpr int32 ControllerWaveUnitCount = 2;
	// Waves draw one supply per unit; harness-spawned controllers start with none.
	constexpr int32 ControllerWaveSupply = 10;
	constexpr float ControllerWaveArrivalTimeoutGameSeconds = 30.f;
	// A wave that has not both fired at and hit a hostile player by then counts as settled (and fails).
	constexpr float ControllerWaveEngageTimeoutGameSeconds = 90.f;
	// Over both controllers' waves: the controllers' units must engage at least this many different players.
	constexpr int32 ControllerWavesHostilePlayersRequired = 2;
	// The controllers' own unit count think step runs every 25 s; the harness queues the same query this often so
	// results arrive while squads still live. The controllers' think loop (every 6 s) sends it.
	constexpr float UnitCountQueryIntervalGameSeconds = 5.f;
	// Without a conclusive result by then the strategic AI check fails instead of holding the fight open.
	constexpr float StrategicCheckTimeoutFightSeconds = 60.f;
	// Reflected: the harness sets protected UPROPERTYs of a deferred controller instead of adding setters to it.
	const FName ControlledPlayerPropertyName(TEXT("ControlledPlayer"));
	const FName MissionSettingsPropertyName(TEXT("M_EnemyAIMissionSettings"));
	const FName DecisionTreePropertyName(TEXT("M_StochasticDecisionTree"));
	// Data asset holding the designer-authored survival decision tree (Docs/40_WavesAndAI.md).
	const TCHAR* const AuthoredTreeSettingsPath =
		TEXT("/Game/RTS_Survival/SurvivalMode/Data/Waves/DA_SurvivalWaveSettings.DA_SurvivalWaveSettings");
	const TCHAR* const SpawnPathOption = TEXT("AsyncSpawnOptionAtLocation");
	const TCHAR* const SpawnPathClass = TEXT("AsyncSpawnClassAtLocation");

	FVector GetOutwardDirectionOfPlayer(const int32 Player)
	{
		const float CornerYawRadians = FMath::DegreesToRadians(
			FirstCornerYawDegrees + CornerStepYawDegrees * (Player - RTSPlayers::HumanPlayer));
		return FVector(FMath::Cos(CornerYawRadians), FMath::Sin(CornerYawRadians), 0.f);
	}

	// Half the diagonal: the corners of a square with this side around the arena centre.
	float GetCornerRadiusCm(const float SquareSideCm)
	{
		return SquareSideCm / FMath::Sqrt(2.f);
	}

	bool TrySetControlledPlayer(AEnemyController& Controller, const int32 Player)
	{
		const FIntProperty* const PlayerProperty = FindFProperty<FIntProperty>(
			AEnemyController::StaticClass(), ControlledPlayerPropertyName);
		if (PlayerProperty == nullptr)
		{
			return false;
		}
		PlayerProperty->SetPropertyValue_InContainer(&Controller, Player);
		return true;
	}

	/**
	 * The strategy a designer would author on a placed controller, kept to one choice: attack-move idle units to the
	 * bulks of units hostile to the controller (USubAction_AttackMoveToPlayerUnits, the stock action).
	 * @return The tree, or an empty tree when its action list cannot be found by reflection.
	 */
	UStochasticDecisionTree* BuildAttackHostileUnitsTree(AEnemyController& Controller)
	{
		UStochasticDecisionTree* const Tree = NewObject<UStochasticDecisionTree>(&Controller);
		const FArrayProperty* const ActionsProperty = FindFProperty<FArrayProperty>(
			UStochasticDecisionTree::StaticClass(), TEXT("M_ActionDefinitions"));
		const FStructProperty* const ActionProperty = ActionsProperty != nullptr
			                                              ? CastField<FStructProperty>(ActionsProperty->Inner)
			                                              : nullptr;
		if (ActionProperty == nullptr || ActionProperty->Struct != FStrategicAIAction::StaticStruct())
		{
			return Tree;
		}
		FScriptArrayHelper Actions(ActionsProperty, ActionsProperty->ContainerPtrToValuePtr<void>(Tree));
		const int32 ActionIndex = Actions.AddValue();
		// Default action: Attack, score 1; the sub-action carries the order.
		FStrategicAIAction* const Action = reinterpret_cast<FStrategicAIAction*>(Actions.GetRawPtr(ActionIndex));
		Action->GetSubActions().Add(NewObject<USubAction_AttackMoveToPlayerUnits>(Tree));
		return Tree;
	}

	/**
	 * Sets up a harness-spawned controller: no training or building, thinking started by the harness (a bare
	 * controller starts thinking at once), and a one-action decision tree that may order its idle units (one unit is
	 * enough for an action).
	 * @return False when a property was not found; the controller then starts with its class defaults.
	 */
	bool TryMakeControllerQuiet(AEnemyController& Controller)
	{
		const FStructProperty* const SettingsProperty = FindFProperty<FStructProperty>(
			AEnemyController::StaticClass(), MissionSettingsPropertyName);
		const FObjectPropertyBase* const TreeProperty = FindFProperty<FObjectPropertyBase>(
			AEnemyController::StaticClass(), DecisionTreePropertyName);
		const bool bIsSettingsPropertyValid = SettingsProperty != nullptr
			&& SettingsProperty->Struct == FEnemyAIMissionSettings::StaticStruct();
		const bool bIsTreePropertyValid = TreeProperty != nullptr
			&& TreeProperty->PropertyClass->IsChildOf(UStochasticDecisionTree::StaticClass());
		if (not bIsSettingsPropertyValid || not bIsTreePropertyValid)
		{
			return false;
		}
		FEnemyAIMissionSettings* const MissionSettings =
			SettingsProperty->ContainerPtrToValuePtr<FEnemyAIMissionSettings>(&Controller);
		MissionSettings->StrategicAIThinkingStartCondition =
			EEnemyStrategicAIThinkingStartCondition::StartAfterMissionTrigger;
		MissionSettings->bAllowDirectControlStochasticDecisionTree = true;
		MissionSettings->MinPickedUnitsForAction = 1;
		MissionSettings->bAllowUnitTraining = false;
		MissionSettings->bAllowBuilding = false;
		TreeProperty->SetObjectPropertyValue_InContainer(&Controller, BuildAttackHostileUnitsTree(Controller));
		return true;
	}

	struct FSkirmishUnitSpec
	{
		EFourPlayerSkirmishUnitKind Kind = EFourPlayerSkirmishUnitKind::Tank;
		const TCHAR* Name = TEXT("");
		const TCHAR* ClassPath = TEXT("");
		// Distance from the group's corner along the line from the arena centre; negative is toward the centre.
		float RadialOffsetCm = 0.f;
		float SpawnLiftCm = 0.f;
	};

	// The same three classes for every player so the fight is fair; 6 m between members keeps the squad's spawn
	// grid clear of the tank and the turret.
	const TArray<FSkirmishUnitSpec>& GetGroupSpecs()
	{
		static const TArray<FSkirmishUnitSpec> Specs = {
			{EFourPlayerSkirmishUnitKind::Tank, TEXT("Tank"),
			 TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
				 "Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C"), 600.f, 150.f},
			{EFourPlayerSkirmishUnitKind::Squad, TEXT("Squad"),
			 TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C"), -600.f, 50.f},
			{EFourPlayerSkirmishUnitKind::Turret, TEXT("Turret"),
			 TEXT("/Game/RTS_Survival/Blueprints/Turrets/StandaloneTurrets/RusBallTurret/"
				 "BP_StandaloneTurret_RusBall_50MM.BP_StandaloneTurret_RusBall_50MM_C"), 0.f, 0.f},
		};
		return Specs;
	}

	/** @return The tank spec of the groups; the controller probes spawn the same tank. */
	const FSkirmishUnitSpec& GetTankSpec()
	{
		const TArray<FSkirmishUnitSpec>& Specs = GetGroupSpecs();
		const FSkirmishUnitSpec* const TankSpec = Specs.FindByPredicate([](const FSkirmishUnitSpec& Spec)
		{
			return Spec.Kind == EFourPlayerSkirmishUnitKind::Tank;
		});
		return TankSpec != nullptr ? *TankSpec : Specs[0];
	}

	FString FormatSeconds(const float Seconds)
	{
		return Seconds < 0.f ? FString(TEXT("-")) : FString::Printf(TEXT("%.1f"), Seconds);
	}

	FString GetRelationName(const int32 AttackingPlayer, const int32 VictimOwner)
	{
		if (not FRTSPlayers::IsValidPlayer(AttackingPlayer))
		{
			return TEXT("unclaimed");
		}
		if (not FRTSPlayers::IsValidPlayer(VictimOwner))
		{
			return TEXT("other");
		}
		if (AttackingPlayer == VictimOwner)
		{
			return TEXT("same_player");
		}
		return FRTSPlayers::AreHostile(AttackingPlayer, VictimOwner) ? TEXT("hostile") : TEXT("allied");
	}

	/** @return "P1 0 / P2 120 / ..." for an array indexed by player. */
	FString FormatPlayerValues(const float (&Values)[RTSPlayers::PlayerArraySize])
	{
		TArray<FString> Parts;
		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			Parts.Add(FString::Printf(TEXT("P%d %.0f"), Player, Values[Player]));
		}
		return FString::Join(Parts, TEXT(" / "));
	}

	/** @return "P1 0 / P2 12 / ..." for a count array indexed by player. */
	FString FormatPlayerCounts(const int32 (&Counts)[RTSPlayers::PlayerArraySize])
	{
		TArray<FString> Parts;
		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			Parts.Add(FString::Printf(TEXT("P%d %d"), Player, Counts[Player]));
		}
		return FString::Join(Parts, TEXT(" / "));
	}

	const TCHAR* GetUnitKindName(const EFourPlayerSkirmishUnitKind Kind)
	{
		switch (Kind)
		{
		case EFourPlayerSkirmishUnitKind::Tank:
			return TEXT("Tank turret + hull MG (Blueprint tank)");
		case EFourPlayerSkirmishUnitKind::Squad:
			return TEXT("Infantry weapons (Blueprint squad)");
		case EFourPlayerSkirmishUnitKind::Turret:
			return TEXT("Standalone turret (Blueprint)");
		default:
			return TEXT("Other");
		}
	}

	const EFourPlayerSkirmishUnitKind AllUnitKinds[] = {
		EFourPlayerSkirmishUnitKind::Tank, EFourPlayerSkirmishUnitKind::Squad, EFourPlayerSkirmishUnitKind::Turret
	};

	/** @return Distinct players hostile to the unit's owner that it fired at; one fire event is enough. */
	int32 CountHostilePlayersFiredAt(const FFourPlayerSkirmishUnit& Unit)
	{
		int32 Count = 0;
		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			if (Unit.FireEventsAtPlayer[Player] > 0 && FRTSPlayers::AreHostile(Unit.OwningPlayer, Player))
			{
				++Count;
			}
		}
		return Count;
	}

	/** @return The most hostile players any one unit of the kind fired at. */
	int32 GetMostHostilePlayersFiredAtByKind(const TArray<FFourPlayerSkirmishUnit>& Units,
	                                          const EFourPlayerSkirmishUnitKind Kind)
	{
		int32 Most = 0;
		for (const FFourPlayerSkirmishUnit& Unit : Units)
		{
			if (Unit.Kind == Kind)
			{
				Most = FMath::Max(Most, CountHostilePlayersFiredAt(Unit));
			}
		}
		return Most;
	}

	// A kind of weapon, over the four players' units of that kind, must have fired at this many different players:
	// with three, it engages more than one other player and no single "enemy" player. Per unit it is not required: a
	// turret keeps its first target until one of them dies.
	constexpr int32 FreeForAllKindTargetPlayersRequired = 3;

	/** @return Distinct players that units of the kind fired at, counting only players hostile to the shooter. */
	int32 CountPlayersFiredAtByKind(const TArray<FFourPlayerSkirmishUnit>& Units,
	                                const EFourPlayerSkirmishUnitKind Kind)
	{
		uint8 TargetPlayersMask = 0;
		for (const FFourPlayerSkirmishUnit& Unit : Units)
		{
			if (Unit.Kind != Kind)
			{
				continue;
			}
			for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
			{
				if (Unit.FireEventsAtPlayer[Player] > 0 && FRTSPlayers::AreHostile(Unit.OwningPlayer, Player))
				{
					TargetPlayersMask |= FRTSPlayers::GetPlayerBit(Player);
				}
			}
		}
		return FMath::CountBits(TargetPlayersMask);
	}

	/** @return "P1->P2,P4" style list of the hostile players each unit of the kind hit (exact) or fired at. */
	FString GetEngagementText(const TArray<FFourPlayerSkirmishUnit>& Units, const EFourPlayerSkirmishUnitKind Kind,
	                          const bool bExactHits)
	{
		TArray<FString> Parts;
		for (const FFourPlayerSkirmishUnit& Unit : Units)
		{
			if (Unit.Kind != Kind)
			{
				continue;
			}
			TArray<FString> Victims;
			for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
			{
				const bool bEngaged = bExactHits ? Unit.ExactHitsOnPlayer[Player] > 0 : Unit.FireEventsAtPlayer[Player] > 0;
				if (bEngaged)
				{
					Victims.Add(FString::Printf(TEXT("P%d"), Player));
				}
			}
			Parts.Add(FString::Printf(TEXT("P%d->%s"), Unit.OwningPlayer,
			                          Victims.Num() > 0 ? *FString::Join(Victims, TEXT(",")) : TEXT("none")));
		}
		return FString::Join(Parts, TEXT("; "));
	}

	FString GetHostilePlayersText(const int32 Player)
	{
		TArray<FString> Hostiles;
		for (int32 Other = RTSPlayers::HumanPlayer; Other <= RTSPlayers::MaxPlayers; ++Other)
		{
			if (FRTSPlayers::AreHostile(Player, Other))
			{
				Hostiles.Add(FString::Printf(TEXT("P%d"), Other));
			}
		}
		return Hostiles.Num() > 0 ? FString::Join(Hostiles, TEXT(" ")) : FString(TEXT("nobody"));
	}

	void TagActorInEnemyVision(AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		Actor->Tags.AddUnique(DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision);
	}

	void AddCarrierIfWeaponOwner(TArray<TWeakObjectPtr<UObject>>& InOutCarriers, UObject* Candidate)
	{
		if (not IsValid(Candidate) || Cast<IWeaponOwner>(Candidate) == nullptr)
		{
			return;
		}
		InOutCarriers.AddUnique(TWeakObjectPtr<UObject>(Candidate));
	}

	/** Adds the actor, its components (hull weapons) and its attached actors (turrets, infantry weapons). */
	void AddActorCarriers(TArray<TWeakObjectPtr<UObject>>& InOutCarriers, AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		AddCarrierIfWeaponOwner(InOutCarriers, Actor);
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* const Component : Components)
		{
			AddCarrierIfWeaponOwner(InOutCarriers, Component);
		}
		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* const AttachedActor : AttachedActors)
		{
			AddCarrierIfWeaponOwner(InOutCarriers, AttachedActor);
		}
	}

	/** @return The target the carrier's weapons aim at now, or null. */
	AActor* GetHeldTargetOfCarrier(const UObject* Carrier)
	{
		if (const ACPPTurretsMaster* const Turret = Cast<ACPPTurretsMaster>(Carrier))
		{
			return Turret->GetCurrentTargetActor();
		}
		if (const UHullWeaponComponent* const HullWeapon = Cast<UHullWeaponComponent>(Carrier))
		{
			return HullWeapon->GetCurrentTargetActor();
		}
		if (const AInfantryWeaponMaster* const InfantryWeapon = Cast<AInfantryWeaponMaster>(Carrier))
		{
			return InfantryWeapon->GetCurrentTargetActor();
		}
		if (const AStandaloneTurret* const StandaloneTurret = Cast<AStandaloneTurret>(Carrier))
		{
			return StandaloneTurret->GetCurrentTargetActor();
		}
		return nullptr;
	}

	/**
	 * @brief Counts the weapon states of one carrier and those that name another player than expected.
	 * @param Carrier Turret, hull weapon, infantry weapon or standalone turret.
	 * @param ExpectedOwner Player the unit was spawned for.
	 * @param InOutWeaponCount Incremented per weapon state.
	 * @return Number of owner mismatches on the carrier and its weapon states.
	 */
	int32 CountCarrierOwnerMismatches(UObject* Carrier, const int32 ExpectedOwner, int32& InOutWeaponCount)
	{
		IWeaponOwner* const WeaponOwner = Cast<IWeaponOwner>(Carrier);
		if (WeaponOwner == nullptr)
		{
			return 0;
		}
		int32 Mismatches = 0;
		if (ACPPTurretsMaster* const Turret = Cast<ACPPTurretsMaster>(Carrier))
		{
			Mismatches += Turret->GetOwningPlayer() != ExpectedOwner ? 1 : 0;
		}
		if (const AStandaloneTurret* const StandaloneTurret = Cast<AStandaloneTurret>(Carrier))
		{
			Mismatches += StandaloneTurret->GetOwningPlayerForWeaponInit() != ExpectedOwner ? 1 : 0;
		}
		const TArray<UWeaponState*> Weapons = WeaponOwner->GetWeapons();
		for (const UWeaponState* const Weapon : Weapons)
		{
			if (not IsValid(Weapon))
			{
				continue;
			}
			++InOutWeaponCount;
			Mismatches += Weapon->GetOwningPlayer() != ExpectedOwner ? 1 : 0;
		}
		return Mismatches;
	}

	bool GetIsAnySoldierAlive(const FFourPlayerSkirmishUnit& Unit)
	{
		for (const TWeakObjectPtr<AActor>& SoldierActor : Unit.Soldiers)
		{
			AActor* const Soldier = SoldierActor.Get();
			if (IsValid(Soldier) && Soldier->IsUnitAlive())
			{
				return true;
			}
		}
		return false;
	}

	// Pairs of players walked as one flat loop: PairIndex 0..MaxPlayers^2-1 -> (attacker, victim) 1..MaxPlayers.
	constexpr int32 PlayerPairCount = RTSPlayers::MaxPlayers * RTSPlayers::MaxPlayers;

	int32 GetFirstPlayerOfPair(const int32 PairIndex)
	{
		return RTSPlayers::HumanPlayer + PairIndex / RTSPlayers::MaxPlayers;
	}

	int32 GetSecondPlayerOfPair(const int32 PairIndex)
	{
		return RTSPlayers::HumanPlayer + PairIndex % RTSPlayers::MaxPlayers;
	}

	void GatherUnitLocations(const FFourPlayerSkirmishUnit& Unit, TArray<FVector>& OutLocations)
	{
		for (const TWeakObjectPtr<AActor>& Soldier : Unit.Soldiers)
		{
			if (Soldier.IsValid())
			{
				OutLocations.Add(Soldier->GetActorLocation());
			}
		}
		if (Unit.Actor.IsValid())
		{
			OutLocations.Add(Unit.Actor->GetActorLocation());
		}
	}
}

bool UFourPlayerSkirmishSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), FourPlayerSkirmishPrivate::SkirmishSwitch);
}

void UFourPlayerSkirmishSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ReadCommandLine();
	OpenReportFiles();
	M_DamageAppliedHandle = FDamageMapTestHooks::OnDamageApplied().AddUObject(
		this, &UFourPlayerSkirmishSubsystem::OnDamageApplied);
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSFourPlayerSkirmish, Display,
	       TEXT("RTS_FPS started phase=%s dir=%s arena=%s side_cm=%.0f max_fight_s=%.0f"), *GetPhaseName(),
	       *M_OutputDirectory, *M_ArenaCentre.ToCompactString(), M_SquareSideCm, M_MaxFightGameSeconds);
}

void UFourPlayerSkirmishSubsystem::Deinitialize()
{
	FDamageMapTestHooks::OnDamageApplied().Remove(M_DamageAppliedHandle);
	M_DamageAppliedHandle.Reset();
	Super::Deinitialize();
}

TStatId UFourPlayerSkirmishSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFourPlayerSkirmishSubsystem, STATGROUP_Tickables);
}

void UFourPlayerSkirmishSubsystem::Tick(float DeltaTime)
{
	using namespace FourPlayerSkirmishPrivate;
	if (not bM_IsActive || M_Step == EFourPlayerSkirmishStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		Finish(false, {TEXT("real_time_watchdog")});
		return;
	}
	KeepGameRunning();
	switch (M_Step)
	{
	case EFourPlayerSkirmishStep::StartGame:
		TickStartGame(RealSeconds);
		break;
	case EFourPlayerSkirmishStep::Spawn:
		SpawnPhase();
		break;
	case EFourPlayerSkirmishStep::Fight:
		TickFight();
		break;
	default:
		break;
	}
}

void UFourPlayerSkirmishSubsystem::ReadCommandLine()
{
	using namespace FourPlayerSkirmishPrivate;
	const TCHAR* const CommandLine = FCommandLine::Get();
	M_ArenaCentre = DefaultArenaLocation;
	FString LocationText;
	// Not stopping on separators: the default would cut "X,Y" at the comma.
	if (FParse::Value(CommandLine, LocationSwitch, LocationText, false))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_ArenaCentre = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	M_SquareSideCm = DefaultSquareSideCm;
	FParse::Value(CommandLine, SideSwitch, M_SquareSideCm);
	M_SquareSideCm = FMath::Clamp(M_SquareSideCm, MinSquareSideCm, MaxSquareSideCm);
	M_MaxFightGameSeconds = DefaultFightGameSeconds;
	FParse::Value(CommandLine, SecondsSwitch, M_MaxFightGameSeconds);
	M_MaxFightGameSeconds = FMath::Max(MinFightGameSeconds, M_MaxFightGameSeconds);
	FString PhaseText;
	if (FParse::Value(CommandLine, PhaseSwitch, PhaseText))
	{
		const bool bIsTeams = PhaseText.Equals(TeamsPhaseName, ESearchCase::IgnoreCase);
		M_Phase = bIsTeams ? EFourPlayerSkirmishPhase::Teams : EFourPlayerSkirmishPhase::FreeForAll;
		bM_IsPhaseArgumentInvalid = not bIsTeams && not PhaseText.Equals(FreeForAllPhaseName, ESearchCase::IgnoreCase);
	}
	M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FourPlayerSkirmish"),
	                                    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_")) + GetPhaseName());
	FParse::Value(CommandLine, DirectorySwitch, M_OutputDirectory, false);
}

void UFourPlayerSkirmishSubsystem::TickStartGame(const double RealSeconds)
{
	if (bM_IsPhaseArgumentInvalid)
	{
		Finish(false, {TEXT("unknown_phase_argument")});
		return;
	}
	if (RealSeconds < FourPlayerSkirmishPrivate::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	M_Step = EFourPlayerSkirmishStep::Spawn;
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS game started, spawning phase=%s"), *GetPhaseName());
}

bool UFourPlayerSkirmishSubsystem::TryStartGame()
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	M_PlayerController = PlayerController;
	if (not IsValid(PlayerController))
	{
		return false;
	}
	UPlayerStartGameControl* const StartGameControl = PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(FourPlayerSkirmishPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const PlayerController = M_PlayerController.Get();
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UFourPlayerSkirmishSubsystem::OpenReportFiles()
{
	M_HitsCsvPath = FPaths::Combine(M_OutputDirectory, TEXT("hits.csv"));
	M_SummaryPath = FPaths::Combine(M_OutputDirectory, TEXT("summary.md"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*M_OutputDirectory);
	FFileHelper::SaveStringToFile(
		TEXT("fight_s,attacker_player,attacker_units,victim,victim_player,reported_victim_owner,relation,damage,killed\n"),
		*M_HitsCsvPath);
}

// ---------------------------------------------------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::SpawnPhase()
{
	using namespace FourPlayerSkirmishPrivate;
	ApplyPhaseTeams();
	if (not GetAreTeamsApplied())
	{
		Finish(false, {TEXT("teams_not_applied")});
		return;
	}
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_ArenaCentre, GroundZ))
	{
		Finish(false, {TEXT("no_ground_at_arena")});
		return;
	}
	M_ArenaCentre.Z = GroundZ;
	SetUpEnemyControllers();
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		SpawnGroup(Player, M_ArenaCentre);
	}
	if (M_SpawnFailureCount > 0)
	{
		Finish(false, {FString::Printf(TEXT("spawn_failures=%d"), M_SpawnFailureCount)});
		return;
	}
	for (FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		Unit.LastTargetedUnitSeconds.Init(NeverTargetedSeconds, M_Units.Num());
	}
	// Units open fire on anything in range as soon as they spawn, so the clock and the damage matrix start here.
	M_FightStartGameSeconds = GetGameSeconds();
	M_LastSampleGameSeconds = M_FightStartGameSeconds;
	M_Step = EFourPlayerSkirmishStep::Fight;
	RequestControllerProbeSpawns();
	StartControllerWaves();
	SpawnOwnerChangeUnits();
	StartControllerStrategicAI();
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS spawned units=%d arena=%s side_cm=%.0f"), M_Units.Num(),
	       *M_ArenaCentre.ToCompactString(), M_SquareSideCm);
}

void UFourPlayerSkirmishSubsystem::ApplyPhaseTeams() const
{
	using namespace FourPlayerSkirmishPrivate;
	FRTSPlayers::ResetToFreeForAll();
	if (M_Phase != EFourPlayerSkirmishPhase::Teams)
	{
		return;
	}
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		FRTSPlayers::SetPlayerTeam(Player, Player % 2 == 1 ? TeamOfOddPlayers : TeamOfEvenPlayers);
	}
}

bool UFourPlayerSkirmishSubsystem::GetAreTeamsApplied() const
{
	using namespace FourPlayerSkirmishPrivate;
	for (int32 PairIndex = 0; PairIndex < PlayerPairCount; ++PairIndex)
	{
		const int32 PlayerA = GetFirstPlayerOfPair(PairIndex);
		const int32 PlayerB = GetSecondPlayerOfPair(PairIndex);
		const bool bShouldBeHostile = PlayerA != PlayerB
			&& (M_Phase == EFourPlayerSkirmishPhase::FreeForAll || PlayerA % 2 != PlayerB % 2);
		if (FRTSPlayers::AreHostile(PlayerA, PlayerB) == bShouldBeHostile)
		{
			continue;
		}
		UE_LOG(LogRTSFourPlayerSkirmish, Error, TEXT("RTS_FPS relation P%d-P%d is %s, expected %s"), PlayerA,
		       PlayerB, bShouldBeHostile ? TEXT("allied") : TEXT("hostile"),
		       bShouldBeHostile ? TEXT("hostile") : TEXT("allied"));
		return false;
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::SpawnGroup(const int32 OwningPlayer, const FVector& ArenaCentre)
{
	using namespace FourPlayerSkirmishPrivate;
	const FVector OutwardDirection = GetOutwardDirectionOfPlayer(OwningPlayer);
	const float CornerRadiusCm = GetCornerRadiusCm(M_SquareSideCm);
	const FRotator Facing = (-OutwardDirection).Rotation();
	const TArray<FSkirmishUnitSpec>& Specs = GetGroupSpecs();
	for (int32 UnitSlot = 0; UnitSlot < Specs.Num(); ++UnitSlot)
	{
		FVector GroundLocation = ArenaCentre + OutwardDirection * (CornerRadiusCm + Specs[UnitSlot].RadialOffsetCm);
		float GroundZ = ArenaCentre.Z;
		(void)TryGetGroundZ(GroundLocation, GroundZ);
		GroundLocation.Z = GroundZ;
		if (not SpawnSkirmishUnit(OwningPlayer, UnitSlot, GroundLocation, Facing))
		{
			++M_SpawnFailureCount;
		}
	}
}

bool UFourPlayerSkirmishSubsystem::SpawnSkirmishUnit(const int32 OwningPlayer, const int32 UnitSlot,
                                                     const FVector& GroundLocation, const FRotator& Facing)
{
	using namespace FourPlayerSkirmishPrivate;
	const FSkirmishUnitSpec& Spec = GetGroupSpecs()[UnitSlot];
	UClass* const UnitClass = LoadClass<AActor>(nullptr, Spec.ClassPath);
	if (not IsValid(UnitClass))
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Error, TEXT("RTS_FPS no class %s"), Spec.ClassPath);
		return false;
	}
	const FVector SpawnLocation = GroundLocation + FVector(0.f, 0.f, Spec.SpawnLiftCm);
	AActor* const SpawnedUnit = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), UnitClass, FTransform(Facing, SpawnLocation), static_cast<uint8>(OwningPlayer));
	if (not IsValid(SpawnedUnit))
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Error, TEXT("RTS_FPS spawn failed class=%s player=%d"),
		       *UnitClass->GetName(), OwningPlayer);
		return false;
	}
	const int32 UnitIndex = M_Units.AddDefaulted();
	FFourPlayerSkirmishUnit& Unit = M_Units[UnitIndex];
	Unit.Actor = SpawnedUnit;
	Unit.ClassName = UnitClass->GetName();
	Unit.ClassName.RemoveFromEnd(TEXT("_C"));
	Unit.Label = FString::Printf(TEXT("P%d_%s"), OwningPlayer, Spec.Name);
	Unit.OwningPlayer = OwningPlayer;
	Unit.Kind = Spec.Kind;
	M_UnitIndexByActor.Add(TWeakObjectPtr<const AActor>(SpawnedUnit), UnitIndex);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS spawned %s class=%s at %s"), *Unit.Label,
	       *Unit.ClassName, *SpawnLocation.ToCompactString());
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Enemy controllers of players 3 and 4
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::SetUpEnemyControllers()
{
	UEnemyControllerSubsystem* const ControllerSubsystem = GetWorld()->GetSubsystem<UEnemyControllerSubsystem>();
	if (not IsValid(ControllerSubsystem))
	{
		AddControllerFailure(TEXT("no_enemy_controller_subsystem"));
		return;
	}
	// Read before players 3 and 4 get one: the argument-less getter falls back to the lowest registered player.
	const AEnemyController* const MapPlayer2Controller =
		ControllerSubsystem->GetEnemyControllerForPlayer(RTSPlayers::FirstAIPlayer);
	const FString MapPlayer2Text = IsValid(MapPlayer2Controller)
		                               ? MapPlayer2Controller->GetName()
		                               : FString(TEXT("none"));
	M_ControllerLines.Add(FString::Printf(TEXT("- Map controller for P%d: %s"), RTSPlayers::FirstAIPlayer,
	                                      *MapPlayer2Text));
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controllers map_P%d=%s"), RTSPlayers::FirstAIPlayer,
	       *MapPlayer2Text);
	for (int32 Player = RTSPlayers::FirstAIPlayer + 1; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		FFourPlayerSkirmishControllerProbe Probe;
		Probe.Player = Player;
		AEnemyController* const Controller = FindOrSpawnEnemyController(*ControllerSubsystem, Player,
		                                                                 Probe.bIsSpawnedByHarness);
		Probe.Controller = Controller;
		Probe.ControllerName = GetNameSafe(Controller);
		M_ControllerProbes.Add(Probe);
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controllers P%d controller=%s source=%s"), Player,
		       *Probe.ControllerName, Probe.bIsSpawnedByHarness ? TEXT("spawned_by_harness") : TEXT("map"));
	}
	CheckEnemyControllerRegistry(*ControllerSubsystem, MapPlayer2Controller);
}

AEnemyController* UFourPlayerSkirmishSubsystem::FindOrSpawnEnemyController(
	const UEnemyControllerSubsystem& ControllerSubsystem, const int32 Player, bool& bOutIsSpawnedByHarness)
{
	AEnemyController* const MapController = ControllerSubsystem.GetEnemyControllerForPlayer(Player);
	bOutIsSpawnedByHarness = not IsValid(MapController);
	return IsValid(MapController) ? MapController : SpawnEnemyController(Player);
}

AEnemyController* UFourPlayerSkirmishSubsystem::SpawnEnemyController(const int32 Player)
{
	using namespace FourPlayerSkirmishPrivate;
	// The plain C++ class, not the maps' RTS_BP_EnemyController: its defaults (decision tree, global ability
	// loadout) could act on the arena, while a quiet bare controller only proves registration and spawning.
	const FTransform SpawnTransform(FRotator::ZeroRotator, M_ArenaCentre);
	AEnemyController* const Controller = GetWorld()->SpawnActorDeferred<AEnemyController>(
		AEnemyController::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Controller))
	{
		AddControllerFailure(FString::Printf(TEXT("controller_actor_spawn_failed_P%d"), Player));
		return nullptr;
	}
	// Set before FinishSpawning: PostInitializeComponents registers the controller under this player.
	if (not TrySetControlledPlayer(*Controller, Player))
	{
		// Finishing it would register a second player-2 controller.
		Controller->Destroy();
		AddControllerFailure(FString::Printf(TEXT("controller_player_property_missing_P%d"), Player));
		return nullptr;
	}
	const bool bIsQuiet = TryMakeControllerQuiet(*Controller);
	Controller->FinishSpawning(SpawnTransform);
	if (not bIsQuiet)
	{
		M_Warnings.Add(FString::Printf(TEXT("The P%d controller could not be made quiet (mission settings or decision "
			"tree property not found); it runs with its class defaults."), Player));
	}
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controllers spawned %s for P%d quiet=%d"),
	       *Controller->GetName(), Player, bIsQuiet ? 1 : 0);
	return Controller;
}

void UFourPlayerSkirmishSubsystem::CheckEnemyControllerRegistry(const UEnemyControllerSubsystem& ControllerSubsystem,
                                                                const AEnemyController* MapPlayer2Controller)
{
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		CheckProbeRegistration(ControllerSubsystem, Probe);
	}
	const AEnemyController* const DefaultController = ControllerSubsystem.GetEnemyController();
	const FString DefaultText = IsValid(DefaultController)
		                            ? FString::Printf(TEXT("%s (P%d)"), *DefaultController->GetName(),
		                                              DefaultController->GetControlledPlayer())
		                            : FString(TEXT("none"));
	M_ControllerLines.Add(TEXT("- Argument-less GetEnemyController(): ") + DefaultText);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controllers default=%s"), *DefaultText);
	if (not IsValid(MapPlayer2Controller))
	{
		M_Warnings.Add(TEXT("The map has no player-2 enemy controller, so the check that the argument-less "
			"GetEnemyController() returns player 2's was skipped; it returns ") + DefaultText + TEXT("."));
		return;
	}
	if (DefaultController != MapPlayer2Controller)
	{
		AddControllerFailure(TEXT("default_controller_not_P2"));
	}
}

void UFourPlayerSkirmishSubsystem::CheckProbeRegistration(const UEnemyControllerSubsystem& ControllerSubsystem,
                                                          const FFourPlayerSkirmishControllerProbe& Probe)
{
	const AEnemyController* const Controller = Probe.Controller.Get();
	if (not IsValid(Controller))
	{
		AddControllerFailure(FString::Printf(TEXT("controller_missing_P%d"), Probe.Player));
		return;
	}
	const AEnemyController* const Registered = ControllerSubsystem.GetEnemyControllerForPlayer(Probe.Player);
	const int32 ControlledPlayer = Controller->GetControlledPlayer();
	const bool bIsListed = ControllerSubsystem.GetAllEnemyControllers().Contains(Controller);
	M_ControllerLines.Add(FString::Printf(TEXT("- P%d: %s, GetControlledPlayer() %d, GetEnemyControllerForPlayer(%d) "
		                                      "%s, in GetAllEnemyControllers() %s"), Probe.Player,
	                                      *Probe.ControllerName, ControlledPlayer, Probe.Player,
	                                      Registered == Controller ? TEXT("returns it") : TEXT("**does not return it**"),
	                                      bIsListed ? TEXT("yes") : TEXT("**no**")));
	if (Registered != Controller)
	{
		AddControllerFailure(FString::Printf(TEXT("controller_not_registered_P%d"), Probe.Player));
	}
	if (ControlledPlayer != Probe.Player)
	{
		AddControllerFailure(FString::Printf(TEXT("controller_plays_P%d_not_P%d"), ControlledPlayer, Probe.Player));
	}
	if (not bIsListed)
	{
		AddControllerFailure(FString::Printf(TEXT("controller_not_listed_P%d"), Probe.Player));
	}
}

void UFourPlayerSkirmishSubsystem::RequestControllerProbeSpawns()
{
	for (int32 ProbeIndex = 0; ProbeIndex < M_ControllerProbes.Num(); ++ProbeIndex)
	{
		RequestControllerProbeSpawn(ProbeIndex);
	}
}

void UFourPlayerSkirmishSubsystem::RequestControllerProbeSpawn(const int32 ProbeIndex)
{
	using namespace FourPlayerSkirmishPrivate;
	AEnemyController* const Controller = M_ControllerProbes[ProbeIndex].Controller.Get();
	ARTSAsyncSpawner* const AsyncSpawner = IsValid(Controller) ? FRTS_Statics::GetAsyncSpawner(Controller) : nullptr;
	if (not IsValid(AsyncSpawner))
	{
		// A missing controller already failed the registry check.
		M_ControllerProbes[ProbeIndex].Result = TEXT("not spawned: no controller or no async spawner");
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_not_requested_P%d"),
		                                     M_ControllerProbes[ProbeIndex].Player));
		return;
	}
	const int32 Player = M_ControllerProbes[ProbeIndex].Player;
	const FTrainingOption ProbeOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_H));
	const bool bHasOption = AsyncSpawner->GetHasClassForTrainingOption(ProbeOption);
	const FVector Location = GetControllerProbeLocation(Player);
	const FRotator Facing = (-GetOutwardDirectionOfPlayer(Player)).Rotation();
	const int32 OwnerOverride = Controller->GetSpawnOwnerOverride();
	// Filled before the call: the class is loaded by the groups already, so the callback can run inside it.
	FFourPlayerSkirmishControllerProbe& Probe = M_ControllerProbes[ProbeIndex];
	Probe.SpawnPath = bHasOption ? SpawnPathOption : SpawnPathClass;
	Probe.SpawnOwnerOverride = OwnerOverride;
	Probe.RequestGameSeconds = GetGameSeconds();
	Probe.bIsRequested = true;
	const TWeakObjectPtr<UFourPlayerSkirmishSubsystem> WeakThis(this);
	auto OnSpawned = [WeakThis, ProbeIndex](const FTrainingOption& TrainingOption, AActor* SpawnedActor,
	                                        const int32 SpawnID)
	{
		static_cast<void>(TrainingOption);
		static_cast<void>(SpawnID);
		if (UFourPlayerSkirmishSubsystem* const Subsystem = WeakThis.Get())
		{
			Subsystem->OnControllerProbeSpawned(ProbeIndex, SpawnedActor);
		}
	};
	// The call the controller's waves make (UEnemyWaveController): the option at a location with the controller's
	// own owner override. Without a mapping for the option, the same tank class goes through the class variant.
	const TWeakObjectPtr<UObject> CallbackOwner(this);
	const bool bIsStarted = bHasOption
		                        ? AsyncSpawner->AsyncSpawnOptionAtLocation(
			                        ProbeOption, Location, CallbackOwner, Player, OnSpawned, Facing, OwnerOverride)
		                        : AsyncSpawner->AsyncSpawnClassAtLocation(
			                        TSoftClassPtr<AActor>(FSoftObjectPath(GetTankSpec().ClassPath)), ProbeOption,
			                        Location, CallbackOwner, Player, OnSpawned, Facing, OwnerOverride);
	UE_LOG(LogRTSFourPlayerSkirmish, Display,
	       TEXT("RTS_FPS controllers P%d probe requested path=%s owner_override=%d started=%d at %s"), Player,
	       *M_ControllerProbes[ProbeIndex].SpawnPath, OwnerOverride, bIsStarted ? 1 : 0, *Location.ToCompactString());
	if (not bIsStarted && not M_ControllerProbes[ProbeIndex].bHasCallbackRun)
	{
		M_ControllerProbes[ProbeIndex].bIsResolved = true;
		M_ControllerProbes[ProbeIndex].Result = TEXT("**FAIL**: the spawner refused the request");
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_failed_P%d"), Player));
	}
}

void UFourPlayerSkirmishSubsystem::OnControllerProbeSpawned(const int32 ProbeIndex, AActor* SpawnedActor)
{
	if (not M_ControllerProbes.IsValidIndex(ProbeIndex))
	{
		return;
	}
	FFourPlayerSkirmishControllerProbe& Probe = M_ControllerProbes[ProbeIndex];
	Probe.bHasCallbackRun = true;
	Probe.bWasUnitDelivered = IsValid(SpawnedActor);
	Probe.SpawnedUnit = SpawnedActor;
	Probe.SpawnedUnitName = GetNameSafe(SpawnedActor);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controllers P%d probe arrived unit=%s owner=%d after=%.1f s"),
	       Probe.Player, *Probe.SpawnedUnitName, FRTSPlayers::GetOwningPlayerOfActor(SpawnedActor),
	       GetGameSeconds() - Probe.RequestGameSeconds);
}

void UFourPlayerSkirmishSubsystem::TickControllerProbes()
{
	using namespace FourPlayerSkirmishPrivate;
	for (FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		if (not Probe.bIsRequested || Probe.bIsResolved)
		{
			continue;
		}
		const float WaitedSeconds = GetGameSeconds() - Probe.RequestGameSeconds;
		int32 WeaponMismatches = 0;
		// Turrets set up their weapon states a moment after BeginPlay, as for the groups.
		const bool bIsReady = WaitedSeconds >= SettleGameSeconds
			&& MakeProbeUnitRecord(Probe, WeaponMismatches).WeaponCount > 0;
		if (bIsReady || WaitedSeconds >= ControllerProbeTimeoutGameSeconds)
		{
			ResolveControllerProbe(Probe);
		}
	}
}

FFourPlayerSkirmishUnit UFourPlayerSkirmishSubsystem::MakeProbeUnitRecord(
	const FFourPlayerSkirmishControllerProbe& Probe, int32& OutWeaponMismatches) const
{
	const AEnemyController* const Controller = Probe.Controller.Get();
	FFourPlayerSkirmishUnit ProbeUnit;
	ProbeUnit.Actor = Probe.SpawnedUnit;
	ProbeUnit.Kind = EFourPlayerSkirmishUnitKind::Tank;
	ProbeUnit.OwningPlayer = IsValid(Controller) ? Controller->GetControlledPlayer() : Probe.Player;
	CollectWeaponCarriers(ProbeUnit);
	OutWeaponMismatches = CountWeaponOwnerMismatches(ProbeUnit);
	return ProbeUnit;
}

void UFourPlayerSkirmishSubsystem::ResolveControllerProbe(FFourPlayerSkirmishControllerProbe& Probe)
{
	Probe.bIsResolved = true;
	const AActor* const ProbeActor = Probe.SpawnedUnit.Get();
	if (not IsValid(ProbeActor))
	{
		ResolveMissingControllerProbe(Probe);
		return;
	}
	const FFourPlayerSkirmishUnit ProbeUnit = MakeProbeUnitRecord(Probe, Probe.WeaponMismatches);
	Probe.WeaponCount = ProbeUnit.WeaponCount;
	Probe.ReportedOwner = FRTSPlayers::GetOwningPlayerOfActor(ProbeActor);
	const bool bIsOwnerOk = Probe.ReportedOwner == ProbeUnit.OwningPlayer && Probe.WeaponMismatches == 0;
	const bool bHasWeapons = Probe.WeaponCount > 0;
	if (not bIsOwnerOk)
	{
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_owner_P%d"), Probe.Player));
	}
	if (not bHasWeapons)
	{
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_no_weapons_P%d"), Probe.Player));
	}
	Probe.Result = bIsOwnerOk && bHasWeapons ? FString(TEXT("ok")) : FString(TEXT("**FAIL**"));
	if (bIsOwnerOk && bHasWeapons)
	{
		StartControllerStrategyOrder(Probe);
	}
	UE_LOG(LogRTSFourPlayerSkirmish, Display,
	       TEXT("RTS_FPS controllers P%d probe %s unit=%s expected_owner=%d reported_owner=%d weapons=%d "
		       "weapon_owner_mismatches=%d path=%s after=%.1f s"), Probe.Player, *Probe.Result,
	       *Probe.SpawnedUnitName, ProbeUnit.OwningPlayer, Probe.ReportedOwner, Probe.WeaponCount,
	       Probe.WeaponMismatches, *Probe.SpawnPath, GetGameSeconds() - Probe.RequestGameSeconds);
}

void UFourPlayerSkirmishSubsystem::ResolveMissingControllerProbe(FFourPlayerSkirmishControllerProbe& Probe)
{
	if (not Probe.bHasCallbackRun)
	{
		Probe.Result = TEXT("**FAIL**: nothing arrived");
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_timeout_P%d"), Probe.Player));
	}
	else
	{
		Probe.Result = Probe.bWasUnitDelivered
			               ? FString(TEXT("**FAIL**: the unit was gone before the check"))
			               : FString(TEXT("**FAIL**: the spawner reported a failed spawn"));
		AddControllerFailure(FString::Printf(TEXT("controller_spawn_failed_P%d"), Probe.Player));
	}
	UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS controllers P%d probe %s"), Probe.Player, *Probe.Result);
}

void UFourPlayerSkirmishSubsystem::ResolvePendingControllerProbes()
{
	for (FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		if (Probe.bIsRequested && not Probe.bIsResolved)
		{
			ResolveControllerProbe(Probe);
		}
	}
}

bool UFourPlayerSkirmishSubsystem::GetAreControllerProbesResolved() const
{
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		if (Probe.bIsRequested && not Probe.bIsResolved)
		{
			return false;
		}
	}
	return true;
}

FVector UFourPlayerSkirmishSubsystem::GetControllerProbeLocation(const int32 Player) const
{
	using namespace FourPlayerSkirmishPrivate;
	const float DistanceCm = GetCornerRadiusCm(M_SquareSideCm) + ControllerProbeOutwardMarginCm;
	FVector Location = M_ArenaCentre + GetOutwardDirectionOfPlayer(Player) * DistanceCm;
	float GroundZ = M_ArenaCentre.Z;
	(void)TryGetGroundZ(Location, GroundZ);
	Location.Z = GroundZ + GetTankSpec().SpawnLiftCm;
	return Location;
}

void UFourPlayerSkirmishSubsystem::AddControllerFailure(const FString& Reason)
{
	M_ControllerFailReasons.AddUnique(Reason);
	UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS controllers FAIL %s"), *Reason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Setup gate
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::TickSetupGate()
{
	using namespace FourPlayerSkirmishPrivate;
	const float FightSeconds = GetFightSeconds();
	if (FightSeconds < SettleGameSeconds)
	{
		return;
	}
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		RegisterSquadSoldiers(UnitIndex);
		CollectWeaponCarriers(M_Units[UnitIndex]);
		(void)CountWeaponOwnerMismatches(M_Units[UnitIndex]);
	}
	const bool bIsReady = GetAreAllSquadsLoaded() && GetDoAllUnitsHaveWeapons();
	if (not bIsReady && FightSeconds < SetupTimeoutGameSeconds)
	{
		return;
	}
	bM_IsSetupTimedOut = not bIsReady;
	RunSetupGate();
}

bool UFourPlayerSkirmishSubsystem::GetAreAllSquadsLoaded() const
{
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		const ASquadController* const Squad = Cast<ASquadController>(Unit.Actor.Get());
		if (IsValid(Squad) && not Squad->GetIsSquadFullyLoadedAndInitialized())
		{
			return false;
		}
	}
	return true;
}

bool UFourPlayerSkirmishSubsystem::GetDoAllUnitsHaveWeapons() const
{
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		if (Unit.WeaponCount == 0 && GetIsUnitAlive(Unit))
		{
			return false;
		}
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::RegisterSquadSoldiers(const int32 UnitIndex)
{
	if (not M_Units.IsValidIndex(UnitIndex) || M_Units[UnitIndex].Kind != EFourPlayerSkirmishUnitKind::Squad)
	{
		return;
	}
	ASquadController* const Squad = Cast<ASquadController>(M_Units[UnitIndex].Actor.Get());
	if (not IsValid(Squad))
	{
		return;
	}
	const TArray<ASquadUnit*> SquadSoldiers = Squad->GetSquadUnitsChecked();
	for (ASquadUnit* const Soldier : SquadSoldiers)
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		M_Units[UnitIndex].Soldiers.AddUnique(TWeakObjectPtr<AActor>(Soldier));
		M_UnitIndexByActor.Add(TWeakObjectPtr<const AActor>(Soldier), UnitIndex);
	}
}

void UFourPlayerSkirmishSubsystem::CollectWeaponCarriers(FFourPlayerSkirmishUnit& Unit) const
{
	using namespace FourPlayerSkirmishPrivate;
	Unit.WeaponCarriers.Reset();
	AActor* const UnitActor = Unit.Actor.Get();
	if (ATankMaster* const Tank = Cast<ATankMaster>(UnitActor))
	{
		const TArray<ACPPTurretsMaster*> Turrets = Tank->GetTurrets();
		for (ACPPTurretsMaster* const Turret : Turrets)
		{
			AddCarrierIfWeaponOwner(Unit.WeaponCarriers, Turret);
		}
		const TArray<UHullWeaponComponent*> HullWeapons = Tank->GetHullWeapons();
		for (UHullWeaponComponent* const HullWeapon : HullWeapons)
		{
			AddCarrierIfWeaponOwner(Unit.WeaponCarriers, HullWeapon);
		}
	}
	AddActorCarriers(Unit.WeaponCarriers, UnitActor);
	for (const TWeakObjectPtr<AActor>& SoldierActor : Unit.Soldiers)
	{
		const ASquadUnit* const Soldier = Cast<ASquadUnit>(SoldierActor.Get());
		if (not IsValid(Soldier))
		{
			continue;
		}
		AddCarrierIfWeaponOwner(Unit.WeaponCarriers, Soldier->GetInfantryWeapon());
		AddActorCarriers(Unit.WeaponCarriers, SoldierActor.Get());
	}
}

void UFourPlayerSkirmishSubsystem::RunSetupGate()
{
	M_SetupLines.Add(TEXT("| Unit | Class | Player | Reported owner | Soldiers (owner mismatches) | Weapon carriers | "
		"Weapon states (owner mismatches) | Longest range m | Gate |"));
	M_SetupLines.Add(TEXT("|---|---|---|---|---|---|---|---|---|"));
	for (FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		CheckUnitSetup(Unit);
	}
	CheckGroupReach();
	BindWeaponFireCounters();
	RunOwnerChangeChecks();
	if (bM_IsSetupTimedOut)
	{
		M_Warnings.Add(TEXT("The setup gate timed out: a squad never finished loading or a unit has no weapon."));
	}
	bM_IsSetupGateDone = true;
	M_LastStatusLogFightSeconds = GetFightSeconds();
	// Held targets are sampled from here on; the first sample must not cover the wait for the gate.
	M_LastSampleGameSeconds = GetGameSeconds();
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS setup_gate done t=%.1f mismatches=%d timed_out=%d"),
	       GetFightSeconds(), M_SetupMismatchCount, bM_IsSetupTimedOut ? 1 : 0);
}

void UFourPlayerSkirmishSubsystem::CheckUnitSetup(FFourPlayerSkirmishUnit& Unit)
{
	using namespace FourPlayerSkirmishPrivate;
	Unit.bIsSetupChecked = true;
	const AActor* const UnitActor = Unit.Actor.Get();
	if (not IsValid(UnitActor))
	{
		// Killed by fire on sight before the gate; nothing left to check.
		M_SetupLines.Add(FString::Printf(TEXT("| %s | %s | P%d | gone before the gate | - | - | - | - | not checked |"),
		                                 *Unit.Label, *Unit.ClassName, Unit.OwningPlayer));
		M_Warnings.Add(FString::Printf(TEXT("%s was gone before the setup gate could check it."), *Unit.Label));
		return;
	}
	const int32 ReportedOwner = FRTSPlayers::GetOwningPlayerOfActor(UnitActor);
	const int32 ActorMismatches = ReportedOwner != Unit.OwningPlayer ? 1 : 0;
	const int32 SoldierMismatches = CountSoldierOwnerMismatches(Unit);
	const int32 WeaponMismatches = CountWeaponOwnerMismatches(Unit);
	Unit.MaxWeaponRangeCm = GetMaxWeaponRange(Unit);
	const bool bIsMissingSoldiers = Unit.Kind == EFourPlayerSkirmishUnitKind::Squad && Unit.Soldiers.Num() == 0;
	const bool bIsMissingWeapons = Unit.WeaponCount == 0;
	const int32 Problems = ActorMismatches + SoldierMismatches + WeaponMismatches + (bIsMissingSoldiers ? 1 : 0)
		+ (bIsMissingWeapons ? 1 : 0);
	Unit.bIsSetupOk = Problems == 0;
	M_SetupMismatchCount += Problems;
	const FString Line = FString::Printf(TEXT("| %s | %s | P%d | %d | %d (%d) | %d | %d (%d) | %.1f | %s |"),
	                                     *Unit.Label, *Unit.ClassName, Unit.OwningPlayer, ReportedOwner,
	                                     Unit.Soldiers.Num(), SoldierMismatches, Unit.WeaponCarriers.Num(),
	                                     Unit.WeaponCount, WeaponMismatches, Unit.MaxWeaponRangeCm / CmPerMetre,
	                                     Unit.bIsSetupOk ? TEXT("ok") : TEXT("**FAIL**"));
	M_SetupLines.Add(Line);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS setup %s"), *Line);
}

int32 UFourPlayerSkirmishSubsystem::CountSoldierOwnerMismatches(const FFourPlayerSkirmishUnit& Unit) const
{
	int32 Mismatches = 0;
	for (const TWeakObjectPtr<AActor>& Soldier : Unit.Soldiers)
	{
		if (Soldier.IsValid() && FRTSPlayers::GetOwningPlayerOfActor(Soldier.Get()) != Unit.OwningPlayer)
		{
			++Mismatches;
		}
	}
	return Mismatches;
}

int32 UFourPlayerSkirmishSubsystem::CountWeaponOwnerMismatches(FFourPlayerSkirmishUnit& Unit) const
{
	Unit.WeaponCount = 0;
	int32 Mismatches = 0;
	for (const TWeakObjectPtr<UObject>& Carrier : Unit.WeaponCarriers)
	{
		Mismatches += FourPlayerSkirmishPrivate::CountCarrierOwnerMismatches(
			Carrier.Get(), Unit.OwningPlayer, Unit.WeaponCount);
	}
	return Mismatches;
}

float UFourPlayerSkirmishSubsystem::GetMaxWeaponRange(const FFourPlayerSkirmishUnit& Unit) const
{
	float MaxRangeCm = 0.f;
	for (const TWeakObjectPtr<UObject>& Carrier : Unit.WeaponCarriers)
	{
		IWeaponOwner* const WeaponOwner = Cast<IWeaponOwner>(Carrier.Get());
		if (WeaponOwner == nullptr)
		{
			continue;
		}
		const TArray<UWeaponState*> Weapons = WeaponOwner->GetWeapons();
		for (const UWeaponState* const Weapon : Weapons)
		{
			MaxRangeCm = IsValid(Weapon) ? FMath::Max(MaxRangeCm, Weapon->GetRange()) : MaxRangeCm;
		}
	}
	return MaxRangeCm;
}

void UFourPlayerSkirmishSubsystem::CheckGroupReach()
{
	using namespace FourPlayerSkirmishPrivate;
	M_SetupLines.Add(TEXT("\nReach with the real weapon ranges (closest members of two groups vs the group's longest "
		"weapon):\n"));
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		float LongestRangeCm = 0.f;
		for (const FFourPlayerSkirmishUnit& Unit : M_Units)
		{
			LongestRangeCm = Unit.OwningPlayer == Player
				                 ? FMath::Max(LongestRangeCm, Unit.MaxWeaponRangeCm)
				                 : LongestRangeCm;
		}
		TArray<FString> Distances;
		const int32 HostileGroupsInReach = CountHostileGroupsInReach(Player, LongestRangeCm, Distances);
		const FString Line = FString::Printf(TEXT("- P%d longest weapon %.1f m, hostile groups in reach %d: %s"), Player,
		                                     LongestRangeCm / CmPerMetre, HostileGroupsInReach,
		                                     *FString::Join(Distances, TEXT(", ")));
		M_SetupLines.Add(Line);
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS reach %s"), *Line);
		if (HostileGroupsInReach < MinHostileGroupsInReach)
		{
			M_Warnings.Add(FString::Printf(TEXT("P%d reaches only %d hostile groups; the layout may be too wide."),
			                               Player, HostileGroupsInReach));
		}
	}
}

int32 UFourPlayerSkirmishSubsystem::CountHostileGroupsInReach(const int32 Player, const float LongestRangeCm,
                                                              TArray<FString>& OutDistances) const
{
	int32 HostileGroupsInReach = 0;
	for (int32 Other = RTSPlayers::HumanPlayer; Other <= RTSPlayers::MaxPlayers; ++Other)
	{
		if (Other == Player)
		{
			continue;
		}
		const float DistanceCm = GetClosestDistanceBetweenPlayers(Player, Other);
		const bool bIsInReach = DistanceCm <= LongestRangeCm;
		HostileGroupsInReach += bIsInReach && FRTSPlayers::AreHostile(Player, Other) ? 1 : 0;
		OutDistances.Add(FString::Printf(TEXT("P%d %.1f m%s"), Other,
		                                 DistanceCm / FourPlayerSkirmishPrivate::CmPerMetre,
		                                 bIsInReach ? TEXT(" (in reach)") : TEXT("")));
	}
	return HostileGroupsInReach;
}

float UFourPlayerSkirmishSubsystem::GetClosestDistanceBetweenPlayers(const int32 PlayerA, const int32 PlayerB) const
{
	TArray<FVector> LocationsA;
	TArray<FVector> LocationsB;
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		if (Unit.OwningPlayer == PlayerA)
		{
			FourPlayerSkirmishPrivate::GatherUnitLocations(Unit, LocationsA);
		}
		if (Unit.OwningPlayer == PlayerB)
		{
			FourPlayerSkirmishPrivate::GatherUnitLocations(Unit, LocationsB);
		}
	}
	float ClosestDistanceCm = TNumericLimits<float>::Max();
	for (const FVector& LocationA : LocationsA)
	{
		for (const FVector& LocationB : LocationsB)
		{
			ClosestDistanceCm = FMath::Min(ClosestDistanceCm, static_cast<float>(FVector::Dist(LocationA, LocationB)));
		}
	}
	return ClosestDistanceCm;
}

// ---------------------------------------------------------------------------------------------------------------------
// Fight
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::TickFight()
{
	using namespace FourPlayerSkirmishPrivate;
	KeepHumanUnitsInEnemyVision();
	RecordDeaths();
	TickControllerProbes();
	TickControllerWaves();
	if (not bM_IsSetupGateDone)
	{
		TickSetupGate();
		return;
	}
	if (GetGameSeconds() - M_LastSampleGameSeconds >= SampleIntervalGameSeconds)
	{
		SampleHeldTargets();
	}
	const bool bIsQueryDue = M_LastUnitCountQueryFightSeconds < 0.f
		|| GetFightSeconds() - M_LastUnitCountQueryFightSeconds >= UnitCountQueryIntervalGameSeconds;
	if (bIsQueryDue && (not GetAreControllerStrategicChecksSettled() || not GetAreControllerStrategyOrdersSettled()))
	{
		QueueControllerUnitCountQueries();
	}
	if (GetFightSeconds() - M_LastStatusLogFightSeconds >= StatusLogIntervalGameSeconds)
	{
		LogStatus();
	}
	if (GetShouldEndFight())
	{
		FinishPhase();
	}
}

void UFourPlayerSkirmishSubsystem::KeepHumanUnitsInEnemyVision() const
{
	// The AI's fog of war tags human units it sees some seconds late; tagging them here lets every player open fire
	// at the same moment, as the vehicle duel bench does. AI players always see each other.
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		if (not FRTSPlayers::IsHumanPlayer(Unit.OwningPlayer))
		{
			continue;
		}
		FourPlayerSkirmishPrivate::TagActorInEnemyVision(Unit.Actor.Get());
		for (const TWeakObjectPtr<AActor>& Soldier : Unit.Soldiers)
		{
			FourPlayerSkirmishPrivate::TagActorInEnemyVision(Soldier.Get());
		}
	}
}

void UFourPlayerSkirmishSubsystem::SampleHeldTargets()
{
	const float NowGameSeconds = GetGameSeconds();
	const float SampleSeconds = NowGameSeconds - M_LastSampleGameSeconds;
	M_LastSampleGameSeconds = NowGameSeconds;
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		SampleUnitTargets(UnitIndex, SampleSeconds);
	}
}

void UFourPlayerSkirmishSubsystem::SampleUnitTargets(const int32 UnitIndex, const float SampleSeconds)
{
	if (not M_Units.IsValidIndex(UnitIndex) || not GetIsUnitAlive(M_Units[UnitIndex]))
	{
		return;
	}
	uint8 TargetedOwnersMask = 0;
	// Copied: recording a target touches other unit records, never this array, but the copy keeps that obvious.
	const TArray<TWeakObjectPtr<UObject>> Carriers = M_Units[UnitIndex].WeaponCarriers;
	for (const TWeakObjectPtr<UObject>& Carrier : Carriers)
	{
		const AActor* const Target = FourPlayerSkirmishPrivate::GetHeldTargetOfCarrier(Carrier.Get());
		if (IsValid(Target))
		{
			RecordHeldTarget(UnitIndex, Target, TargetedOwnersMask);
		}
	}
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if ((TargetedOwnersMask & FRTSPlayers::GetPlayerBit(Player)) != 0)
		{
			M_Units[UnitIndex].TargetSecondsByPlayer[Player] += SampleSeconds;
		}
	}
}

void UFourPlayerSkirmishSubsystem::RecordHeldTarget(const int32 UnitIndex, const AActor* Target,
                                                    uint8& InOutTargetedOwnersMask)
{
	using namespace FourPlayerSkirmishPrivate;
	const int32 TargetUnitIndex = FindUnitIndexByActor(Target);
	const int32 TargetOwner = M_Units.IsValidIndex(TargetUnitIndex)
		                          ? M_Units[TargetUnitIndex].OwningPlayer
		                          : FRTSPlayers::GetOwningPlayerOfActor(Target);
	FFourPlayerSkirmishUnit& Unit = M_Units[UnitIndex];
	if (Unit.LastTargetedUnitSeconds.IsValidIndex(TargetUnitIndex))
	{
		Unit.LastTargetedUnitSeconds[TargetUnitIndex] = GetFightSeconds();
	}
	if (not FRTSPlayers::IsValidPlayer(TargetOwner))
	{
		return;
	}
	InOutTargetedOwnersMask |= FRTSPlayers::GetPlayerBit(TargetOwner);
	if (FRTSPlayers::AreHostile(Unit.OwningPlayer, TargetOwner))
	{
		return;
	}
	++Unit.NonHostileTargetSamples;
	if (Unit.NonHostileTargetSamples <= MaxDetailLogsPerKind)
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS NON_HOSTILE_TARGET %s holds %s (P%d) t=%.1f"),
		       *Unit.Label, *Target->GetName(), TargetOwner, GetFightSeconds());
	}
}

void UFourPlayerSkirmishSubsystem::RecordDeaths()
{
	for (FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		if (Unit.DeathSeconds >= 0.f || GetIsUnitAlive(Unit))
		{
			continue;
		}
		Unit.DeathSeconds = GetFightSeconds();
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS died %s t=%.1f"), *Unit.Label, Unit.DeathSeconds);
	}
}

bool UFourPlayerSkirmishSubsystem::GetIsUnitAlive(const FFourPlayerSkirmishUnit& Unit) const
{
	if (Unit.Kind == EFourPlayerSkirmishUnitKind::Squad && Unit.Soldiers.Num() > 0)
	{
		return FourPlayerSkirmishPrivate::GetIsAnySoldierAlive(Unit);
	}
	AActor* const UnitActor = Unit.Actor.Get();
	return IsValid(UnitActor) && UnitActor->IsUnitAlive();
}

int32 UFourPlayerSkirmishSubsystem::GetLivingPlayerCount() const
{
	uint8 LivingPlayersMask = 0;
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		LivingPlayersMask |= GetIsUnitAlive(Unit) ? FRTSPlayers::GetPlayerBit(Unit.OwningPlayer) : static_cast<uint8>(0);
	}
	int32 LivingPlayers = 0;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		LivingPlayers += (LivingPlayersMask & FRTSPlayers::GetPlayerBit(Player)) != 0 ? 1 : 0;
	}
	return LivingPlayers;
}

bool UFourPlayerSkirmishSubsystem::GetShouldEndFight()
{
	using namespace FourPlayerSkirmishPrivate;
	const float FightSeconds = GetFightSeconds();
	if (FightSeconds >= M_MaxFightGameSeconds)
	{
		M_FightEndReason = TEXT("time_limit");
		return true;
	}
	if (not GetAreControllerProbesResolved())
	{
		// The controller probes resolve within their own timeout; an early end waits for them.
		return false;
	}
	if (not GetAreControllerWavesSettled() || not GetAreControllerStrategicChecksSettled())
	{
		// The waves of the player 3 and 4 controllers reach the arena some seconds after the groups start fighting.
		return false;
	}
	if (M_LastPlayerStandingSeconds < 0.f && GetLivingPlayerCount() <= 1)
	{
		M_LastPlayerStandingSeconds = FightSeconds;
	}
	if (M_LastPlayerStandingSeconds >= 0.f && FightSeconds - M_LastPlayerStandingSeconds >= AfterLastPlayerGameSeconds)
	{
		M_FightEndReason = TEXT("one_player_left");
		return true;
	}
	if (not GetIsDamageRequirementMet())
	{
		M_CriteriaHeldSinceSeconds = -1.f;
		return false;
	}
	if (M_CriteriaHeldSinceSeconds < 0.f)
	{
		M_CriteriaHeldSinceSeconds = FightSeconds;
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS damage requirement met t=%.1f, holding %.0f s"),
		       FightSeconds, CriteriaHoldGameSeconds);
	}
	if (FightSeconds - M_CriteriaHeldSinceSeconds < CriteriaHoldGameSeconds)
	{
		return false;
	}
	M_FightEndReason = TEXT("criteria_held");
	return true;
}

void UFourPlayerSkirmishSubsystem::LogStatus()
{
	M_LastStatusLogFightSeconds = GetFightSeconds();
	TArray<FString> PlayerTexts;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		int32 LivingUnits = 0;
		for (const FFourPlayerSkirmishUnit& Unit : M_Units)
		{
			LivingUnits += Unit.OwningPlayer == Player && GetIsUnitAlive(Unit) ? 1 : 0;
		}
		PlayerTexts.Add(FString::Printf(TEXT("P%d alive=%d hostile_damaged=%d"), Player, LivingUnits,
		                                CountHostilePlayersDamagedBy(Player)));
	}
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS status t=%.0f reports=%d %s"), M_LastStatusLogFightSeconds,
	       M_DamageReportCount, *FString::Join(PlayerTexts, TEXT(" | ")));
}

// ---------------------------------------------------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::OnDamageApplied(const FMapTestDamageReport& Report)
{
	using namespace FourPlayerSkirmishPrivate;
	if (M_Step != EFourPlayerSkirmishStep::Fight || Report.AppliedDamage <= 0.f)
	{
		return;
	}
	++M_DamageReportCount;
	const bool bIsClaimed = FRTSPlayers::IsValidPlayer(Report.AttackingPlayer);
	if (not bIsClaimed && Report.AttackingPlayer != INDEX_NONE && ++M_OddAttackerHits <= MaxDetailLogsPerKind)
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS ODD_ATTACKER owner=%d victim=%s"),
		       Report.AttackingPlayer, *GetNameSafe(Report.Victim.Get()));
	}
	const int32 AttackerSlot = bIsClaimed ? Report.AttackingPlayer : RTSPlayers::NeutralOwner;
	const int32 VictimUnitIndex = FindUnitIndexByActor(Report.Victim.Get());
	(void)CreditControllerWaveHit(Report, M_Units.IsValidIndex(VictimUnitIndex)
		                                      ? M_Units[VictimUnitIndex].OwningPlayer
		                                      : Report.VictimOwner);
	if (not M_Units.IsValidIndex(VictimUnitIndex))
	{
		RecordDamageOnOtherVictim(Report, AttackerSlot);
		return;
	}
	const int32 VictimOwner = M_Units[VictimUnitIndex].OwningPlayer;
	if (Report.VictimOwner != VictimOwner && ++M_VictimOwnerDriftHits <= MaxDetailLogsPerKind)
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS OWNER_DRIFT %s spawned for P%d reports owner %d"),
		       *M_Units[VictimUnitIndex].Label, VictimOwner, Report.VictimOwner);
	}
	M_DamageMatrix[AttackerSlot][VictimOwner] += Report.AppliedDamage;
	++M_HitMatrix[AttackerSlot][VictimOwner];
	M_Units[VictimUnitIndex].DamageTakenFromPlayer[AttackerSlot] += Report.AppliedDamage;
	++M_Units[VictimUnitIndex].HitsTakenFromPlayer[AttackerSlot];
	// Inferred credit stays for every claimed hit (the verdict never reads it); exact credit is added when the hook
	// named the shooting weapon.
	const FString InferredUnits = bIsClaimed
		                              ? CreditAttackingUnits(AttackerSlot, VictimUnitIndex, VictimOwner,
		                                                     Report.AppliedDamage)
		                              : FString();
	const FString ExactUnit = CreditExactAttackingUnit(Report, AttackerSlot, VictimOwner);
	const FString AttackerUnits = ExactUnit.IsEmpty() ? InferredUnits : TEXT("=") + ExactUnit;
	const bool bIsFriendlyDamage = bIsClaimed && not FRTSPlayers::AreHostile(AttackerSlot, VictimOwner);
	if (bIsFriendlyDamage && M_HitMatrix[AttackerSlot][VictimOwner] <= MaxDetailLogsPerKind)
	{
		UE_LOG(LogRTSFourPlayerSkirmish, Warning,
		       TEXT("RTS_FPS FRIENDLY_DAMAGE P%d (%s) -> %s (P%d) damage=%.1f t=%.1f"), AttackerSlot, *AttackerUnits,
		       *M_Units[VictimUnitIndex].Label, VictimOwner, Report.AppliedDamage, GetFightSeconds());
	}
	AppendHitCsvLine(Report, AttackerUnits, M_Units[VictimUnitIndex].Label, VictimOwner);
}

void UFourPlayerSkirmishSubsystem::RecordDamageOnOtherVictim(const FMapTestDamageReport& Report,
                                                             const int32 AttackerSlot)
{
	M_OtherVictimDamage += Report.AppliedDamage;
	++M_OtherVictimHits;
	const bool bIsFriendly = FRTSPlayers::IsValidPlayer(AttackerSlot)
		&& FRTSPlayers::IsValidPlayer(Report.VictimOwner)
		&& not FRTSPlayers::AreHostile(AttackerSlot, Report.VictimOwner);
	if (bIsFriendly)
	{
		if (M_OtherVictimNonHostileDamage <= 0.f)
		{
			UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS FRIENDLY_DAMAGE P%d -> %s (P%d, not spawned) "
				       "damage=%.1f"), AttackerSlot, *GetNameSafe(Report.Victim.Get()), Report.VictimOwner,
			       Report.AppliedDamage);
		}
		M_OtherVictimNonHostileDamage += Report.AppliedDamage;
	}
	AppendHitCsvLine(Report, FString(), GetNameSafe(Report.Victim.Get()), Report.VictimOwner);
}

int32 UFourPlayerSkirmishSubsystem::FindUnitIndexByActor(const AActor* Actor) const
{
	using namespace FourPlayerSkirmishPrivate;
	// Turrets and infantry weapons are attached child actors; soldiers not registered yet resolve via their squad.
	// Only attach parents are walked: a wreck spawned with the dying unit as its Owner must stay "other".
	// Lookups only ever use valid actors: two invalid weak pointers compare equal, so a null key would match the
	// stale key of any destroyed unit.
	for (int32 Step = 0; Step < MaxOwnerWalkSteps && IsValid(Actor); ++Step)
	{
		if (const int32* const FoundIndex = M_UnitIndexByActor.Find(TWeakObjectPtr<const AActor>(Actor)))
		{
			return *FoundIndex;
		}
		const int32 SquadIndex = FindUnitIndexOfSoldierSquad(Actor);
		if (SquadIndex != INDEX_NONE)
		{
			return SquadIndex;
		}
		Actor = Actor->GetAttachParentActor();
	}
	return INDEX_NONE;
}

int32 UFourPlayerSkirmishSubsystem::FindUnitIndexOfSoldierSquad(const AActor* Actor) const
{
	const ASquadUnit* const Soldier = Cast<ASquadUnit>(Actor);
	if (not IsValid(Soldier))
	{
		return INDEX_NONE;
	}
	const AActor* const SquadController = Soldier->GetSquadControllerChecked().Get();
	if (not IsValid(SquadController))
	{
		return INDEX_NONE;
	}
	const int32* const SquadIndex = M_UnitIndexByActor.Find(TWeakObjectPtr<const AActor>(SquadController));
	return SquadIndex != nullptr ? *SquadIndex : INDEX_NONE;
}

FString UFourPlayerSkirmishSubsystem::CreditAttackingUnits(const int32 AttackingPlayer, const int32 VictimUnitIndex,
                                                           const int32 VictimOwner, const float AppliedDamage)
{
	using namespace FourPlayerSkirmishPrivate;
	const float FightSeconds = GetFightSeconds();
	TArray<int32> CreditedUnitIndices;
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		const FFourPlayerSkirmishUnit& Unit = M_Units[UnitIndex];
		const bool bHeldVictim = Unit.LastTargetedUnitSeconds.IsValidIndex(VictimUnitIndex)
			&& FightSeconds - Unit.LastTargetedUnitSeconds[VictimUnitIndex] <= TargetCreditWindowSeconds;
		if (Unit.OwningPlayer == AttackingPlayer && bHeldVictim)
		{
			CreditedUnitIndices.Add(UnitIndex);
		}
	}
	if (CreditedUnitIndices.Num() == 0)
	{
		M_UnitUncreditedDamage[AttackingPlayer] += AppliedDamage;
		return FString();
	}
	const float DamageShare = AppliedDamage / CreditedUnitIndices.Num();
	TArray<FString> Labels;
	for (const int32 UnitIndex : CreditedUnitIndices)
	{
		M_Units[UnitIndex].DamageDealtToPlayer[VictimOwner] += DamageShare;
		++M_Units[UnitIndex].HitsOnPlayer[VictimOwner];
		Labels.Add(M_Units[UnitIndex].Label);
	}
	return FString::Join(Labels, TEXT("+"));
}

void UFourPlayerSkirmishSubsystem::AppendHitCsvLine(const FMapTestDamageReport& Report, const FString& AttackerUnits,
                                                    const FString& VictimLabel, const int32 VictimOwner) const
{
	const FString Line = FString::Printf(TEXT("%.2f,%d,%s,%s,%d,%d,%s,%.2f,%d"), GetFightSeconds(),
	                                     Report.AttackingPlayer, *AttackerUnits, *VictimLabel, VictimOwner,
	                                     Report.VictimOwner,
	                                     *FourPlayerSkirmishPrivate::GetRelationName(Report.AttackingPlayer, VictimOwner),
	                                     Report.AppliedDamage, Report.bKilled ? 1 : 0);
	AppendLine(M_HitsCsvPath, Line);
}

// ---------------------------------------------------------------------------------------------------------------------
// Verdict and report
// ---------------------------------------------------------------------------------------------------------------------

bool UFourPlayerSkirmishSubsystem::EvaluateCriteria(TArray<FString>& OutReasons) const
{
	const int32 Required = GetRequiredHostilePlayersDamaged();
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		const int32 Damaged = CountHostilePlayersDamagedBy(Player);
		if (Damaged < Required)
		{
			OutReasons.Add(FString::Printf(TEXT("P%d_damaged_%d_of_%d_required_hostile_players"), Player, Damaged,
			                               Required));
		}
	}
	for (int32 PairIndex = 0; PairIndex < FourPlayerSkirmishPrivate::PlayerPairCount; ++PairIndex)
	{
		const int32 Attacker = FourPlayerSkirmishPrivate::GetFirstPlayerOfPair(PairIndex);
		const int32 Victim = FourPlayerSkirmishPrivate::GetSecondPlayerOfPair(PairIndex);
		if (M_DamageMatrix[Attacker][Victim] <= 0.f || FRTSPlayers::AreHostile(Attacker, Victim))
		{
			continue;
		}
		OutReasons.Add(Attacker == Victim
			               ? FString::Printf(TEXT("P%d_damaged_own_units=%.0f"), Attacker,
			                                 M_DamageMatrix[Attacker][Victim])
			               : FString::Printf(TEXT("P%d_damaged_ally_P%d=%.0f"), Attacker, Victim,
			                                 M_DamageMatrix[Attacker][Victim]));
	}
	AppendIntegrityReasons(OutReasons);
	OutReasons.Append(M_ControllerFailReasons);
	AppendControllerWaveReasons(OutReasons);
	AppendStrategicAIReasons(OutReasons);
	OutReasons.Append(M_OwnerChangeFailReasons);
	if (M_Phase == EFourPlayerSkirmishPhase::FreeForAll)
	{
		// Free for all: every kind of Blueprint weapon must engage several players, not only one enemy.
		for (const EFourPlayerSkirmishUnitKind Kind : FourPlayerSkirmishPrivate::AllUnitKinds)
		{
			const int32 Most = FourPlayerSkirmishPrivate::CountPlayersFiredAtByKind(M_Units, Kind);
			if (Most < FourPlayerSkirmishPrivate::FreeForAllKindTargetPlayersRequired)
			{
				const TCHAR* const KindTag = Kind == EFourPlayerSkirmishUnitKind::Tank
					                             ? TEXT("tank")
					                             : Kind == EFourPlayerSkirmishUnitKind::Squad
					                             ? TEXT("infantry")
					                             : TEXT("standalone_turret");
				OutReasons.Add(FString::Printf(TEXT("%s_weapons_fired_at_only_%d_players"), KindTag, Most));
			}
		}
	}
	return OutReasons.Num() == 0;
}

void UFourPlayerSkirmishSubsystem::AppendIntegrityReasons(TArray<FString>& OutReasons) const
{
	int32 NonHostileTargetSamples = 0;
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		NonHostileTargetSamples += Unit.NonHostileTargetSamples;
	}
	struct FIntegrityCount
	{
		int32 Count = 0;
		const TCHAR* Name = TEXT("");
	};
	const FIntegrityCount IntegrityCounts[] = {
		{M_SpawnFailureCount, TEXT("spawn_failures")},
		{M_SetupMismatchCount, TEXT("setup_mismatches")},
		{bM_IsSetupTimedOut ? 1 : 0, TEXT("setup_timeout")},
		{M_EndWeaponMismatchCount, TEXT("end_weapon_owner_mismatches")},
		{M_VictimOwnerDriftHits, TEXT("victim_owner_drift_hits")},
		{M_OddAttackerHits, TEXT("odd_attacker_hits")},
		{NonHostileTargetSamples, TEXT("non_hostile_targets_held")},
		{M_ShooterOwnerMismatchHits, TEXT("shooter_owner_mismatch_hits")},
	};
	for (const FIntegrityCount& IntegrityCount : IntegrityCounts)
	{
		if (IntegrityCount.Count > 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s=%d"), IntegrityCount.Name, IntegrityCount.Count));
		}
	}
	if (M_OtherVictimNonHostileDamage > 0.f)
	{
		OutReasons.Add(FString::Printf(TEXT("friendly_damage_on_unspawned=%.0f"), M_OtherVictimNonHostileDamage));
	}
}

bool UFourPlayerSkirmishSubsystem::GetIsDamageRequirementMet() const
{
	const int32 Required = GetRequiredHostilePlayersDamaged();
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (CountHostilePlayersDamagedBy(Player) < Required)
		{
			return false;
		}
	}
	if (M_Phase != EFourPlayerSkirmishPhase::FreeForAll)
	{
		return true;
	}
	for (const EFourPlayerSkirmishUnitKind Kind : FourPlayerSkirmishPrivate::AllUnitKinds)
	{
		if (FourPlayerSkirmishPrivate::CountPlayersFiredAtByKind(M_Units, Kind)
			< FourPlayerSkirmishPrivate::FreeForAllKindTargetPlayersRequired)
		{
			return false;
		}
	}
	return true;
}

int32 UFourPlayerSkirmishSubsystem::GetRequiredHostilePlayersDamaged() const
{
	using namespace FourPlayerSkirmishPrivate;
	return M_Phase == EFourPlayerSkirmishPhase::Teams ? TeamsHostilePlayersRequired : FreeForAllHostilePlayersRequired;
}

int32 UFourPlayerSkirmishSubsystem::CountHostilePlayersDamagedBy(const int32 AttackingPlayer) const
{
	if (not FRTSPlayers::IsValidPlayer(AttackingPlayer))
	{
		return 0;
	}
	int32 Damaged = 0;
	for (int32 Victim = RTSPlayers::HumanPlayer; Victim <= RTSPlayers::MaxPlayers; ++Victim)
	{
		Damaged += FRTSPlayers::AreHostile(AttackingPlayer, Victim) && M_DamageMatrix[AttackingPlayer][Victim] > 0.f
			           ? 1
			           : 0;
	}
	return Damaged;
}

float UFourPlayerSkirmishSubsystem::GetUnclaimedDamageOnUnits() const
{
	float UnclaimedDamage = 0.f;
	for (int32 Victim = RTSPlayers::HumanPlayer; Victim <= RTSPlayers::MaxPlayers; ++Victim)
	{
		UnclaimedDamage += M_DamageMatrix[RTSPlayers::NeutralOwner][Victim];
	}
	return UnclaimedDamage;
}

void UFourPlayerSkirmishSubsystem::RecheckWeaponOwnersAtEnd()
{
	if (not bM_IsSetupGateDone)
	{
		return;
	}
	for (FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		if (not GetIsUnitAlive(Unit))
		{
			continue;
		}
		CollectWeaponCarriers(Unit);
		const int32 Mismatches = CountWeaponOwnerMismatches(Unit);
		M_EndWeaponMismatchCount += Mismatches;
		if (Mismatches > 0)
		{
			M_Warnings.Add(FString::Printf(TEXT("%s: %d weapon owner mismatches at the end of the fight."),
			                               *Unit.Label, Mismatches));
		}
	}
}

void UFourPlayerSkirmishSubsystem::WarnAboutUnitsThatNeverTargeted()
{
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		float TargetSeconds = 0.f;
		for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
		{
			TargetSeconds += Unit.TargetSecondsByPlayer[Player];
		}
		if (TargetSeconds <= 0.f)
		{
			M_Warnings.Add(FString::Printf(TEXT("%s never held a target after the setup gate."), *Unit.Label));
		}
	}
}

void UFourPlayerSkirmishSubsystem::FinishPhase()
{
	RecordDeaths();
	RecheckWeaponOwnersAtEnd();
	WarnAboutUnitsThatNeverTargeted();
	ResolvePendingControllerProbes();
	TArray<FString> Reasons;
	const bool bPassed = EvaluateCriteria(Reasons);
	LogMatrix();
	Finish(bPassed, Reasons);
}

void UFourPlayerSkirmishSubsystem::BuildSummary(const bool bPassed, const TArray<FString>& Reasons,
                                                TArray<FString>& OutLines) const
{
	using namespace FourPlayerSkirmishPrivate;
	OutLines.Add(FString::Printf(TEXT("# Four-player skirmish: %s"), *GetPhaseName()));
	OutLines.Add(FString::Printf(TEXT("\n**%s**%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
	                             Reasons.Num() > 0 ? *(TEXT(": ") + FString::Join(Reasons, TEXT(", "))) : TEXT("")));
	OutLines.Add(M_Phase == EFourPlayerSkirmishPhase::Teams
		             ? FString(TEXT("\nCriteria (Teams {1,3} vs {2,4}): damage only crosses teams, zero damage between "
			             "allies or within a player, and every player damages at least one hostile player."))
		             : FString(TEXT("\nCriteria (free for all): every player damages at least two other players and no "
			             "player damages its own units.")));
	OutLines.Add(TEXT("Also checked (stricter than plan section 6, each a FAIL reason): every unit, soldier, turret and "
		"weapon state reports the owner it was spawned for at the setup gate and again at the end; no weapon holds a "
		"target of its own player or an ally; no damage is claimed by an owner outside 1..4; a hit unit's reported "
		"owner matches its spawn owner."));
	OutLines.Add(FString::Printf(TEXT("\nWritten %s. Arena %s, square side %.1f m, time limit %.0f s. Fight ended after "
		                             "%.1f s (%s). %d units, %d damage reports."), *FDateTime::Now().ToString(),
	                             *M_ArenaCentre.ToCompactString(), M_SquareSideCm / CmPerMetre, M_MaxFightGameSeconds,
	                             GetFightSeconds(), M_FightEndReason.IsEmpty() ? TEXT("-") : *M_FightEndReason,
	                             M_Units.Num(), M_DamageReportCount));
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		OutLines.Add(FString::Printf(TEXT("- P%d: team %d, hostile to %s"), Player, FRTSPlayers::GetPlayerTeam(Player),
		                             *GetHostilePlayersText(Player)));
	}
	OutLines.Add(TEXT("\n## Setup gate\n"));
	if (M_SetupLines.Num() == 0)
	{
		OutLines.Add(TEXT("Not reached."));
	}
	OutLines.Append(M_SetupLines);
	AppendControllersToSummary(OutLines);
	AppendControllerWavesToSummary(OutLines);
	AppendStrategicAIToSummary(OutLines);
	AppendOwnerChangeToSummary(OutLines);
	AppendMatrixToSummary(OutLines);
	AppendPlayersToSummary(OutLines);
	AppendUnitsToSummary(OutLines);
	AppendWeaponKindsToSummary(OutLines);
	OutLines.Add(TEXT("\n## Warnings\n"));
	if (M_Warnings.Num() == 0)
	{
		OutLines.Add(TEXT("None."));
	}
	for (const FString& Warning : M_Warnings)
	{
		OutLines.Add(TEXT("- ") + Warning);
	}
}

void UFourPlayerSkirmishSubsystem::AppendMatrixToSummary(TArray<FString>& OutLines) const
{
	OutLines.Add(TEXT("\n## Damage matrix (health really lost / hits; rows attack, columns are hit)\n"));
	OutLines.Add(TEXT("| Attacker \\ Victim | P1 | P2 | P3 | P4 |"));
	OutLines.Add(TEXT("|---|---|---|---|---|"));
	for (int32 Attacker = RTSPlayers::HumanPlayer; Attacker <= RTSPlayers::MaxPlayers + 1; ++Attacker)
	{
		// The row after the four players is slot 0: damage no damage source claimed (cook-off, engine fire).
		const int32 AttackerSlot = Attacker > RTSPlayers::MaxPlayers ? RTSPlayers::NeutralOwner : Attacker;
		TArray<FString> Cells;
		for (int32 Victim = RTSPlayers::HumanPlayer; Victim <= RTSPlayers::MaxPlayers; ++Victim)
		{
			const bool bIsFriendly = AttackerSlot != RTSPlayers::NeutralOwner
				&& not FRTSPlayers::AreHostile(AttackerSlot, Victim) && M_DamageMatrix[AttackerSlot][Victim] > 0.f;
			Cells.Add(FString::Printf(TEXT("%.0f / %d%s"), M_DamageMatrix[AttackerSlot][Victim],
			                          M_HitMatrix[AttackerSlot][Victim], bIsFriendly ? TEXT(" **FRIENDLY**") : TEXT("")));
		}
		const FString RowName = AttackerSlot == RTSPlayers::NeutralOwner
			                        ? FString(TEXT("unclaimed"))
			                        : FString::Printf(TEXT("P%d"), AttackerSlot);
		OutLines.Add(FString::Printf(TEXT("| %s | %s |"), *RowName, *FString::Join(Cells, TEXT(" | "))));
	}
	OutLines.Add(FString::Printf(TEXT("\nOutside the spawned groups (wrecks, scenery, map units): %.0f damage in %d hits, "
		                             "of which friendly %.0f."), M_OtherVictimDamage, M_OtherVictimHits,
	                             M_OtherVictimNonHostileDamage));
}

void UFourPlayerSkirmishSubsystem::AppendPlayersToSummary(TArray<FString>& OutLines) const
{
	OutLines.Add(TEXT("\n## Players\n"));
	OutLines.Add(TEXT("| Player | Hostile players damaged (needed) | Dmg to hostiles | Dmg to own/allies | Dmg taken | "
		"Dmg no unit held the victim for | Units alive at end |"));
	OutLines.Add(TEXT("|---|---|---|---|---|---|---|"));
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		float HostileDamage = 0.f;
		float FriendlyDamage = 0.f;
		float DamageTaken = 0.f;
		for (int32 Other = RTSPlayers::NeutralOwner; Other <= RTSPlayers::MaxPlayers; ++Other)
		{
			DamageTaken += M_DamageMatrix[Other][Player];
			const bool bIsHostile = FRTSPlayers::AreHostile(Player, Other);
			HostileDamage += bIsHostile ? M_DamageMatrix[Player][Other] : 0.f;
			FriendlyDamage += not bIsHostile && FRTSPlayers::IsValidPlayer(Other) ? M_DamageMatrix[Player][Other] : 0.f;
		}
		int32 LivingUnits = 0;
		for (const FFourPlayerSkirmishUnit& Unit : M_Units)
		{
			LivingUnits += Unit.OwningPlayer == Player && GetIsUnitAlive(Unit) ? 1 : 0;
		}
		OutLines.Add(FString::Printf(TEXT("| P%d | %d (%d) | %.0f | %.0f | %.0f | %.0f | %d |"), Player,
		                             CountHostilePlayersDamagedBy(Player), GetRequiredHostilePlayersDamaged(),
		                             HostileDamage, FriendlyDamage, DamageTaken, M_UnitUncreditedDamage[Player],
		                             LivingUnits));
	}
}

void UFourPlayerSkirmishSubsystem::AppendUnitsToSummary(TArray<FString>& OutLines) const
{
	using namespace FourPlayerSkirmishPrivate;
	OutLines.Add(TEXT("\n## Units\n"));
	OutLines.Add(TEXT("Exact dmg dealt: hits whose damage hook named one of the unit's own weapons as the shooter. "
		"Inferred dmg dealt: a hit of the unit's player counts for the units that held the victim as target in the 3 s "
		"before it, shared evenly. Fire events: rounds (bursts for random-burst weapons) by the owner of the target "
		"the weapon held.\n"));
	OutLines.Add(TEXT("| Unit | Setup | Weapons | Fire events at | Exact dmg dealt to | Inferred dmg dealt to | "
		"Dmg taken from | Unclaimed dmg taken | Target seconds on | Non-hostile targets | Died at |"));
	OutLines.Add(TEXT("|---|---|---|---|---|---|---|---|---|---|---|"));
	for (const FFourPlayerSkirmishUnit& Unit : M_Units)
	{
		const FString SetupText = not Unit.bIsSetupChecked ? FString(TEXT("not checked"))
			                          : Unit.bIsSetupOk ? FString(TEXT("ok")) : FString(TEXT("**FAIL**"));
		OutLines.Add(FString::Printf(TEXT("| %s | %s | %d | %s | %s | %s | %s | %.0f | %s | %d | %s |"), *Unit.Label,
		                             *SetupText, Unit.WeaponCount, *FormatPlayerCounts(Unit.FireEventsAtPlayer),
		                             *FormatPlayerValues(Unit.ExactDamageDealtToPlayer),
		                             *FormatPlayerValues(Unit.DamageDealtToPlayer),
		                             *FormatPlayerValues(Unit.DamageTakenFromPlayer),
		                             Unit.DamageTakenFromPlayer[RTSPlayers::NeutralOwner],
		                             *FormatPlayerValues(Unit.TargetSecondsByPlayer), Unit.NonHostileTargetSamples,
		                             *FormatSeconds(Unit.DeathSeconds)));
	}
}

void UFourPlayerSkirmishSubsystem::AppendControllersToSummary(TArray<FString>& OutLines) const
{
	OutLines.Add(TEXT("\n## Enemy controllers\n"));
	OutLines.Add(TEXT("Players 3 and 4 each need their own AEnemyController (spawned quiet by the harness when the map "
		"has none). For each controller the harness spawns one Pz IV H the way its waves do: the async spawner call "
		"with the controller's GetSpawnOwnerOverride() (no real wave runs), 45 m "
		"beyond its group's corner and out of the fight; the unit, its turrets and its weapon states must report the "
		"controller's player.\n"));
	if (M_ControllerLines.Num() == 0 && M_ControllerProbes.Num() == 0 && M_ControllerFailReasons.Num() == 0)
	{
		OutLines.Add(TEXT("Not reached."));
		return;
	}
	OutLines.Append(M_ControllerLines);
	OutLines.Add(TEXT("\n| Player | Controller | Source | Spawn path | Owner override | Unit | Reported owner | "
		"Weapon states (owner mismatches) | Result |"));
	OutLines.Add(TEXT("|---|---|---|---|---|---|---|---|---|"));
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		OutLines.Add(FString::Printf(TEXT("| P%d | %s | %s | %s | %d | %s | %d | %d (%d) | %s |"), Probe.Player,
		                             *Probe.ControllerName,
		                             Probe.bIsSpawnedByHarness ? TEXT("spawned by harness") : TEXT("map"),
		                             Probe.SpawnPath.IsEmpty() ? TEXT("-") : *Probe.SpawnPath,
		                             Probe.SpawnOwnerOverride,
		                             Probe.SpawnedUnitName.IsEmpty() ? TEXT("-") : *Probe.SpawnedUnitName,
		                             Probe.ReportedOwner, Probe.WeaponCount, Probe.WeaponMismatches,
		                             Probe.Result.IsEmpty() ? TEXT("pending") : *Probe.Result));
	}
	OutLines.Add(M_ControllerFailReasons.Num() > 0
		             ? TEXT("\nController FAIL reasons: ") + FString::Join(M_ControllerFailReasons, TEXT(", "))
		             : FString(TEXT("\nController checks: no FAIL reasons.")));
}

void UFourPlayerSkirmishSubsystem::LogMatrix() const
{
	for (int32 Attacker = RTSPlayers::NeutralOwner; Attacker <= RTSPlayers::MaxPlayers; ++Attacker)
	{
		TArray<FString> Cells;
		for (int32 Victim = RTSPlayers::HumanPlayer; Victim <= RTSPlayers::MaxPlayers; ++Victim)
		{
			Cells.Add(FString::Printf(TEXT("P%d=%.0f/%d"), Victim, M_DamageMatrix[Attacker][Victim],
			                          M_HitMatrix[Attacker][Victim]));
		}
		const FString RowName = Attacker == RTSPlayers::NeutralOwner
			                        ? FString(TEXT("unclaimed"))
			                        : FString::Printf(TEXT("P%d"), Attacker);
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS matrix %s -> %s"), *RowName,
		       *FString::Join(Cells, TEXT(" ")));
	}
}

FString UFourPlayerSkirmishSubsystem::GetPhaseName() const
{
	using namespace FourPlayerSkirmishPrivate;
	return M_Phase == EFourPlayerSkirmishPhase::Teams ? TeamsPhaseName : FreeForAllPhaseName;
}

void UFourPlayerSkirmishSubsystem::AppendLine(const FString& FilePath, const FString& Line) const
{
	// Appended per line so a crash mid-fight still leaves the hits so far.
	FFileHelper::SaveStringToFile(Line + TEXT("\n"), *FilePath, FFileHelper::EEncodingOptions::AutoDetect,
	                              &IFileManager::Get(), FILEWRITE_Append);
}

bool UFourPlayerSkirmishSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace FourPlayerSkirmishPrivate;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FourPlayerSkirmishGround), false);
	FHitResult GroundHit;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + GroundTraceUpCm);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - GroundTraceDownCm);
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE,
	                                             QueryParams))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

float UFourPlayerSkirmishSubsystem::GetFightSeconds() const
{
	return GetGameSeconds() - M_FightStartGameSeconds;
}

float UFourPlayerSkirmishSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UFourPlayerSkirmishSubsystem::Finish(const bool bPassed, const TArray<FString>& Reasons)
{
	if (M_Step == EFourPlayerSkirmishStep::Done)
	{
		return;
	}
	M_Step = EFourPlayerSkirmishStep::Done;
	if (not M_SummaryPath.IsEmpty())
	{
		TArray<FString> SummaryLines;
		BuildSummary(bPassed, Reasons, SummaryLines);
		FFileHelper::SaveStringToFile(FString::Join(SummaryLines, TEXT("\n")) + TEXT("\n"), *M_SummaryPath);
	}
	const FString ReasonText = Reasons.Num() > 0 ? FString::Join(Reasons, TEXT(";")) : FString(TEXT("none"));
	UE_LOG(LogRTSFourPlayerSkirmish, Display,
	       TEXT("RTS_FPS RESULT %s reasons=%s phase=%s end=%s fight_s=%.1f unclaimed_damage=%.0f summary=%s real_s=%.0f"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), *ReasonText, *GetPhaseName(),
	       M_FightEndReason.IsEmpty() ? TEXT("-") : *M_FightEndReason, GetFightSeconds(), GetUnclaimedDamageOnUnits(),
	       *M_SummaryPath, FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("FourPlayerSkirmish"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Strategic AI of the player 3 and 4 controllers
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::StartControllerStrategicAI()
{
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		AEnemyController* const Controller = Probe.Controller.Get();
		UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
			                                                ? Controller->GetEnemyStrategicAIComponent()
			                                                : nullptr;
		if (not IsValid(StrategicAI))
		{
			AddControllerFailure(FString::Printf(TEXT("strategic_ai_missing_P%d"), Probe.Player));
			continue;
		}
		const int32 Player = Probe.Player;
		// The designer-authored survival decision tree, applied the way survival mode applies it to player 2.
		const USurvivalWaveSettings* const WaveSettings = LoadObject<USurvivalWaveSettings>(
			nullptr, FourPlayerSkirmishPrivate::AuthoredTreeSettingsPath);
		const UStochasticDecisionTree* const AuthoredTree = IsValid(WaveSettings)
			                                                    ? WaveSettings->StrategicAI.DecisionTreeTemplate.Get()
			                                                    : nullptr;
		if (IsValid(AuthoredTree))
		{
			Controller->OverrideStrategicDecisionTree(AuthoredTree);
			M_ControllerLines.Add(FString::Printf(TEXT("- P%d strategy: authored tree of %s (%d actions)"), Player,
			                                      *GetNameSafe(WaveSettings),
			                                      AuthoredTree->GetActionDefinitions().Num()));
		}
		else
		{
			AddControllerFailure(FString::Printf(TEXT("authored_decision_tree_missing_P%d"), Player));
		}
		StrategicAI->OnStrategicAIResultsApplied.AddWeakLambda(
			this, [this, Player](const FStrategicAIResultBatch& ResultBatch)
			{
				OnControllerStrategicResults(Player, ResultBatch);
			});
		// The controller's own think loop: it stamps the controller's player on every batch it sends.
		StrategicAI->StartStrategicAIThinkingTimer();
	}
}

void UFourPlayerSkirmishSubsystem::QueueControllerUnitCountQueries()
{
	M_LastUnitCountQueryFightSeconds = GetFightSeconds();
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		const AEnemyController* const Controller = Probe.Controller.Get();
		UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
			                                                ? Controller->GetEnemyStrategicAIComponent()
			                                                : nullptr;
		if (not IsValid(StrategicAI) || GetHasConclusiveStrategicSample(Probe.Player))
		{
			continue;
		}
		FGetPlayerUnitCountsAndBase Request;
		Request.RequestID = M_NextUnitCountQueryId++;
		StrategicAI->QueueGetPlayerUnitCountsAndBaseRequest(Request);
	}
	// The decision tree attacks the bulks of hostile units on the blackboard; the controller's own bulk think step
	// runs every 25 s, so the same default query is queued here while the strategy orders are not settled.
	if (GetAreControllerStrategyOrdersSettled())
	{
		return;
	}
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		const AEnemyController* const Controller = Probe.Controller.Get();
		UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
			                                                ? Controller->GetEnemyStrategicAIComponent()
			                                                : nullptr;
		if (IsValid(StrategicAI))
		{
			FFindPlayerUnitBulkLocations BulkRequest;
			BulkRequest.RequestID = M_NextUnitCountQueryId++;
			StrategicAI->QueueFindPlayerUnitBulkLocationsRequest(BulkRequest);
		}
	}
}

void UFourPlayerSkirmishSubsystem::OnControllerStrategicResults(const int32 Player,
                                                                const FStrategicAIResultBatch& ResultBatch)
{
	if (M_Step != EFourPlayerSkirmishStep::Fight || ResultBatch.PlayerUnitCountsResults.IsEmpty())
	{
		return;
	}
	const UGameUnitManager* const UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return;
	}
	const uint8 OwnBit = FRTSPlayers::GetPlayerBit(Player);
	// The strategic AI counts squads (one per squad with living soldiers), not soldiers; the map's own units count too.
	auto CountSquadsOfPlayers = [UnitManager](const uint8 PlayerMask)
	{
		TSet<const AActor*> Squads;
		for (const ASquadUnit* const Soldier : UnitManager->GetSquadUnitsOfPlayers(PlayerMask))
		{
			const AActor* const Squad = IsValid(Soldier) ? Soldier->GetSquadControllerChecked().Get() : nullptr;
			if (IsValid(Squad))
			{
				Squads.Add(Squad);
			}
		}
		return Squads.Num();
	};
	FFourPlayerSkirmishStrategicSample Sample;
	Sample.Player = Player;
	Sample.FightSeconds = GetFightSeconds();
	Sample.ReportedSquads = ResultBatch.PlayerUnitCountsResults.Last().PlayerSquads;
	Sample.HostileSquads = CountSquadsOfPlayers(FRTSPlayers::GetHostileMask(Player));
	Sample.HumanSquads = CountSquadsOfPlayers(RTSPlayers::HumanPlayerBit);
	Sample.OtherPlayersSquads = CountSquadsOfPlayers(static_cast<uint8>(RTSPlayers::AllPlayersMask & ~OwnBit));
	// Conclusive: the count matches the hostile players and could not have come from the old human-only rule (when the
	// human is an enemy of the controller), nor, in the teams phase, from counting allies as enemies.
	const bool bMatchesHostile = Sample.ReportedSquads == Sample.HostileSquads;
	const bool bTellsHumanOnlyApart = not FRTSPlayers::IsHostileToHuman(Player)
		|| Sample.HostileSquads != Sample.HumanSquads;
	const bool bTellsAlliesApart = M_Phase == EFourPlayerSkirmishPhase::FreeForAll
		|| Sample.HostileSquads != Sample.OtherPlayersSquads;
	Sample.bIsConclusive = bMatchesHostile && bTellsHumanOnlyApart && bTellsAlliesApart;
	M_StrategicSamples.Add(Sample);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS strategic_ai P%d t=%.1f reported_squads=%d "
		       "hostile=%d human_only=%d all_others=%d conclusive=%d"), Player, Sample.FightSeconds,
	       Sample.ReportedSquads, Sample.HostileSquads, Sample.HumanSquads, Sample.OtherPlayersSquads,
	       Sample.bIsConclusive ? 1 : 0);
}

bool UFourPlayerSkirmishSubsystem::GetHasConclusiveStrategicSample(const int32 Player) const
{
	return M_StrategicSamples.ContainsByPredicate([Player](const FFourPlayerSkirmishStrategicSample& Sample)
	{
		return Sample.Player == Player && Sample.bIsConclusive;
	});
}

bool UFourPlayerSkirmishSubsystem::GetAreControllerStrategicChecksSettled() const
{
	if (GetFightSeconds() >= FourPlayerSkirmishPrivate::StrategicCheckTimeoutFightSeconds)
	{
		return true;
	}
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		if (Probe.Controller.IsValid() && not GetHasConclusiveStrategicSample(Probe.Player))
		{
			return false;
		}
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::AppendStrategicAIReasons(TArray<FString>& OutReasons) const
{
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		if (GetHasConclusiveStrategicSample(Probe.Player))
		{
			continue;
		}
		const int32 Samples = M_StrategicSamples.FilterByPredicate(
			[&Probe](const FFourPlayerSkirmishStrategicSample& Sample)
			{
				return Sample.Player == Probe.Player;
			}).Num();
		OutReasons.Add(FString::Printf(TEXT("strategic_ai_P%d_no_conclusive_hostile_count_in_%d_results"),
		                               Probe.Player, Samples));
	}
}

void UFourPlayerSkirmishSubsystem::AppendStrategicAIToSummary(TArray<FString>& OutLines) const
{
	OutLines.Add(TEXT("\n## Controller strategic AI\n"));
	OutLines.Add(TEXT("The player 3 and 4 controllers run their own strategic AI think loop; a \"count the enemy's units\" "
		"query (FGetPlayerUnitCountsAndBase) is queued on it every 5 s and sent by that loop with the controller's "
		"requester. Its squad count is compared with the unit manager's squads (with living soldiers) of: the players hostile to the "
		"controller, the human only (the old two-player rule) and all other players (allies included). A result is "
		"conclusive when it equals the hostile count and tells the other readings apart; each controller needs one.\n"));
	if (M_StrategicSamples.Num() == 0)
	{
		OutLines.Add(TEXT("No results."));
		return;
	}
	OutLines.Add(TEXT("| Controller of | Fight s | Reported enemy squads | Hostile players | Human only | "
		"All other players | Conclusive |"));
	OutLines.Add(TEXT("|---|---|---|---|---|---|---|"));
	for (const FFourPlayerSkirmishStrategicSample& Sample : M_StrategicSamples)
	{
		OutLines.Add(FString::Printf(TEXT("| P%d | %.1f | %d | %d | %d | %d | %s |"), Sample.Player, Sample.FightSeconds,
		                             Sample.ReportedSquads, Sample.HostileSquads, Sample.HumanSquads,
		                             Sample.OtherPlayersSquads, Sample.bIsConclusive ? TEXT("yes") : TEXT("no")));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Runtime owner change between the human and an AI player
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::SpawnOwnerChangeUnits()
{
	using namespace FourPlayerSkirmishPrivate;
	// Human -> AI and AI -> human; both tanks stand on the free diagonals of players 1 and 2, out of the fight.
	struct FOwnerChangePlan
	{
		int32 From = RTSPlayers::NeutralOwner;
		int32 To = RTSPlayers::NeutralOwner;
	};
	const FOwnerChangePlan Plans[] = {{RTSPlayers::HumanPlayer, 3}, {RTSPlayers::FirstAIPlayer, RTSPlayers::HumanPlayer}};
	UClass* const TankClass = LoadClass<AActor>(nullptr, GetTankSpec().ClassPath);
	for (const FOwnerChangePlan& Plan : Plans)
	{
		const FVector Location = GetControllerProbeLocation(Plan.From);
		AActor* const Tank = IsValid(TankClass)
			                     ? FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
				                     *GetWorld(), TankClass,
				                     FTransform((-GetOutwardDirectionOfPlayer(Plan.From)).Rotation(), Location),
				                     static_cast<uint8>(Plan.From))
			                     : nullptr;
		const FString Label = FString::Printf(TEXT("OwnerChange_P%d_to_P%d"), Plan.From, Plan.To);
		if (not IsValid(Tank))
		{
			M_OwnerChangeFailReasons.Add(Label + TEXT("_spawn_failed"));
			continue;
		}
		M_OwnerChangeCases.Add({Tank, Label, Plan.From, Plan.To});
	}
}

void UFourPlayerSkirmishSubsystem::RunOwnerChangeChecks()
{
	for (const FFourPlayerSkirmishOwnerChangeCase& Case : M_OwnerChangeCases)
	{
		CheckOwnerChangeCase(Case);
	}
}

void UFourPlayerSkirmishSubsystem::CheckOwnerChangeCase(const FFourPlayerSkirmishOwnerChangeCase& Case)
{
	AActor* const Tank = Case.Actor.Get();
	URTSComponent* const RTSComponent = IsValid(Tank) ? Tank->FindComponentByClass<URTSComponent>() : nullptr;
	if (not IsValid(RTSComponent))
	{
		M_OwnerChangeFailReasons.Add(Case.Label + TEXT("_no_unit"));
		return;
	}
	// Control: the test must see the old side before the change, or a pass after it proves nothing.
	const int32 WrongBefore = CheckShotsAtOwnerChangeUnit(Case, Case.FromPlayer, TEXT("before"));
	RTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(Case.ToPlayer));
	const int32 WrongAfter = CheckShotsAtOwnerChangeUnit(Case, Case.ToPlayer, TEXT("after"));
	FFourPlayerSkirmishUnit Record;
	Record.Actor = Tank;
	Record.OwningPlayer = Case.ToPlayer;
	CollectWeaponCarriers(Record);
	const int32 WeaponMismatches = CountWeaponOwnerMismatches(Record);
	const int32 ReportedOwner = FRTSPlayers::GetOwningPlayerOfActor(Tank);
	// No body may stay on the old side's object channel, and no hull blocker box on the old side's hull channel.
	const ECollisionChannel OldOwnChannel = FRTSPlayerCollision::GetOwnerObjectChannel(Case.FromPlayer);
	const ECollisionChannel OldHullChannel = FRTSPlayers::IsHumanPlayer(Case.FromPlayer)
		                                         ? ECC_Vehicle
		                                         : COLLISION_OBJ_ENEMY_HULL;
	TArray<UPrimitiveComponent*> Primitives;
	Tank->GetComponents<UPrimitiveComponent>(Primitives);
	int32 BodiesOnOldChannel = 0;
	for (const UPrimitiveComponent* const Primitive : Primitives)
	{
		if (not IsValid(Primitive))
		{
			continue;
		}
		const bool bIsOldHullBox = Primitive->IsA<UBoxComponent>()
			&& Primitive->GetCollisionObjectType() == OldHullChannel;
		BodiesOnOldChannel += Primitive->GetCollisionObjectType() == OldOwnChannel || bIsOldHullBox ? 1 : 0;
	}
	const TPair<int32, const TCHAR*> Problems[] = {
		{WrongBefore, TEXT("_shots_before")}, {WrongAfter, TEXT("_shots_after")},
		{WeaponMismatches, TEXT("_weapon_owner")}, {ReportedOwner != Case.ToPlayer ? 1 : 0, TEXT("_unit_owner")},
		{BodiesOnOldChannel, TEXT("_bodies_on_old_object_channel")},
	};
	for (const TPair<int32, const TCHAR*>& Problem : Problems)
	{
		if (Problem.Key > 0)
		{
			M_OwnerChangeFailReasons.Add(FString::Printf(TEXT("%s%s=%d"), *Case.Label, Problem.Value, Problem.Key));
		}
	}
	M_OwnerChangeLines.Add(FString::Printf(TEXT("| %s | %d | %d | %d weapon states (%d owner mismatches) | %d |"),
	                                       *Case.Label, Case.FromPlayer, ReportedOwner, Record.WeaponCount,
	                                       WeaponMismatches, BodiesOnOldChannel));
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS owner_change %s reported_owner=%d weapons=%d "
		       "weapon_mismatches=%d wrong_shots_before=%d wrong_shots_after=%d bodies_on_old_channel=%d"),
	       *Case.Label, ReportedOwner, Record.WeaponCount, WeaponMismatches, WrongBefore, WrongAfter, BodiesOnOldChannel);
}

int32 UFourPlayerSkirmishSubsystem::CheckShotsAtOwnerChangeUnit(const FFourPlayerSkirmishOwnerChangeCase& Case,
                                                                const int32 ExpectedOwner, const TCHAR* StageName)
{
	const AActor* const Tank = Case.Actor.Get();
	if (not IsValid(Tank))
	{
		return 1;
	}
	int32 WrongShots = 0;
	TArray<FString> ShotTexts;
	for (int32 Shooter = RTSPlayers::HumanPlayer; Shooter <= RTSPlayers::MaxPlayers; ++Shooter)
	{
		const bool bShouldHit = FRTSPlayers::AreHostile(Shooter, ExpectedOwner);
		const bool bDidHit = GetDoesShotOfPlayerHitActor(Shooter, *Tank);
		WrongShots += bShouldHit != bDidHit ? 1 : 0;
		ShotTexts.Add(FString::Printf(TEXT("P%d %s%s"), Shooter, bDidHit ? TEXT("hits") : TEXT("passes"),
		                              bShouldHit != bDidHit ? TEXT(" **WRONG**") : TEXT("")));
	}
	M_OwnerChangeLines.Add(FString::Printf(TEXT("- %s, %s (owner %d): %s"), *Case.Label, StageName, ExpectedOwner,
	                                       *FString::Join(ShotTexts, TEXT(", "))));
	return WrongShots;
}

bool UFourPlayerSkirmishSubsystem::GetDoesShotOfPlayerHitActor(const int32 Player, const AActor& Target) const
{
	using namespace FourPlayerSkirmishPrivate;
	// Shot from 9 m outside the arena at the middle of the tank, so nothing of the fight stands in between.
	constexpr float ShotDistanceCm = 900.f;
	constexpr float ShotHeightCm = 60.f;
	const FVector Centre = Target.GetComponentsBoundingBox(true).GetCenter();
	const FVector Outward = (Centre - M_ArenaCentre).GetSafeNormal2D();
	const FVector Start = Centre + Outward * ShotDistanceCm + FVector(0.f, 0.f, ShotHeightCm);
	FCollisionQueryParams QueryParams(FName(TEXT("FourPlayerSkirmishOwnerChangeShot")), false);
	FRTSPlayerCollision::ApplyWeaponIgnoreMask(QueryParams, Player);
	FHitResult Hit;
	if (not GetWorld()->LineTraceSingleByChannel(Hit, Start, Centre, FRTSPlayerCollision::GetWeaponTraceChannel(Player),
	                                             QueryParams))
	{
		return false;
	}
	// Turrets are attached child actors of the tank.
	const AActor* HitActor = Hit.GetActor();
	for (int32 Step = 0; Step < MaxOwnerWalkSteps && IsValid(HitActor); ++Step)
	{
		if (HitActor == &Target)
		{
			return true;
		}
		HitActor = HitActor->GetAttachParentActor();
	}
	return false;
}

void UFourPlayerSkirmishSubsystem::AppendOwnerChangeToSummary(TArray<FString>& OutLines) const
{
	OutLines.Add(TEXT("\n## Runtime owner change\n"));
	OutLines.Add(TEXT("Two tanks out of the fight change owner at the setup gate with URTSComponent::"
		"SetOwningPlayerRuntime: human -> AI player 3 and AI player 2 -> human. Before and after, a weapon trace of "
		"every player (its channel and ignore mask) must stop on the tank exactly when that player is hostile to the "
		"tank's owner; afterwards the tank, its turret and weapon states report the new owner and no body stays on "
		"the old side's object channel.\n"));
	if (M_OwnerChangeLines.Num() == 0)
	{
		OutLines.Add(TEXT("Not reached."));
		return;
	}
	TArray<FString> ShotLines;
	TArray<FString> TableLines;
	for (const FString& Line : M_OwnerChangeLines)
	{
		(Line.StartsWith(TEXT("|")) ? TableLines : ShotLines).Add(Line);
	}
	OutLines.Append(ShotLines);
	OutLines.Add(TEXT("\n| Tank | Owner before | Owner after | Weapons | Bodies left on old object channel |"));
	OutLines.Add(TEXT("|---|---|---|---|---|"));
	OutLines.Append(TableLines);
}

// ---------------------------------------------------------------------------------------------------------------------
// Attack waves of the player 3 and 4 controllers
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::StartControllerWaves()
{
	M_ControllerWaves.Reset();
	for (const FFourPlayerSkirmishControllerProbe& Probe : M_ControllerProbes)
	{
		FFourPlayerSkirmishControllerWave Wave;
		Wave.Player = Probe.Player;
		Wave.Controller = Probe.Controller;
		M_ControllerWaves.Add(Wave);
		// The strategy entry of the same controller; filled when its probe tank comes up (StartControllerStrategyOrder).
		Wave.bIsStrategyOrder = true;
		M_ControllerWaves.Add(Wave);
	}
	// Sized once above: the spawn callbacks address waves by index.
	for (int32 WaveIndex = 0; WaveIndex < M_ControllerWaves.Num(); ++WaveIndex)
	{
		if (not M_ControllerWaves[WaveIndex].bIsStrategyOrder)
		{
			StartControllerWave(WaveIndex);
		}
	}
}

void UFourPlayerSkirmishSubsystem::StartControllerStrategyOrder(const FFourPlayerSkirmishControllerProbe& Probe)
{
	AEnemyController* const Controller = Probe.Controller.Get();
	UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
		                                                ? Controller->GetEnemyStrategicAIComponent()
		                                                : nullptr;
	AActor* const ProbeUnit = Probe.SpawnedUnit.Get();
	FFourPlayerSkirmishControllerWave* const Entry = M_ControllerWaves.FindByPredicate(
		[&Probe](const FFourPlayerSkirmishControllerWave& Wave)
		{
			return Wave.bIsStrategyOrder && Wave.Player == Probe.Player;
		});
	if (Entry == nullptr || not IsValid(StrategicAI) || not IsValid(ProbeUnit))
	{
		return;
	}
	FFourPlayerSkirmishWaveUnit WaveUnit;
	WaveUnit.Actor = ProbeUnit;
	WaveUnit.Name = ProbeUnit->GetName();
	WaveUnit.ReportedOwner = FRTSPlayers::GetOwningPlayerOfActor(ProbeUnit);
	WaveUnit.ArrivedGameSeconds = GetGameSeconds();
	WaveUnit.StartLocation = ProbeUnit->GetActorLocation();
	Entry->Units.Add(WaveUnit);
	// No wave: the id only marks the entry as started.
	Entry->WaveId = 0;
	Entry->StartGameSeconds = GetGameSeconds();
	// From here the tank is an idle unit of the controller; only its decision tree can send it anywhere.
	StrategicAI->RegisterDroppedUnitWithBlackboardWhenReady(ProbeUnit);
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controller_strategy P%d idle unit=%s handed to %s"),
	       Probe.Player, *WaveUnit.Name, *GetNameSafe(Controller));
}

bool UFourPlayerSkirmishSubsystem::GetAreControllerStrategyOrdersSettled() const
{
	for (const FFourPlayerSkirmishControllerWave& Wave : M_ControllerWaves)
	{
		if (Wave.bIsStrategyOrder && not GetIsControllerWaveSettled(Wave))
		{
			return false;
		}
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::StartControllerWave(const int32 WaveIndex)
{
	using namespace FourPlayerSkirmishPrivate;
	FFourPlayerSkirmishControllerWave& Wave = M_ControllerWaves[WaveIndex];
	AEnemyController* const Controller = Wave.Controller.Get();
	if (not IsValid(Controller))
	{
		Wave.Result = TEXT("**FAIL**: no controller");
		return;
	}
	Controller->SetEnemyWaveSupply(ControllerWaveSupply);
	const FVector Outward = GetOutwardDirectionOfPlayer(Wave.Player);
	const FVector Sideways(-Outward.Y, Outward.X, 0.f);
	const FVector SpawnCentre = M_ArenaCentre + Outward * (GetCornerRadiusCm(M_SquareSideCm) +
		ControllerWaveOutwardMarginCm);
	FAttackWaveRequest Request;
	Request.WaveType = EEnemyWaveType::Wave_NoOwningBuilding;
	Request.IterationCount = 1;
	Request.Movement = EAttackWaveMovement::AttackMove;
	Request.Waypoints = {M_ArenaCentre};
	Request.FinalWaypointDirection = (-Outward).Rotation();
	Request.WaveCreator = Controller;
	for (int32 UnitSlot = 0; UnitSlot < ControllerWaveUnitCount; ++UnitSlot)
	{
		const float SideOffset = (UnitSlot - (ControllerWaveUnitCount - 1) * 0.5f) * ControllerWaveElementSpacingCm;
		FVector SpawnLocation = SpawnCentre + Sideways * SideOffset;
		float GroundZ = M_ArenaCentre.Z;
		(void)TryGetGroundZ(SpawnLocation, GroundZ);
		SpawnLocation.Z = GroundZ + GetTankSpec().SpawnLiftCm;
		FAttackWaveElement Element;
		Element.SpawnLocation = SpawnLocation;
		Element.UnitOptions = {FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_H))};
		Request.WaveElements.Add(Element);
	}
	Request.OnUnitsSpawned = FOnAttackWaveUnitsSpawned::CreateWeakLambda(
		this, [this, WaveIndex](const int32 WaveId, const TArray<AActor*>& SpawnedUnits)
		{
			static_cast<void>(WaveId);
			OnControllerWaveUnitsSpawned(WaveIndex, SpawnedUnits);
		});
	Wave.StartGameSeconds = GetGameSeconds();
	const int32 WaveId = Controller->StartWaveFromRequest(Request);
	// The request may have spawned synchronously; Wave is still valid, the array is never resized after setup.
	M_ControllerWaves[WaveIndex].WaveId = WaveId;
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controller_wave P%d controller=%s wave_id=%d units=%d "
		       "from=%s to=%s"), Wave.Player, *GetNameSafe(Controller), WaveId, ControllerWaveUnitCount,
	       *SpawnCentre.ToCompactString(), *M_ArenaCentre.ToCompactString());
	if (WaveId == INDEX_NONE)
	{
		M_ControllerWaves[WaveIndex].Result = TEXT("**FAIL**: StartWaveFromRequest refused the wave");
	}
}

void UFourPlayerSkirmishSubsystem::OnControllerWaveUnitsSpawned(const int32 WaveIndex,
                                                                const TArray<AActor*>& SpawnedUnits)
{
	if (not M_ControllerWaves.IsValidIndex(WaveIndex))
	{
		return;
	}
	FFourPlayerSkirmishControllerWave& Wave = M_ControllerWaves[WaveIndex];
	for (AActor* const SpawnedUnit : SpawnedUnits)
	{
		if (not IsValid(SpawnedUnit))
		{
			continue;
		}
		FFourPlayerSkirmishWaveUnit WaveUnit;
		WaveUnit.Actor = SpawnedUnit;
		WaveUnit.Name = SpawnedUnit->GetName();
		WaveUnit.ReportedOwner = FRTSPlayers::GetOwningPlayerOfActor(SpawnedUnit);
		WaveUnit.ArrivedGameSeconds = GetGameSeconds();
		WaveUnit.StartLocation = SpawnedUnit->GetActorLocation();
		Wave.Units.Add(WaveUnit);
		UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS controller_wave P%d unit=%s owner=%d after=%.1f s"),
		       Wave.Player, *WaveUnit.Name, WaveUnit.ReportedOwner, WaveUnit.ArrivedGameSeconds - Wave.StartGameSeconds);
	}
}

void UFourPlayerSkirmishSubsystem::TickControllerWaves()
{
	using namespace FourPlayerSkirmishPrivate;
	for (int32 WaveIndex = 0; WaveIndex < M_ControllerWaves.Num(); ++WaveIndex)
	{
		for (FFourPlayerSkirmishWaveUnit& WaveUnit : M_ControllerWaves[WaveIndex].Units)
		{
			// Turrets set up their weapon states a moment after BeginPlay, as for the groups.
			const bool bIsSettled = GetGameSeconds() - WaveUnit.ArrivedGameSeconds >= SettleGameSeconds;
			if (not WaveUnit.bAreWeaponsBound && bIsSettled && WaveUnit.Actor.IsValid())
			{
				BindWaveUnitWeapons(WaveIndex, WaveUnit);
			}
			if (WaveUnit.Actor.IsValid())
			{
				WaveUnit.MaxMovedCm = FMath::Max(WaveUnit.MaxMovedCm, static_cast<float>(
					                                 FVector::Dist2D(WaveUnit.StartLocation,
					                                                 WaveUnit.Actor->GetActorLocation())));
			}
		}
	}
}

void UFourPlayerSkirmishSubsystem::BindWaveUnitWeapons(const int32 WaveIndex, FFourPlayerSkirmishWaveUnit& WaveUnit)
{
	WaveUnit.bAreWeaponsBound = true;
	FFourPlayerSkirmishUnit Record;
	Record.Actor = WaveUnit.Actor;
	Record.Kind = EFourPlayerSkirmishUnitKind::Tank;
	Record.OwningPlayer = M_ControllerWaves[WaveIndex].Player;
	CollectWeaponCarriers(Record);
	WaveUnit.WeaponOwnerMismatches = CountWeaponOwnerMismatches(Record);
	WaveUnit.WeaponCount = Record.WeaponCount;
	for (const TWeakObjectPtr<UObject>& Carrier : Record.WeaponCarriers)
	{
		IWeaponOwner* const WeaponOwner = Cast<IWeaponOwner>(Carrier.Get());
		if (WeaponOwner == nullptr)
		{
			continue;
		}
		const TArray<UWeaponState*> Weapons = WeaponOwner->GetWeapons();
		for (UWeaponState* const Weapon : Weapons)
		{
			if (not IsValid(Weapon))
			{
				continue;
			}
			// Same drop-of-the-count rule as the groups' counters: reloads and repeated burst counts are no shots.
			Weapon->OnMagConsumed.AddWeakLambda(this, [this, WaveIndex, Carrier, LastMagCount = MAX_int32](
			                                    const int32 MagCount) mutable
			                                    {
				                                    const bool bHasFired = MagCount < LastMagCount;
				                                    LastMagCount = MagCount;
				                                    if (bHasFired)
				                                    {
					                                    OnWaveWeaponFired(WaveIndex, Carrier);
				                                    }
			                                    });
		}
	}
}

void UFourPlayerSkirmishSubsystem::OnWaveWeaponFired(const int32 WaveIndex, const TWeakObjectPtr<UObject>& Carrier)
{
	if (M_Step != EFourPlayerSkirmishStep::Fight || not M_ControllerWaves.IsValidIndex(WaveIndex))
	{
		return;
	}
	const AActor* const Target = FourPlayerSkirmishPrivate::GetHeldTargetOfCarrier(Carrier.Get());
	const int32 TargetUnitIndex = IsValid(Target) ? FindUnitIndexByActor(Target) : INDEX_NONE;
	const int32 TargetOwner = M_Units.IsValidIndex(TargetUnitIndex)
		                          ? M_Units[TargetUnitIndex].OwningPlayer
		                          : FRTSPlayers::GetOwningPlayerOfActor(Target);
	const int32 Slot = FRTSPlayers::IsValidPlayer(TargetOwner) ? TargetOwner : RTSPlayers::NeutralOwner;
	++M_ControllerWaves[WaveIndex].FireEventsAtPlayer[Slot];
}

int32 UFourPlayerSkirmishSubsystem::FindControllerWaveIndexOfActor(const AActor* Actor) const
{
	using namespace FourPlayerSkirmishPrivate;
	for (int32 Step = 0; Step < MaxOwnerWalkSteps && IsValid(Actor); ++Step)
	{
		for (int32 WaveIndex = 0; WaveIndex < M_ControllerWaves.Num(); ++WaveIndex)
		{
			for (const FFourPlayerSkirmishWaveUnit& WaveUnit : M_ControllerWaves[WaveIndex].Units)
			{
				if (WaveUnit.Actor.Get() == Actor)
				{
					return WaveIndex;
				}
			}
		}
		Actor = Actor->GetAttachParentActor();
	}
	return INDEX_NONE;
}

bool UFourPlayerSkirmishSubsystem::CreditControllerWaveHit(const FMapTestDamageReport& Report, const int32 VictimOwner)
{
	const int32 WaveIndex = FindControllerWaveIndexOfActor(Report.AttackingActor.Get());
	if (not M_ControllerWaves.IsValidIndex(WaveIndex) || not FRTSPlayers::IsValidPlayer(VictimOwner))
	{
		return false;
	}
	FFourPlayerSkirmishControllerWave& Wave = M_ControllerWaves[WaveIndex];
	Wave.ExactDamageToPlayer[VictimOwner] += Report.AppliedDamage;
	++Wave.ExactHitsOnPlayer[VictimOwner];
	return true;
}

bool UFourPlayerSkirmishSubsystem::GetIsControllerWaveSettled(const FFourPlayerSkirmishControllerWave& Wave) const
{
	using namespace FourPlayerSkirmishPrivate;
	if (Wave.WaveId == INDEX_NONE)
	{
		return true;
	}
	const float WaitedSeconds = GetGameSeconds() - Wave.StartGameSeconds;
	if (Wave.Units.Num() == 0)
	{
		return WaitedSeconds >= ControllerWaveArrivalTimeoutGameSeconds;
	}
	bool bHasFiredAtHostile = false;
	bool bHasHitHostile = false;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		const bool bIsHostile = FRTSPlayers::AreHostile(Wave.Player, Player);
		bHasFiredAtHostile |= bIsHostile && Wave.FireEventsAtPlayer[Player] > 0;
		bHasHitHostile |= bIsHostile && Wave.ExactHitsOnPlayer[Player] > 0;
	}
	bool bIsAnyUnitAlive = false;
	for (const FFourPlayerSkirmishWaveUnit& WaveUnit : Wave.Units)
	{
		// IsUnitAlive is not const on AActor.
		AActor* const UnitActor = WaveUnit.Actor.Get();
		bIsAnyUnitAlive |= IsValid(UnitActor) && UnitActor->IsUnitAlive();
	}
	return (bHasFiredAtHostile && bHasHitHostile) || not bIsAnyUnitAlive
		|| WaitedSeconds >= ControllerWaveEngageTimeoutGameSeconds;
}

bool UFourPlayerSkirmishSubsystem::GetAreControllerWavesSettled() const
{
	for (const FFourPlayerSkirmishControllerWave& Wave : M_ControllerWaves)
	{
		if (not GetIsControllerWaveSettled(Wave))
		{
			return false;
		}
	}
	return true;
}

void UFourPlayerSkirmishSubsystem::AppendControllerWaveReasons(TArray<FString>& OutReasons) const
{
	using namespace FourPlayerSkirmishPrivate;
	// Index 0: attack waves, 1: strategy orders.
	uint8 HostilePlayersFiredAtMask[2] = {0, 0};
	for (const FFourPlayerSkirmishControllerWave& Wave : M_ControllerWaves)
	{
		const int32 Player = Wave.Player;
		const TCHAR* const Prefix = Wave.bIsStrategyOrder ? TEXT("controller_strategy") : TEXT("controller_wave");
		uint8& SourceMask = HostilePlayersFiredAtMask[Wave.bIsStrategyOrder ? 1 : 0];
		if (Wave.WaveId == INDEX_NONE)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_not_started_P%d"), Prefix, Player));
			continue;
		}
		if (Wave.Units.Num() == 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_no_units_P%d"), Prefix, Player));
			continue;
		}
		if (Wave.bIsStrategyOrder)
		{
			// Ordered by the decision tree: the idle tank must have left its spot behind the group for the fight.
			constexpr float MinStrategyMoveCm = 1500.f;
			float MostMovedCm = 0.f;
			for (const FFourPlayerSkirmishWaveUnit& WaveUnit : Wave.Units)
			{
				MostMovedCm = FMath::Max(MostMovedCm, WaveUnit.MaxMovedCm);
			}
			if (MostMovedCm < MinStrategyMoveCm)
			{
				OutReasons.Add(FString::Printf(TEXT("%s_unit_never_moved_P%d=%.0fm"), Prefix, Player,
				                               MostMovedCm / CmPerMetre));
			}
		}
		int32 OwnerProblems = 0;
		for (const FFourPlayerSkirmishWaveUnit& WaveUnit : Wave.Units)
		{
			OwnerProblems += (WaveUnit.ReportedOwner != Player ? 1 : 0) + WaveUnit.WeaponOwnerMismatches;
		}
		if (OwnerProblems > 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_owner_mismatches_P%d=%d"), Prefix, Player, OwnerProblems));
		}
		int32 FiredAtHostile = 0;
		int32 FiredAtNonHostile = 0;
		int32 HitsOnHostile = 0;
		for (int32 Target = RTSPlayers::HumanPlayer; Target <= RTSPlayers::MaxPlayers; ++Target)
		{
			const bool bIsHostile = FRTSPlayers::AreHostile(Player, Target);
			(bIsHostile ? FiredAtHostile : FiredAtNonHostile) += Wave.FireEventsAtPlayer[Target];
			HitsOnHostile += bIsHostile ? Wave.ExactHitsOnPlayer[Target] : 0;
			if (bIsHostile && Wave.FireEventsAtPlayer[Target] > 0)
			{
				SourceMask |= FRTSPlayers::GetPlayerBit(Target);
			}
		}
		if (FiredAtHostile == 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_never_fired_P%d"), Prefix, Player));
		}
		if (HitsOnHostile == 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_never_hit_P%d"), Prefix, Player));
		}
		if (FiredAtNonHostile > 0)
		{
			OutReasons.Add(FString::Printf(TEXT("%s_fired_at_non_hostile_P%d=%d"), Prefix, Player, FiredAtNonHostile));
		}
	}
	// The waves (two tanks per controller) must together engage several players; a strategy order is one tank.
	const int32 PlayersEngagedByWaves = FMath::CountBits(HostilePlayersFiredAtMask[0]);
	if (M_ControllerWaves.Num() > 0 && PlayersEngagedByWaves < ControllerWavesHostilePlayersRequired)
	{
		OutReasons.Add(FString::Printf(TEXT("controller_waves_engaged_only_%d_players"), PlayersEngagedByWaves));
	}
}

void UFourPlayerSkirmishSubsystem::AppendControllerWavesToSummary(TArray<FString>& OutLines) const
{
	using namespace FourPlayerSkirmishPrivate;
	OutLines.Add(TEXT("\n## Controller attack waves\n"));
	OutLines.Add(FString::Printf(TEXT("The enemy controllers of players 3 and 4 each start one attack-move wave of %d "
		                             "Pz IV H themselves (AEnemyController::StartWaveFromRequest: the controller's own "
		                             "spawning with its owner override, formation and attack-move orders) from %.0f m "
		                             "beyond their group's corner to the arena centre. Each wave must come up owned by "
		                             "its controller's player, fire at and hit a hostile player, never fire at a "
		                             "non-hostile one; together the waves must engage %d different players.\n"),
	                             ControllerWaveUnitCount, ControllerWaveOutwardMarginCm / CmPerMetre,
	                             ControllerWavesHostilePlayersRequired));
	if (M_ControllerWaves.Num() == 0)
	{
		OutLines.Add(TEXT("Not reached."));
		return;
	}
	OutLines.Add(TEXT("Strategy rows: the controller's probe tank, idle 45 m behind its group, is handed to the "
		"controller (RegisterDroppedUnitWithBlackboardWhenReady). Its decision tree is the designer-authored survival tree "
		"(DA_SurvivalWaveSettings, applied with AEnemyController::OverrideStrategicDecisionTree as survival mode does), fed "
		"by the controller's own strategic queries for bulks of hostile units; nothing else orders the tank. It must move at least 15 m, fire at and hit a hostile player and never fire at "
		"an ally.\n"));
	OutLines.Add(TEXT("| Player | Source | Controller | Wave id | Units (reported owner, weapon states, owner mismatches, "
		"moved m) | Fire events at | Exact hits on | Exact dmg dealt to |"));
	OutLines.Add(TEXT("|---|---|---|---|---|---|---|---|"));
	for (const FFourPlayerSkirmishControllerWave& Wave : M_ControllerWaves)
	{
		TArray<FString> UnitTexts;
		for (const FFourPlayerSkirmishWaveUnit& WaveUnit : Wave.Units)
		{
			UnitTexts.Add(FString::Printf(TEXT("%s (P%d, %d, %d, %.0f)"), *WaveUnit.Name, WaveUnit.ReportedOwner,
			                              WaveUnit.WeaponCount, WaveUnit.WeaponOwnerMismatches,
			                              WaveUnit.MaxMovedCm / CmPerMetre));
		}
		OutLines.Add(FString::Printf(TEXT("| P%d | %s | %s | %d | %s | %s | %s | %s |"), Wave.Player,
		                             Wave.bIsStrategyOrder ? TEXT("decision tree") : TEXT("attack wave"),
		                             *GetNameSafe(Wave.Controller.Get()), Wave.WaveId,
		                             UnitTexts.Num() > 0 ? *FString::Join(UnitTexts, TEXT("; ")) : TEXT("none"),
		                             *FormatPlayerCounts(Wave.FireEventsAtPlayer),
		                             *FormatPlayerCounts(Wave.ExactHitsOnPlayer),
		                             *FormatPlayerValues(Wave.ExactDamageToPlayer)));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Per-weapon attribution
// ---------------------------------------------------------------------------------------------------------------------

void UFourPlayerSkirmishSubsystem::BindWeaponFireCounters()
{
	int32 BoundWeapons = 0;
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		// Copied: binding never touches the unit records, the copy only keeps the loop independent of them.
		const TArray<TWeakObjectPtr<UObject>> Carriers = M_Units[UnitIndex].WeaponCarriers;
		for (const TWeakObjectPtr<UObject>& Carrier : Carriers)
		{
			IWeaponOwner* const WeaponOwner = Cast<IWeaponOwner>(Carrier.Get());
			if (WeaponOwner == nullptr)
			{
				continue;
			}
			const TArray<UWeaponState*> Weapons = WeaponOwner->GetWeapons();
			for (UWeaponState* const Weapon : Weapons)
			{
				if (not IsValid(Weapon))
				{
					continue;
				}
				// The delegate also reports a refilled magazine after a reload and repeats the last count when a burst
				// ends; only a drop of the count is a round (or a random burst) fired.
				Weapon->OnMagConsumed.AddWeakLambda(this, [this, UnitIndex, Carrier, LastMagCount = MAX_int32](
				                                    const int32 MagCount) mutable
				                                    {
					                                    const bool bHasFired = MagCount < LastMagCount;
					                                    LastMagCount = MagCount;
					                                    if (bHasFired)
					                                    {
						                                    OnWeaponFired(UnitIndex, Carrier);
					                                    }
				                                    });
				++BoundWeapons;
			}
		}
	}
	UE_LOG(LogRTSFourPlayerSkirmish, Display, TEXT("RTS_FPS fire_counters bound weapons=%d"), BoundWeapons);
}

void UFourPlayerSkirmishSubsystem::OnWeaponFired(const int32 UnitIndex, const TWeakObjectPtr<UObject>& Carrier)
{
	if (M_Step != EFourPlayerSkirmishStep::Fight || not M_Units.IsValidIndex(UnitIndex))
	{
		return;
	}
	const AActor* const Target = FourPlayerSkirmishPrivate::GetHeldTargetOfCarrier(Carrier.Get());
	const int32 TargetUnitIndex = IsValid(Target) ? FindUnitIndexByActor(Target) : INDEX_NONE;
	const int32 TargetOwner = M_Units.IsValidIndex(TargetUnitIndex)
		                          ? M_Units[TargetUnitIndex].OwningPlayer
		                          : FRTSPlayers::GetOwningPlayerOfActor(Target);
	const int32 Slot = FRTSPlayers::IsValidPlayer(TargetOwner) ? TargetOwner : RTSPlayers::NeutralOwner;
	++M_Units[UnitIndex].FireEventsAtPlayer[Slot];
}

FString UFourPlayerSkirmishSubsystem::CreditExactAttackingUnit(const FMapTestDamageReport& Report,
                                                               const int32 AttackerSlot, const int32 VictimOwner)
{
	const int32 ShooterUnitIndex = FindUnitIndexByActor(Report.AttackingActor.Get());
	if (not M_Units.IsValidIndex(ShooterUnitIndex))
	{
		return FString();
	}
	FFourPlayerSkirmishUnit& Shooter = M_Units[ShooterUnitIndex];
	++M_ExactlyCreditedHits;
	if (Shooter.OwningPlayer != AttackerSlot)
	{
		// The weapon state named a player other than the one its unit was spawned for.
		if (++M_ShooterOwnerMismatchHits <= FourPlayerSkirmishPrivate::MaxDetailLogsPerKind)
		{
			UE_LOG(LogRTSFourPlayerSkirmish, Warning, TEXT("RTS_FPS SHOOTER_OWNER %s (P%d) hit as P%d"),
			       *Shooter.Label, Shooter.OwningPlayer, AttackerSlot);
		}
	}
	Shooter.ExactDamageDealtToPlayer[VictimOwner] += Report.AppliedDamage;
	++Shooter.ExactHitsOnPlayer[VictimOwner];
	return Shooter.Label;
}

void UFourPlayerSkirmishSubsystem::AppendWeaponKindsToSummary(TArray<FString>& OutLines) const
{
	using namespace FourPlayerSkirmishPrivate;
	OutLines.Add(TEXT("\n## Weapon kinds\n"));
	OutLines.Add(FString::Printf(TEXT("Which players each kind of Blueprint weapon fired at (held target when it fired) "
		                             "and really hit (damage hook named the weapon; %d of %d hits). Free for all needs "
		                             "every kind's shots, over the four players' units, to reach %d different players. "
		                             "A turret keeps its first target until one of them dies, so one turret often "
		                             "engages one player.\n"), M_ExactlyCreditedHits, M_DamageReportCount,
	                             FreeForAllKindTargetPlayersRequired));
	OutLines.Add(TEXT("| Kind | Fired at (shooter->targets) | Hit (shooter->victims) | Players fired at | Most hostile "
		"players one unit fired at |"));
	OutLines.Add(TEXT("|---|---|---|---|---|"));
	for (const EFourPlayerSkirmishUnitKind Kind : AllUnitKinds)
	{
		OutLines.Add(FString::Printf(TEXT("| %s | %s | %s | %d | %d |"), GetUnitKindName(Kind),
		                             *GetEngagementText(M_Units, Kind, false), *GetEngagementText(M_Units, Kind, true),
		                             CountPlayersFiredAtByKind(M_Units, Kind),
		                             GetMostHostilePlayersFiredAtByKind(M_Units, Kind)));
	}
}

#else

bool UFourPlayerSkirmishSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UFourPlayerSkirmishSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UFourPlayerSkirmishSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFourPlayerSkirmishSubsystem::Tick(float DeltaTime)
{
}

TStatId UFourPlayerSkirmishSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFourPlayerSkirmishSubsystem, STATGROUP_Tickables);
}

#endif
