// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "CoreMinimal.h"

#ifndef RTS_WITH_SHIPPING_MAP_TESTS
#define RTS_WITH_SHIPPING_MAP_TESTS 0
#endif

#if RTS_WITH_SHIPPING_MAP_TESTS

#include "EngineUtils.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Stats/Stats.h"
#include "Tickable.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSNomadicBxpShippingTests, Log, All);

enum class ENomadicBxpShippingScenarioType : uint8
{
	CancelDuringAsyncLoad,
	CancelAsyncThenBuildAgain,
	CancelBeforePlacement,
	DestroyDuringConstruction,
	BuildThenCancelOwnerPack,
	PackOwnerWithActivePreview,
	CancelConstructionThenBuildAgain,
	BuildPackAndRebuild,
	BuildAndDestroyCleanly,
};

enum class ENomadicBxpShippingScenarioStage : uint8
{
	NotStarted,
	EnsureNomadicBuilding,
	RequestBxpSpawn,
	WaitForBxpSpawn,
	PlaceBxp,
	WaitBeforeWeirdAction,
	WaitForBxpBuilt,
	WaitForNomadicTruck,
	WaitForNomadicTruckAndNoLiveBxp,
	WaitForSlotClearBeforeRetry,
	RebuildNomadicBuilding,
	WaitForRebuildSpawn,
	WaitForRebuiltBxp,
	VerifyNoLiveBxp,
	Done,
};

struct FNomadicBxpShippingScenario
{
	TWeakObjectPtr<ANomadicVehicle> Nomadic;
	TWeakObjectPtr<ABuildingExpansion> Bxp;
	FBxpOptionData BxpOption;
	FString Name;
	ENomadicBxpShippingScenarioType Type = ENomadicBxpShippingScenarioType::BuildAndDestroyCleanly;
	ENomadicBxpShippingScenarioStage Stage = ENomadicBxpShippingScenarioStage::NotStarted;
	int32 ExpansionSlotIndex = INDEX_NONE;
	int32 RetryCount = 0;
	double StageStartedSeconds = 0.0;
	bool bActionStarted = false;
};

namespace RTS::UnitTests::NomadicBxpShipping
{
	constexpr float AutoStartDelaySeconds = 2.0f;
	constexpr float BxpSpawnTimeoutSeconds = 30.0f;
	constexpr float BxpBuildTimeoutSeconds = 180.0f;
	constexpr float NomadicConversionFastForwardDelaySeconds = 3.0f;
	constexpr float NomadicConversionTimeoutSeconds = 180.0f;
	constexpr float WeirdTimingDelaySeconds = 1.0f;
	constexpr int32 TestResourceAmount = 10000000;
	constexpr int32 DefaultDifficultyPercentage = 50;
	constexpr int32 MaxTestMapTravelAttempts = 3;
	const TCHAR* TestMapPackageName = TEXT("/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics");

	bool GetIsCurrentMapSupported(const UWorld& World)
	{
		return World.GetMapName().Contains(TEXT("UT_Nomadics"));
	}

	bool GetWasRequestedOnCommandLine()
	{
		FString RequestedMapTest;
		const bool bHasNamedRun = FParse::Value(FCommandLine::Get(), TEXT("RTSRunMapTest="), RequestedMapTest);
		return FParse::Param(FCommandLine::Get(), TEXT("RTSRunNomadicBxpTests")) ||
			(bHasNamedRun && (RequestedMapTest == TEXT("NomadicBxp") || RequestedMapTest == TEXT("All")));
	}

	double GetWorldSeconds(const UWorld* World)
	{
		return IsValid(World) ? World->GetTimeSeconds() : 0.0;
	}

	bool GetIsBxpBuilt(const ABuildingExpansion* Bxp)
	{
		return IsValid(Bxp) && Bxp->GetStatus() == EBuildingExpansionStatus::BXS_Built;
	}
}

namespace RTS::UnitTests::ShippingUnitAbilityStress
{
	constexpr float CommandPauseSeconds = 0.75f;
	constexpr uint8 PlayerIndex = 1;
	constexpr uint8 EnemyIndex = 2;

	enum class EUnitAbilityStressSource : uint8
	{
		PlayerTank,
		EnemyVehicle,
		PlayerSquad,
		EnemySquad,
		PlayerAircraft,
		EnemyAircraft,
	};

	struct FUnitAbilityStressScenario
	{
		TWeakObjectPtr<AActor> UnitActor;
		TWeakObjectPtr<AActor> OpposingTargetActor;
		TWeakObjectPtr<AActor> AlliedTargetActor;
		FUnitAbilityEntry AbilityEntry;
		FString Name;
		EUnitAbilityStressSource Source = EUnitAbilityStressSource::PlayerTank;
		bool bAttempted = false;
	};

	class FUnitAbilityStressHelper
	{
	public:
		bool Start(UWorld& InWorld, int32 MaxScenarios);
		bool Tick(float DeltaTime);

		int32 GetCompletedScenarioCount() const { return M_CompletedScenarioCount; }
		int32 GetFailedScenarioCount() const { return M_FailedScenarioCount; }
		int32 GetScenarioCount() const { return M_Scenarios.Num(); }
		int32 GetAttemptedScenarioCount() const { return M_AttemptedScenarioCount; }
		int32 GetSkippedScenarioCount() const { return M_SkippedScenarioCount; }

	private:
		void GatherScenarios(UGameUnitManager& GameUnitManager, int32 MaxScenarios);
		void GatherTankScenarios(
			const TArray<ATankMaster*>& Tanks,
			EUnitAbilityStressSource Source,
			bool bIsPlayerOwned);
		void GatherAircraftScenarios(
			const TArray<AAircraftMaster*>& Aircraft,
			EUnitAbilityStressSource Source,
			bool bIsPlayerOwned);
		void GatherAircraftScenariosFromWorld(UWorld& World);
		void GatherSquadScenarios(
			const TArray<ASquadUnit*>& SquadUnits,
			EUnitAbilityStressSource Source,
			bool bIsPlayerOwned);
		void AddCommandScenariosForActor(
			AActor& UnitActor,
			EUnitAbilityStressSource Source,
			bool bIsPlayerOwned,
			int32 MaxScenarios);
		void AddTargetActor(AActor& TargetActor, bool bIsPlayerOwned);
		void StartNextScenario();
		void FinishCurrentScenario();
		void SkipCurrentScenario(const FString& Reason);
		bool TryExecuteCurrentScenario(FString& OutSkipReason);

		ECommandQueueError ExecuteAbility(
			ICommands& Commands,
			const FUnitAbilityStressScenario& Scenario,
			bool& bOutWasDispatched,
			FString& OutSkipReason) const;

		AActor* GetFirstValidTarget(bool bUsePlayerTargets) const;
		FVector GetScenarioTargetLocation(const FUnitAbilityStressScenario& Scenario) const;
		FVector GetScenarioOffsetLocation(const FUnitAbilityStressScenario& Scenario) const;
		FRotator GetScenarioRotation(const FUnitAbilityStressScenario& Scenario) const;
		bool GetIsPlayerOwnedSource(EUnitAbilityStressSource Source) const;
		bool GetHasCommandScenariosForActor(const AActor& UnitActor) const;
		FString GetSourceString(EUnitAbilityStressSource Source) const;

		TWeakObjectPtr<UWorld> M_World;
		TArray<FUnitAbilityStressScenario> M_Scenarios;
		TArray<TWeakObjectPtr<AActor>> M_PlayerTargetActors;
		TArray<TWeakObjectPtr<AActor>> M_EnemyTargetActors;
		int32 M_CurrentScenarioIndex = INDEX_NONE;
		int32 M_CompletedScenarioCount = 0;
		int32 M_AttemptedScenarioCount = 0;
		int32 M_SkippedScenarioCount = 0;
		int32 M_FailedScenarioCount = 0;
		float M_CommandPauseSeconds = 0.0f;
		bool bM_IsRunning = false;
		bool bM_IsComplete = false;
	};

	bool GetShouldIncludeAbilityEntry(const FUnitAbilityEntry& AbilityEntry)
	{
		switch (AbilityEntry.AbilityId)
		{
		case EAbilityID::IdNoAbility:
		case EAbilityID::IdNoAbility_MoveCloserToTarget:
		case EAbilityID::IdNoAbility_MoveToEvasionLocation:
		case EAbilityID::IdIdle:
			return false;
		default:
			return true;
		}
	}

	bool FUnitAbilityStressHelper::Start(UWorld& InWorld, const int32 MaxScenarios)
	{
		M_World = &InWorld;
		M_CurrentScenarioIndex = INDEX_NONE;
		M_CompletedScenarioCount = 0;
		M_AttemptedScenarioCount = 0;
		M_SkippedScenarioCount = 0;
		M_FailedScenarioCount = 0;
		M_CommandPauseSeconds = 0.0f;
		M_Scenarios.Empty();
		M_PlayerTargetActors.Empty();
		M_EnemyTargetActors.Empty();
		bM_IsRunning = true;
		bM_IsComplete = false;

		UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(&InWorld);
		if (not IsValid(GameUnitManager))
		{
			M_FailedScenarioCount++;
			bM_IsComplete = true;
			bM_IsRunning = false;
			UE_LOG(LogRTSNomadicBxpShippingTests, Error,
			       TEXT("RTS_UNIT_ABILITY_STRESS_RESULT FAIL reason=GameUnitManagerInvalid"));
			return false;
		}

		GatherScenarios(*GameUnitManager, MaxScenarios);
		UE_LOG(LogRTSNomadicBxpShippingTests, Display,
		       TEXT("RTS_UNIT_ABILITY_STRESS_BEGIN gathered=%d max_scenarios=%d"),
		       M_Scenarios.Num(),
		       MaxScenarios);

		StartNextScenario();
		return true;
	}

	bool FUnitAbilityStressHelper::Tick(const float DeltaTime)
	{
		if (bM_IsComplete)
		{
			return true;
		}

		if (not bM_IsRunning)
		{
			return false;
		}

		if (M_CommandPauseSeconds > 0.0f)
		{
			M_CommandPauseSeconds -= DeltaTime;
			return false;
		}

		if (not M_Scenarios.IsValidIndex(M_CurrentScenarioIndex))
		{
			bM_IsComplete = true;
			bM_IsRunning = false;
			UE_LOG(LogRTSNomadicBxpShippingTests, Display,
			       TEXT("RTS_UNIT_ABILITY_STRESS_RESULT PASS completed=%d attempted=%d skipped=%d failed=%d total=%d"),
			       M_CompletedScenarioCount,
			       M_AttemptedScenarioCount,
			       M_SkippedScenarioCount,
			       M_FailedScenarioCount,
			       M_Scenarios.Num());
			return true;
		}

		FUnitAbilityStressScenario& Scenario = M_Scenarios[M_CurrentScenarioIndex];
		if (Scenario.bAttempted)
		{
			FinishCurrentScenario();
			return false;
		}

		FString SkipReason;
		if (TryExecuteCurrentScenario(SkipReason))
		{
			Scenario.bAttempted = true;
			M_CommandPauseSeconds = CommandPauseSeconds;
			return false;
		}

		SkipCurrentScenario(SkipReason);
		return false;
	}

	void FUnitAbilityStressHelper::GatherScenarios(UGameUnitManager& GameUnitManager, const int32 MaxScenarios)
	{
		GatherTankScenarios(GameUnitManager.GetPlayerTanks(PlayerIndex), EUnitAbilityStressSource::PlayerTank, true);
		GatherTankScenarios(GameUnitManager.GetPlayerTanks(EnemyIndex), EUnitAbilityStressSource::EnemyVehicle, false);
		GatherSquadScenarios(GameUnitManager.GetSquadUnitsOfPlayer(PlayerIndex), EUnitAbilityStressSource::PlayerSquad, true);
		GatherSquadScenarios(GameUnitManager.GetSquadUnitsOfPlayer(EnemyIndex), EUnitAbilityStressSource::EnemySquad, false);
		GatherAircraftScenarios(
			GameUnitManager.ShippingTest_GetAircraftOfPlayer(PlayerIndex),
			EUnitAbilityStressSource::PlayerAircraft,
			true);
		GatherAircraftScenarios(
			GameUnitManager.ShippingTest_GetAircraftOfPlayer(EnemyIndex),
			EUnitAbilityStressSource::EnemyAircraft,
			false);
		if (UWorld* World = M_World.Get())
		{
			GatherAircraftScenariosFromWorld(*World);
		}

		if (MaxScenarios > 0 && M_Scenarios.Num() > MaxScenarios)
		{
			M_Scenarios.SetNum(MaxScenarios);
		}
	}

	void FUnitAbilityStressHelper::GatherTankScenarios(
		const TArray<ATankMaster*>& Tanks,
		const EUnitAbilityStressSource Source,
		const bool bIsPlayerOwned)
	{
		for (ATankMaster* Tank : Tanks)
		{
			if (not IsValid(Tank))
			{
				continue;
			}

			AddTargetActor(*Tank, bIsPlayerOwned);
			AddCommandScenariosForActor(*Tank, Source, bIsPlayerOwned, 0);
		}
	}

	void FUnitAbilityStressHelper::GatherAircraftScenarios(
		const TArray<AAircraftMaster*>& Aircraft,
		const EUnitAbilityStressSource Source,
		const bool bIsPlayerOwned)
	{
		for (AAircraftMaster* AircraftUnit : Aircraft)
		{
			if (not IsValid(AircraftUnit))
			{
				continue;
			}

			AddTargetActor(*AircraftUnit, bIsPlayerOwned);
			AddCommandScenariosForActor(*AircraftUnit, Source, bIsPlayerOwned, 0);
		}
	}

	void FUnitAbilityStressHelper::GatherAircraftScenariosFromWorld(UWorld& World)
	{
		int32 PlayerAircraftFallbackCount = 0;
		int32 EnemyAircraftFallbackCount = 0;
		for (TActorIterator<AAircraftMaster> AircraftIterator(&World); AircraftIterator; ++AircraftIterator)
		{
			AAircraftMaster* AircraftUnit = *AircraftIterator;
			if (not IsValid(AircraftUnit) || GetHasCommandScenariosForActor(*AircraftUnit))
			{
				continue;
			}

			URTSComponent* RTSComponent = AircraftUnit->GetRTSComponent();
			if (not IsValid(RTSComponent))
			{
				continue;
			}

			if (RTSComponent->GetOwningPlayer() == PlayerIndex)
			{
				PlayerAircraftFallbackCount++;
				AddTargetActor(*AircraftUnit, true);
				AddCommandScenariosForActor(*AircraftUnit, EUnitAbilityStressSource::PlayerAircraft, true, 0);
				continue;
			}

			if (RTSComponent->GetOwningPlayer() == EnemyIndex)
			{
				EnemyAircraftFallbackCount++;
				AddTargetActor(*AircraftUnit, false);
				AddCommandScenariosForActor(*AircraftUnit, EUnitAbilityStressSource::EnemyAircraft, false, 0);
			}
		}

		if (PlayerAircraftFallbackCount > 0 || EnemyAircraftFallbackCount > 0)
		{
			UE_LOG(LogRTSNomadicBxpShippingTests, Display,
			       TEXT("RTS_UNIT_ABILITY_STRESS_AIRCRAFT_WORLD_FALLBACK player=%d enemy=%d"),
			       PlayerAircraftFallbackCount,
			       EnemyAircraftFallbackCount);
		}
	}

	void FUnitAbilityStressHelper::GatherSquadScenarios(
		const TArray<ASquadUnit*>& SquadUnits,
		const EUnitAbilityStressSource Source,
		const bool bIsPlayerOwned)
	{
		TSet<ASquadController*> SquadControllers;
		for (ASquadUnit* SquadUnit : SquadUnits)
		{
			if (not IsValid(SquadUnit))
			{
				continue;
			}

			AddTargetActor(*SquadUnit, bIsPlayerOwned);
			ASquadController* SquadController = SquadUnit->GetSquadControllerChecked();
			if (not IsValid(SquadController))
			{
				continue;
			}

			SquadControllers.Add(SquadController);
		}

		for (ASquadController* SquadController : SquadControllers)
		{
			if (IsValid(SquadController))
			{
				AddCommandScenariosForActor(*SquadController, Source, bIsPlayerOwned, 0);
			}
		}
	}

	void FUnitAbilityStressHelper::AddCommandScenariosForActor(
		AActor& UnitActor,
		const EUnitAbilityStressSource Source,
		const bool bIsPlayerOwned,
		const int32 MaxScenarios)
	{
		ICommands* Commands = Cast<ICommands>(&UnitActor);
		if (Commands == nullptr)
		{
			return;
		}

		TArray<FUnitAbilityEntry> AbilityEntries = Commands->GetUnitAbilityEntries();
		for (const FUnitAbilityEntry& AbilityEntry : AbilityEntries)
		{
			if (MaxScenarios > 0 && M_Scenarios.Num() >= MaxScenarios)
			{
				return;
			}

			if (not GetShouldIncludeAbilityEntry(AbilityEntry))
			{
				continue;
			}

			FUnitAbilityStressScenario Scenario;
			Scenario.UnitActor = &UnitActor;
			Scenario.OpposingTargetActor = GetFirstValidTarget(not bIsPlayerOwned);
			Scenario.AlliedTargetActor = GetFirstValidTarget(bIsPlayerOwned);
			Scenario.AbilityEntry = AbilityEntry;
			Scenario.Source = Source;
			Scenario.Name = FString::Printf(
				TEXT("%s.%s.%s.%d"),
				*GetSourceString(Source),
				*UnitActor.GetName(),
				*Global_GetAbilityIDAsString(AbilityEntry.AbilityId),
				AbilityEntry.CustomType);
			M_Scenarios.Add(Scenario);
		}
	}

	void FUnitAbilityStressHelper::AddTargetActor(AActor& TargetActor, const bool bIsPlayerOwned)
	{
		TArray<TWeakObjectPtr<AActor>>& Targets = bIsPlayerOwned ? M_PlayerTargetActors : M_EnemyTargetActors;
		Targets.AddUnique(&TargetActor);
	}

	void FUnitAbilityStressHelper::StartNextScenario()
	{
		M_CurrentScenarioIndex++;
		if (not M_Scenarios.IsValidIndex(M_CurrentScenarioIndex))
		{
			return;
		}

		UE_LOG(LogRTSNomadicBxpShippingTests, Display,
		       TEXT("RTS_UNIT_ABILITY_STRESS_BEGIN_SCENARIO index=%d name=%s"),
		       M_CurrentScenarioIndex,
		       *M_Scenarios[M_CurrentScenarioIndex].Name);
	}

	void FUnitAbilityStressHelper::FinishCurrentScenario()
	{
		FUnitAbilityStressScenario& Scenario = M_Scenarios[M_CurrentScenarioIndex];
		if (AActor* UnitActor = Scenario.UnitActor.Get())
		{
			if (ICommands* Commands = Cast<ICommands>(UnitActor))
			{
				Commands->SetUnitToIdle();
			}
		}

		M_CompletedScenarioCount++;
		UE_LOG(LogRTSNomadicBxpShippingTests, Display,
		       TEXT("RTS_UNIT_ABILITY_STRESS_PASS index=%d name=%s"),
		       M_CurrentScenarioIndex,
		       *Scenario.Name);
		StartNextScenario();
	}

	void FUnitAbilityStressHelper::SkipCurrentScenario(const FString& Reason)
	{
		const FUnitAbilityStressScenario& Scenario = M_Scenarios[M_CurrentScenarioIndex];
		M_CompletedScenarioCount++;
		M_SkippedScenarioCount++;
		UE_LOG(LogRTSNomadicBxpShippingTests, Warning,
		       TEXT("RTS_UNIT_ABILITY_STRESS_SKIP index=%d name=%s reason=%s"),
		       M_CurrentScenarioIndex,
		       *Scenario.Name,
		       *Reason);
		StartNextScenario();
	}

	bool FUnitAbilityStressHelper::TryExecuteCurrentScenario(FString& OutSkipReason)
	{
		FUnitAbilityStressScenario& Scenario = M_Scenarios[M_CurrentScenarioIndex];
		AActor* UnitActor = Scenario.UnitActor.Get();
		if (not IsValid(UnitActor))
		{
			OutSkipReason = TEXT("Unit actor destroyed before ability stress attempt.");
			return false;
		}

		ICommands* Commands = Cast<ICommands>(UnitActor);
		if (Commands == nullptr)
		{
			OutSkipReason = TEXT("Unit no longer implements ICommands.");
			return false;
		}

		const bool bIsPlayerOwnedSource = GetIsPlayerOwnedSource(Scenario.Source);
		Scenario.OpposingTargetActor = GetFirstValidTarget(not bIsPlayerOwnedSource);
		Scenario.AlliedTargetActor = GetFirstValidTarget(bIsPlayerOwnedSource);

		bool bWasDispatched = false;
		const ECommandQueueError CommandError = ExecuteAbility(*Commands, Scenario, bWasDispatched, OutSkipReason);
		if (not bWasDispatched)
		{
			return false;
		}

		M_AttemptedScenarioCount++;
		UE_LOG(LogRTSNomadicBxpShippingTests, Display,
		       TEXT("RTS_UNIT_ABILITY_STRESS_ATTEMPT index=%d name=%s result=%d"),
		       M_CurrentScenarioIndex,
		       *Scenario.Name,
		       static_cast<int32>(CommandError));
		return true;
	}

	ECommandQueueError FUnitAbilityStressHelper::ExecuteAbility(
		ICommands& Commands,
		const FUnitAbilityStressScenario& Scenario,
		bool& bOutWasDispatched,
		FString& OutSkipReason) const
	{
		bOutWasDispatched = true;
		const FVector TargetLocation = GetScenarioTargetLocation(Scenario);
		const FVector OffsetLocation = GetScenarioOffsetLocation(Scenario);
		const FRotator TargetRotation = GetScenarioRotation(Scenario);
		AActor* const OpposingTarget = Scenario.OpposingTargetActor.Get();
		AActor* const AlliedTarget = Scenario.AlliedTargetActor.Get();

		switch (Scenario.AbilityEntry.AbilityId)
		{
		case EAbilityID::IdAttack: return Commands.AttackActor(OpposingTarget, true);
		case EAbilityID::IdAttackGround: return Commands.AttackGround(TargetLocation, true);
		case EAbilityID::IdMove: return Commands.MoveToLocation(OffsetLocation, true, TargetRotation);
		case EAbilityID::IdPatrol: return Commands.PatrolToLocation(OffsetLocation, true);
		case EAbilityID::IdStop: return Commands.StopCommand();
		case EAbilityID::IdSwitchWeapon: return Commands.SwitchWeapons(true);
		case EAbilityID::IdReverseMove: return Commands.ReverseUnitToLocation(OffsetLocation, true);
		case EAbilityID::IdRotateTowards: return Commands.RotateTowards(TargetRotation, true);
		case EAbilityID::IdCreateBuilding: return Commands.CreateBuildingAtLocation(OffsetLocation, TargetRotation, true);
		case EAbilityID::IdConvertToVehicle: return Commands.ConvertToVehicle(true);
		case EAbilityID::IdHarvestResource: return Commands.HarvestResource(OpposingTarget, true);
		case EAbilityID::IdReturnCargo: return Commands.ReturnCargo(true);
		case EAbilityID::IdPickupItem: return Commands.PickupItem(nullptr, true);
		case EAbilityID::IdScavenge: return Commands.ScavengeObject(OpposingTarget, true);
		case EAbilityID::IdDigIn: return Commands.DigIn(true);
		case EAbilityID::IdBreakCover: return Commands.BreakCover(true);
		case EAbilityID::IdFireRockets: return Commands.FireRockets(true);
		case EAbilityID::IdCancelRocketFire: return Commands.CancelFireRockets(true);
		case EAbilityID::IdThrowGrenade:
			return Commands.ThrowGrenade(TargetLocation, true, static_cast<EGrenadeAbilityType>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdCancelThrowGrenade:
			return Commands.CancelThrowingGrenade(true, static_cast<EGrenadeAbilityType>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdRepair: return Commands.RepairActor(AlliedTarget, true);
		case EAbilityID::IdReturnToBase: return Commands.ReturnToBase(true);
		case EAbilityID::IdRetreat: return Commands.RetreatToLocation(OffsetLocation, true);
		case EAbilityID::IdEnterCargo: return Commands.EnterCargo(AlliedTarget, true);
		case EAbilityID::IdExitCargo: return Commands.ExitCargo(true);
		case EAbilityID::IdEnableResourceConversion:
			Commands.NoQueue_SetResourceConversionEnabled(true);
			return ECommandQueueError::NoError;
		case EAbilityID::IdDisableResourceConversion:
			Commands.NoQueue_SetResourceConversionEnabled(false);
			return ECommandQueueError::NoError;
		case EAbilityID::IdCapture: return Commands.CaptureActor(OpposingTarget, true);
		case EAbilityID::IdReinforceSquad: return Commands.Reinforce(true);
		case EAbilityID::IdApplyBehaviour:
			return Commands.ActivateBehaviourAbility(static_cast<EBehaviourAbilityType>(Scenario.AbilityEntry.CustomType), true);
		case EAbilityID::IdActivateMode:
			return Commands.ActivateModeAbility(static_cast<EModeAbilityType>(Scenario.AbilityEntry.CustomType), true);
		case EAbilityID::IdDisableMode:
			return Commands.DisableModeAbility(static_cast<EModeAbilityType>(Scenario.AbilityEntry.CustomType), true);
		case EAbilityID::IdFieldConstruction:
			return Commands.FieldConstruction(
				static_cast<EFieldConstructionType>(Scenario.AbilityEntry.CustomType),
				true,
				OffsetLocation,
				TargetRotation,
				nullptr);
		case EAbilityID::IdAttachedWeapon:
			return Commands.FireAttachedWeaponAbility(
				TargetLocation,
				true,
				static_cast<EAttachWeaponAbilitySubType>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdAimAbility:
			return Commands.AimAbility(TargetLocation, true, static_cast<EAimAbilityType>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdCancelAimAbility:
			return Commands.CancelAimAbility(true, static_cast<EAimAbilityType>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdSwapTurret:
			return Commands.SwapTurret(true, static_cast<ETurretSwapAbility>(Scenario.AbilityEntry.CustomType));
		case EAbilityID::IdManAbandonedTeamWeapon:
			return Commands.ManAbandonedTeamWeapon(OpposingTarget, true);
		case EAbilityID::IdTowActor:
			return Commands.TowActor(
				OpposingTarget,
				static_cast<ETowedActorTarget>(Scenario.AbilityEntry.CustomType),
				true);
		case EAbilityID::IdDetachTow: return Commands.DetachTow(true);
		case EAbilityID::IdRegisterUnitAsBlackboardIdle: return Commands.RegisterAsBlackboardIdle(true);
		case EAbilityID::IdResearchTechnology:
			return Commands.ResearchTechnology(
				static_cast<ETechnology>(Scenario.AbilityEntry.CustomType),
				TArray<ETechnology>(),
				true);
		default:
			bOutWasDispatched = false;
			OutSkipReason = TEXT("No direct ICommands stress dispatch for ability.");
			return ECommandQueueError::AbilityNotAllowed;
		}
	}

	AActor* FUnitAbilityStressHelper::GetFirstValidTarget(const bool bUsePlayerTargets) const
	{
		const TArray<TWeakObjectPtr<AActor>>& Targets = bUsePlayerTargets ? M_PlayerTargetActors : M_EnemyTargetActors;
		for (const TWeakObjectPtr<AActor>& Target : Targets)
		{
			if (AActor* TargetActor = Target.Get())
			{
				return TargetActor;
			}
		}

		return nullptr;
	}

	FVector FUnitAbilityStressHelper::GetScenarioTargetLocation(const FUnitAbilityStressScenario& Scenario) const
	{
		if (const AActor* TargetActor = Scenario.OpposingTargetActor.Get())
		{
			return TargetActor->GetActorLocation();
		}

		return GetScenarioOffsetLocation(Scenario);
	}

	FVector FUnitAbilityStressHelper::GetScenarioOffsetLocation(const FUnitAbilityStressScenario& Scenario) const
	{
		const AActor* UnitActor = Scenario.UnitActor.Get();
		const FVector Origin = IsValid(UnitActor) ? UnitActor->GetActorLocation() : FVector::ZeroVector;
		const int32 AngleSeed = (M_CurrentScenarioIndex * 37) % 360;
		const float AngleRadians = FMath::DegreesToRadians(static_cast<float>(AngleSeed));
		return Origin + FVector(FMath::Cos(AngleRadians) * 700.0f, FMath::Sin(AngleRadians) * 700.0f, 0.0f);
	}

	FRotator FUnitAbilityStressHelper::GetScenarioRotation(const FUnitAbilityStressScenario& Scenario) const
	{
		const FVector FromLocation = Scenario.UnitActor.IsValid()
			                             ? Scenario.UnitActor->GetActorLocation()
			                             : FVector::ZeroVector;
		const FVector ToLocation = GetScenarioTargetLocation(Scenario);
		return (ToLocation - FromLocation).Rotation();
	}

	bool FUnitAbilityStressHelper::GetIsPlayerOwnedSource(const EUnitAbilityStressSource Source) const
	{
		return Source == EUnitAbilityStressSource::PlayerTank ||
			Source == EUnitAbilityStressSource::PlayerSquad ||
			Source == EUnitAbilityStressSource::PlayerAircraft;
	}

	bool FUnitAbilityStressHelper::GetHasCommandScenariosForActor(const AActor& UnitActor) const
	{
		for (const FUnitAbilityStressScenario& Scenario : M_Scenarios)
		{
			if (Scenario.UnitActor.Get() == &UnitActor)
			{
				return true;
			}
		}

		return false;
	}

	FString FUnitAbilityStressHelper::GetSourceString(const EUnitAbilityStressSource Source) const
	{
		switch (Source)
		{
		case EUnitAbilityStressSource::PlayerTank: return TEXT("PlayerTank");
		case EUnitAbilityStressSource::EnemyVehicle: return TEXT("EnemyVehicle");
		case EUnitAbilityStressSource::PlayerSquad: return TEXT("PlayerSquad");
		case EUnitAbilityStressSource::EnemySquad: return TEXT("EnemySquad");
		case EUnitAbilityStressSource::PlayerAircraft: return TEXT("PlayerAircraft");
		case EUnitAbilityStressSource::EnemyAircraft: return TEXT("EnemyAircraft");
		}

		return TEXT("UnknownSource");
	}
}

enum class EShippingMapTestPhase : uint8
{
	NotStarted,
	NomadicBxp,
	UnitAbilityStress,
	Complete,
};

class FRTSNomadicBxpShippingTestRunner final : public FTickableGameObject
{
public:
	explicit FRTSNomadicBxpShippingTestRunner(UWorld& InWorld)
		: M_World(&InWorld)
	{
	}

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override;

	void StartRun(const FString& Reason, const TArray<FString>& Args);
	bool GetIsForWorld(const UWorld* World) const;
	bool GetIsRunning() const { return bM_IsRunning; }

private:
	UWorld* GetWorld() const;
	void FinishRun();
	void GatherScenarios();
	void StartUnitAbilityStress();
	void ResumeStartGameFlowIfPaused() const;
	void GivePlayerTestResources() const;
	void StartNextScenario();
	void AdvanceScenario(FNomadicBxpShippingScenario& Scenario);
	void SetScenarioStage(
		FNomadicBxpShippingScenario& Scenario,
		ENomadicBxpShippingScenarioStage NewStage);

	void AdvanceEnsureNomadicBuilding(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForBxpSpawn(FNomadicBxpShippingScenario& Scenario);
	void AdvancePlaceBxp(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitBeforeWeirdAction(FNomadicBxpShippingScenario& Scenario);
	void AdvanceDestroyDuringConstructionAction(
		FNomadicBxpShippingScenario& Scenario,
		ACPPController& Controller,
		ANomadicVehicle& Nomadic);
	void AdvanceBuildThenCancelOwnerPackAction(
		FNomadicBxpShippingScenario& Scenario,
		ACPPController& Controller,
		ANomadicVehicle& Nomadic);
	void AdvancePackOwnerWithActivePreviewAction(
		FNomadicBxpShippingScenario& Scenario,
		ACPPController& Controller,
		ANomadicVehicle& Nomadic);
	void AdvanceCancelConstructionThenBuildAgainAction(
		FNomadicBxpShippingScenario& Scenario,
		ACPPController& Controller,
		ANomadicVehicle& Nomadic);
	void AdvanceWaitForBxpBuilt(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForNomadicTruck(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForNomadicTruckAndNoLiveBxp(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForSlotClearBeforeRetry(FNomadicBxpShippingScenario& Scenario);
	void AdvanceRebuildNomadicBuilding(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForRebuildSpawn(FNomadicBxpShippingScenario& Scenario);
	void AdvanceWaitForRebuiltBxp(FNomadicBxpShippingScenario& Scenario);
	void AdvanceVerifyNoLiveBxp(FNomadicBxpShippingScenario& Scenario);

	bool GetIsStageTimedOut(const FNomadicBxpShippingScenario& Scenario, float TimeoutSeconds) const;
	bool TryQueueNomadicBuildCommand(ANomadicVehicle& Nomadic) const;
	bool TryFastForwardNomadicBuildingConversion(FNomadicBxpShippingScenario& Scenario) const;
	bool TryStartBxpSpawn(FNomadicBxpShippingScenario& Scenario, FString& OutFailureReason) const;
	bool TryCancelActiveBxpPreview(
		const FNomadicBxpShippingScenario& Scenario,
		bool bIsPackedPreview) const;
	bool TryGetOwnerInterface(
		ANomadicVehicle& Nomadic,
		TScriptInterface<IBuildingExpansionOwner>& OutOwner) const;
	bool TryGetPlacementClickLocation(
		const FNomadicBxpShippingScenario& Scenario,
		FVector& OutClickedLocation) const;
	bool GetHasNoLiveBxpAtScenarioSlot(const FNomadicBxpShippingScenario& Scenario) const;
	bool GetScenarioNamePassesFilters(const FString& ScenarioName) const;

	ACPPController* GetPlayerController() const;
	ABuildingExpansion* GetScenarioSlotBxp(const FNomadicBxpShippingScenario& Scenario) const;
	FBuildingExpansionItem* GetScenarioSlotItem(const FNomadicBxpShippingScenario& Scenario) const;
	FString GetScenarioTypeString(ENomadicBxpShippingScenarioType Type) const;
	FString GetScenarioStageString(ENomadicBxpShippingScenarioStage Stage) const;

	void AddScenario(
		ANomadicVehicle& Nomadic,
		const FBxpOptionData& BxpOption,
		ENomadicBxpShippingScenarioType Type);
	void LogScenarioFailure(FNomadicBxpShippingScenario& Scenario, const FString& Reason);
	void LogScenarioSuccess(FNomadicBxpShippingScenario& Scenario);
	void CleanupScenario(FNomadicBxpShippingScenario& Scenario) const;

	TWeakObjectPtr<UWorld> M_World;
	TArray<FNomadicBxpShippingScenario> M_Scenarios;
	RTS::UnitTests::ShippingUnitAbilityStress::FUnitAbilityStressHelper M_UnitAbilityStressHelper;
	FString M_RunReason;
	FString M_NameFilter;
	TArray<FString> M_ExcludeNameFilters;
	int32 M_CurrentScenarioIndex = INDEX_NONE;
	int32 M_PassedScenarioCount = 0;
	int32 M_FailedScenarioCount = 0;
	int32 M_MaxScenarios = 0;
	int32 M_UnitAbilityMaxScenarios = 0;
	float M_StartDelaySeconds = 0.0f;
	EShippingMapTestPhase M_Phase = EShippingMapTestPhase::NotStarted;
	bool bM_IsRunning = false;
	bool bM_ExitOnComplete = false;
};

namespace RTS::UnitTests::NomadicBxpShipping
{
	TArray<TUniquePtr<FRTSNomadicBxpShippingTestRunner>> GActiveRunners;
	int32 GTestMapTravelAttempts = 0;

	FRTSNomadicBxpShippingTestRunner* FindRunnerForWorld(const UWorld* World)
	{
		for (const TUniquePtr<FRTSNomadicBxpShippingTestRunner>& Runner : GActiveRunners)
		{
			if (Runner.IsValid() && Runner->GetIsForWorld(World))
			{
				return Runner.Get();
			}
		}

		return nullptr;
	}

	void StartRunnerForWorld(UWorld& World, const FString& Reason, const TArray<FString>& Args)
	{
		if (not World.IsGameWorld())
		{
			return;
		}

		FRTSNomadicBxpShippingTestRunner* ExistingRunner = FindRunnerForWorld(&World);
		if (ExistingRunner != nullptr)
		{
			ExistingRunner->StartRun(Reason, Args);
			return;
		}

		TUniquePtr<FRTSNomadicBxpShippingTestRunner> NewRunner =
			MakeUnique<FRTSNomadicBxpShippingTestRunner>(World);
		NewRunner->StartRun(Reason, Args);
		GActiveRunners.Add(MoveTemp(NewRunner));
	}

	void RemoveRunnerForWorld(const UWorld* World)
	{
		for (int32 Index = GActiveRunners.Num() - 1; Index >= 0; --Index)
		{
			if (GActiveRunners[Index].IsValid() && GActiveRunners[Index]->GetIsForWorld(World))
			{
				GActiveRunners.RemoveAtSwap(Index);
			}
		}
	}

	void ApplyDefaultRuntimeStateForTestMap(UWorld& World)
	{
		URTSGameInstance* const GameInstance = World.GetGameInstance<URTSGameInstance>();
		if (not IsValid(GameInstance))
		{
			UE_LOG(LogRTSNomadicBxpShippingTests, Error,
			       TEXT("Cannot prepare Nomadic/BXP test map: RTS game instance is invalid."));
			return;
		}

		FRTSGameDifficulty GameDifficulty;
		GameDifficulty.DifficultyLevel = ERTSGameDifficulty::Normal;
		GameDifficulty.DifficultyPercentage = DefaultDifficultyPercentage;
		GameDifficulty.bIsInitialized = true;

		GameInstance->SetSelectedGameDifficulty(GameDifficulty);
		GameInstance->SetPlayerFaction(ERTSFaction::GerStrikeDivision);
		GameInstance->SetPlayerCommander(ERTSCommander::BalancedCommander);
	}

	void QueueTravelToTestMap(UWorld& World)
	{
		if (GTestMapTravelAttempts >= MaxTestMapTravelAttempts)
		{
			UE_LOG(LogRTSNomadicBxpShippingTests, Error,
			       TEXT("RTS_NOMADIC_BXP_TEST_RESULT FAIL could not travel to %s after %d attempts."),
			       TestMapPackageName,
			       GTestMapTravelAttempts);

			if (FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete")))
			{
				FPlatformMisc::RequestExit(false);
			}
			return;
		}

		GTestMapTravelAttempts++;
		ApplyDefaultRuntimeStateForTestMap(World);

		TWeakObjectPtr<UWorld> WeakWorld = &World;
		World.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld]()
		{
			UWorld* const TravelWorld = WeakWorld.Get();
			if (not IsValid(TravelWorld))
			{
				return;
			}

			UE_LOG(LogRTSNomadicBxpShippingTests, Display,
			       TEXT("RTS_NOMADIC_BXP_TEST_TRAVEL map=%s attempt=%d"),
			       TestMapPackageName,
			       GTestMapTravelAttempts);
			UGameplayStatics::OpenLevel(TravelWorld, FName(TestMapPackageName));
		}));
	}

	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues)
	{
		if (not IsValid(World) || not World->IsGameWorld() || not GetWasRequestedOnCommandLine())
		{
			return;
		}

		if (not GetIsCurrentMapSupported(*World))
		{
			QueueTravelToTestMap(*World);
			return;
		}

		GTestMapTravelAttempts = 0;
		ApplyDefaultRuntimeStateForTestMap(*World);
		StartRunnerForWorld(*World, TEXT("command-line"), TArray<FString>());
	}

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		RemoveRunnerForWorld(World);
	}

	void RunNomadicBxpTestsCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (not IsValid(World))
		{
			UE_LOG(LogRTSNomadicBxpShippingTests, Error, TEXT("RTS.UnitTests.NomadicBxp.Run failed: world is invalid."));
			return;
		}

		StartRunnerForWorld(*World, TEXT("console-command"), Args);
	}

	FAutoConsoleCommandWithWorldAndArgs GRunNomadicBxpTestsCommand(
		TEXT("RTS.UnitTests.NomadicBxp.Run"),
		TEXT("Runs the internal Nomadic/BXP packaged-build stress scenarios on UT_Nomadics."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunNomadicBxpTestsCommand));

	struct FNomadicBxpShippingTestRegistration
	{
		FDelegateHandle PostWorldInitializationHandle;
		FDelegateHandle WorldCleanupHandle;

		FNomadicBxpShippingTestRegistration()
		{
			PostWorldInitializationHandle =
				FWorldDelegates::OnPostWorldInitialization.AddStatic(&HandlePostWorldInitialization);
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
		}

		~FNomadicBxpShippingTestRegistration()
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}
	};

	FNomadicBxpShippingTestRegistration GRegistration;
}

void FRTSNomadicBxpShippingTestRunner::Tick(const float DeltaTime)
{
	if (not bM_IsRunning)
	{
		return;
	}

	if (M_StartDelaySeconds > 0.0f)
	{
		M_StartDelaySeconds -= DeltaTime;
		return;
	}

	if (M_Phase == EShippingMapTestPhase::NotStarted)
	{
		GatherScenarios();
		M_Phase = EShippingMapTestPhase::NomadicBxp;
		StartNextScenario();
		return;
	}

	if (M_Phase == EShippingMapTestPhase::UnitAbilityStress)
	{
		if (M_UnitAbilityStressHelper.Tick(DeltaTime))
		{
			FinishRun();
		}
		return;
	}

	if (not M_Scenarios.IsValidIndex(M_CurrentScenarioIndex))
	{
		StartUnitAbilityStress();
		return;
	}

	AdvanceScenario(M_Scenarios[M_CurrentScenarioIndex]);
}

TStatId FRTSNomadicBxpShippingTestRunner::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FRTSNomadicBxpShippingTestRunner, STATGROUP_Tickables);
}

bool FRTSNomadicBxpShippingTestRunner::IsTickable() const
{
	return bM_IsRunning && M_World.IsValid();
}

bool FRTSNomadicBxpShippingTestRunner::IsTickableWhenPaused() const
{
	return true;
}

void FRTSNomadicBxpShippingTestRunner::StartRun(const FString& Reason, const TArray<FString>& Args)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not RTS::UnitTests::NomadicBxpShipping::GetIsCurrentMapSupported(*World))
	{
		UE_LOG(LogRTSNomadicBxpShippingTests, Error, TEXT("Nomadic/BXP tests can only run on UT_Nomadics."));
		return;
	}

	if (bM_IsRunning)
	{
		UE_LOG(LogRTSNomadicBxpShippingTests, Warning, TEXT("Nomadic/BXP tests already running."));
		return;
	}

	for (const FString& Arg : Args)
	{
		int32 ParsedMaxScenarios = 0;
		if (FDefaultValueHelper::ParseInt(Arg, ParsedMaxScenarios))
		{
			M_MaxScenarios = ParsedMaxScenarios;
		}
	}

	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSNomadicBxpMaxScenarios="), M_MaxScenarios);
	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSUnitAbilityMaxScenarios="), M_UnitAbilityMaxScenarios);
	M_NameFilter.Empty();
	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSNomadicBxpNameFilter="), M_NameFilter);
	M_ExcludeNameFilters.Empty();
	FString ExcludeNameFilters;
	if (FParse::Value(FCommandLine::Get(), TEXT("RTSNomadicBxpExcludeNameFilters="), ExcludeNameFilters))
	{
		ExcludeNameFilters.ParseIntoArray(M_ExcludeNameFilters, TEXT(";"), true);
		for (FString& ExcludeNameFilter : M_ExcludeNameFilters)
		{
			ExcludeNameFilter.TrimStartAndEndInline();
		}

		M_ExcludeNameFilters.RemoveAll([](const FString& ExcludeNameFilter)
		{
			return ExcludeNameFilter.IsEmpty();
		});
	}

	bM_ExitOnComplete = FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete"));
	M_RunReason = Reason;
	M_CurrentScenarioIndex = INDEX_NONE;
	M_PassedScenarioCount = 0;
	M_FailedScenarioCount = 0;
	M_StartDelaySeconds = RTS::UnitTests::NomadicBxpShipping::AutoStartDelaySeconds;
	M_Phase = EShippingMapTestPhase::NotStarted;
	M_Scenarios.Empty();
	bM_IsRunning = true;

	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_TEST_BEGIN reason=%s max_scenarios=%d unit_ability_max_scenarios=%d name_filter=%s exclude_filters=%s"),
	       *M_RunReason,
	       M_MaxScenarios,
	       M_UnitAbilityMaxScenarios,
	       *M_NameFilter,
	       *ExcludeNameFilters);
}

bool FRTSNomadicBxpShippingTestRunner::GetIsForWorld(const UWorld* World) const
{
	return M_World.Get() == World;
}

UWorld* FRTSNomadicBxpShippingTestRunner::GetWorld() const
{
	return M_World.Get();
}

void FRTSNomadicBxpShippingTestRunner::FinishRun()
{
	M_Phase = EShippingMapTestPhase::Complete;
	const int32 UnitCompletedCount = M_UnitAbilityStressHelper.GetCompletedScenarioCount();
	const int32 UnitFailedCount = M_UnitAbilityStressHelper.GetFailedScenarioCount();
	const int32 UnitTotalCount = M_UnitAbilityStressHelper.GetScenarioCount();
	const int32 TotalPassedCount = M_PassedScenarioCount + UnitCompletedCount;
	const int32 TotalFailedCount = M_FailedScenarioCount + UnitFailedCount;
	const bool bPassed = TotalFailedCount == 0 && TotalPassedCount > 0;
	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_TEST_RESULT %s passed=%d failed=%d total=%d bxp_passed=%d bxp_failed=%d unit_completed=%d unit_attempted=%d unit_skipped=%d unit_failed=%d unit_total=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"),
	       TotalPassedCount,
	       TotalFailedCount,
	       TotalPassedCount + TotalFailedCount,
	       M_PassedScenarioCount,
	       M_FailedScenarioCount,
	       UnitCompletedCount,
	       M_UnitAbilityStressHelper.GetAttemptedScenarioCount(),
	       M_UnitAbilityStressHelper.GetSkippedScenarioCount(),
	       UnitFailedCount,
	       UnitTotalCount);

	bM_IsRunning = false;

	if (bM_ExitOnComplete)
	{
		FPlatformMisc::RequestExit(false);
	}
}

void FRTSNomadicBxpShippingTestRunner::StartUnitAbilityStress()
{
	if (M_Phase == EShippingMapTestPhase::UnitAbilityStress || M_Phase == EShippingMapTestPhase::Complete)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_Phase = EShippingMapTestPhase::Complete;
		M_FailedScenarioCount++;
		FinishRun();
		return;
	}

	M_Phase = EShippingMapTestPhase::UnitAbilityStress;
	if (not M_UnitAbilityStressHelper.Start(*World, M_UnitAbilityMaxScenarios))
	{
		FinishRun();
	}
}

void FRTSNomadicBxpShippingTestRunner::GatherScenarios()
{
	ResumeStartGameFlowIfPaused();
	GivePlayerTestResources();

	for (TActorIterator<ANomadicVehicle> NomadicIterator(GetWorld()); NomadicIterator; ++NomadicIterator)
	{
		ANomadicVehicle* Nomadic = *NomadicIterator;
		if (not IsValid(Nomadic))
		{
			continue;
		}

		TScriptInterface<IBuildingExpansionOwner> Owner;
		if (not TryGetOwnerInterface(*Nomadic, Owner) || Owner->GetBuildingExpansions().IsEmpty())
		{
			continue;
		}

		const TArray<FBxpOptionData> Options = Owner->GetUnlockedBuildingExpansionTypes();
		for (const FBxpOptionData& Option : Options)
		{
			if (Option.ExpansionType == EBuildingExpansionType::BXT_Invalid)
			{
				continue;
			}

			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::CancelDuringAsyncLoad);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::CancelAsyncThenBuildAgain);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::CancelBeforePlacement);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::DestroyDuringConstruction);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::BuildThenCancelOwnerPack);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::PackOwnerWithActivePreview);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::CancelConstructionThenBuildAgain);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::BuildPackAndRebuild);
			AddScenario(*Nomadic, Option, ENomadicBxpShippingScenarioType::BuildAndDestroyCleanly);
		}
	}

	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_TEST_SCENARIOS gathered=%d"),
	       M_Scenarios.Num());
}

void FRTSNomadicBxpShippingTestRunner::ResumeStartGameFlowIfPaused() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World) || not UGameplayStatics::IsGamePaused(World))
	{
		return;
	}

	ACPPController* const Controller = GetPlayerController();
	if (IsValid(Controller))
	{
		Controller->ShippingTest_ResumeStartGameFlow();
	}

	if (UGameplayStatics::IsGamePaused(World))
	{
		UE_LOG(LogRTSNomadicBxpShippingTests, Warning,
		       TEXT("Start-game flow did not unpause the test map; forcing unpause for Shipping map test."));
		UGameplayStatics::SetGamePaused(World, false);
	}
}

void FRTSNomadicBxpShippingTestRunner::GivePlayerTestResources() const
{
	const ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not IsValid(Controller->GetPlayerResourceManager()))
	{
		UE_LOG(LogRTSNomadicBxpShippingTests, Error, TEXT("Cannot give test resources: player resource manager invalid."));
		return;
	}

	UPlayerResourceManager* ResourceManager = Controller->GetPlayerResourceManager();
	const TArray<ERTSResourceType> Resources = {
		ERTSResourceType::Resource_Radixite,
		ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts,
		ERTSResourceType::Resource_Fuel,
		ERTSResourceType::Resource_Ammo,
		ERTSResourceType::Resource_Intel,
		ERTSResourceType::Blueprint_Weapon,
		ERTSResourceType::Blueprint_Vehicle,
		ERTSResourceType::Blueprint_Energy,
		ERTSResourceType::Blueprint_Construction,
	};

	for (const ERTSResourceType Resource : Resources)
	{
		ResourceManager->CheatAddStorage(Resource, RTS::UnitTests::NomadicBxpShipping::TestResourceAmount);
	}
}

void FRTSNomadicBxpShippingTestRunner::StartNextScenario()
{
	M_CurrentScenarioIndex++;
	if (not M_Scenarios.IsValidIndex(M_CurrentScenarioIndex))
	{
		StartUnitAbilityStress();
		return;
	}

	FNomadicBxpShippingScenario& Scenario = M_Scenarios[M_CurrentScenarioIndex];
	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_SCENARIO_BEGIN index=%d name=%s"),
	       M_CurrentScenarioIndex,
	       *Scenario.Name);

	CleanupScenario(Scenario);
	SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::EnsureNomadicBuilding);
}

void FRTSNomadicBxpShippingTestRunner::AdvanceScenario(FNomadicBxpShippingScenario& Scenario)
{
	switch (Scenario.Stage)
	{
	case ENomadicBxpShippingScenarioStage::EnsureNomadicBuilding:
		AdvanceEnsureNomadicBuilding(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::RequestBxpSpawn:
		{
			FString FailureReason;
			if (TryStartBxpSpawn(Scenario, FailureReason))
			{
				if (Scenario.Type == ENomadicBxpShippingScenarioType::CancelDuringAsyncLoad)
				{
					if (TryCancelActiveBxpPreview(Scenario, false))
					{
						SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::VerifyNoLiveBxp);
						break;
					}

					LogScenarioFailure(Scenario, TEXT("Could not cancel active BXP preview during async load."));
					break;
				}

				if (Scenario.Type == ENomadicBxpShippingScenarioType::CancelAsyncThenBuildAgain &&
					Scenario.RetryCount == 0)
				{
					if (TryCancelActiveBxpPreview(Scenario, false))
					{
						Scenario.RetryCount++;
						SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForSlotClearBeforeRetry);
						break;
					}

					LogScenarioFailure(Scenario, TEXT("Could not cancel active BXP preview before retry."));
					break;
				}

				SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForBxpSpawn);
				break;
			}

			if (FailureReason.IsEmpty())
			{
				FailureReason = TEXT("Failed to start BXP async spawn.");
			}

			LogScenarioFailure(Scenario, FailureReason);
		}
		break;
	case ENomadicBxpShippingScenarioStage::WaitForBxpSpawn:
		AdvanceWaitForBxpSpawn(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::PlaceBxp:
		AdvancePlaceBxp(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitBeforeWeirdAction:
		AdvanceWaitBeforeWeirdAction(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForBxpBuilt:
		AdvanceWaitForBxpBuilt(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForNomadicTruck:
		AdvanceWaitForNomadicTruck(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForNomadicTruckAndNoLiveBxp:
		AdvanceWaitForNomadicTruckAndNoLiveBxp(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForSlotClearBeforeRetry:
		AdvanceWaitForSlotClearBeforeRetry(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::RebuildNomadicBuilding:
		AdvanceRebuildNomadicBuilding(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForRebuildSpawn:
		AdvanceWaitForRebuildSpawn(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::WaitForRebuiltBxp:
		AdvanceWaitForRebuiltBxp(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::VerifyNoLiveBxp:
		AdvanceVerifyNoLiveBxp(Scenario);
		break;
	case ENomadicBxpShippingScenarioStage::Done:
	case ENomadicBxpShippingScenarioStage::NotStarted:
	default:
		break;
	}
}

void FRTSNomadicBxpShippingTestRunner::SetScenarioStage(
	FNomadicBxpShippingScenario& Scenario,
	const ENomadicBxpShippingScenarioStage NewStage)
{
	Scenario.Stage = NewStage;
	Scenario.StageStartedSeconds = RTS::UnitTests::NomadicBxpShipping::GetWorldSeconds(GetWorld());
	Scenario.bActionStarted = false;

	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_SCENARIO_STAGE name=%s stage=%s"),
	       *Scenario.Name,
	       *GetScenarioStageString(NewStage));
}

void FRTSNomadicBxpShippingTestRunner::AdvanceEnsureNomadicBuilding(FNomadicBxpShippingScenario& Scenario)
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		LogScenarioFailure(Scenario, TEXT("Nomadic actor became invalid."));
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::Building)
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::RequestBxpSpawn);
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::Truck && not Scenario.bActionStarted)
	{
		Scenario.bActionStarted = TryQueueNomadicBuildCommand(*Nomadic);
		return;
	}

	if (GetIsStageTimedOut(
		Scenario,
		RTS::UnitTests::NomadicBxpShipping::NomadicConversionFastForwardDelaySeconds) &&
		TryFastForwardNomadicBuildingConversion(Scenario))
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::RequestBxpSpawn);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::NomadicConversionTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for nomadic to convert to building."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForBxpSpawn(FNomadicBxpShippingScenario& Scenario)
{
	ABuildingExpansion* SpawnedBxp = GetScenarioSlotBxp(Scenario);
	if (IsValid(SpawnedBxp))
	{
		Scenario.Bxp = SpawnedBxp;
		if (Scenario.Type == ENomadicBxpShippingScenarioType::CancelBeforePlacement)
		{
			TScriptInterface<IBuildingExpansionOwner> Owner;
			ACPPController* Controller = GetPlayerController();
			if (IsValid(Controller) && TryGetOwnerInterface(*Scenario.Nomadic.Get(), Owner))
			{
				Controller->OnClickedCancelBxpPlacement(Owner, false, Scenario.BxpOption.ExpansionType);
				SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::VerifyNoLiveBxp);
				return;
			}
		}

		if (Scenario.Type == ENomadicBxpShippingScenarioType::PackOwnerWithActivePreview)
		{
			SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitBeforeWeirdAction);
			return;
		}

		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::PlaceBxp);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpSpawnTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for BXP async spawn."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvancePlaceBxp(FNomadicBxpShippingScenario& Scenario)
{
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller))
	{
		LogScenarioFailure(Scenario, TEXT("Player controller invalid while placing BXP."));
		return;
	}

	FVector ClickedLocation = FVector::ZeroVector;
	if (not TryGetPlacementClickLocation(Scenario, ClickedLocation))
	{
		LogScenarioFailure(Scenario, TEXT("Could not calculate a placement click location."));
		return;
	}

	if (not Controller->ShippingTest_PlaceLoadedBxpAtLocation(ClickedLocation))
	{
		LogScenarioFailure(Scenario, TEXT("Controller failed to place the loaded BXP."));
		return;
	}

	if (Scenario.Type == ENomadicBxpShippingScenarioType::DestroyDuringConstruction ||
		Scenario.Type == ENomadicBxpShippingScenarioType::BuildThenCancelOwnerPack ||
		(Scenario.Type == ENomadicBxpShippingScenarioType::CancelConstructionThenBuildAgain &&
			Scenario.RetryCount == 0))
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitBeforeWeirdAction);
		return;
	}

	SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForBxpBuilt);
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitBeforeWeirdAction(FNomadicBxpShippingScenario& Scenario)
{
	const double Elapsed =
		RTS::UnitTests::NomadicBxpShipping::GetWorldSeconds(GetWorld()) - Scenario.StageStartedSeconds;
	if (Elapsed < RTS::UnitTests::NomadicBxpShipping::WeirdTimingDelaySeconds)
	{
		return;
	}

	ACPPController* Controller = GetPlayerController();
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Controller) || not IsValid(Nomadic))
	{
		LogScenarioFailure(Scenario, TEXT("Controller or nomadic invalid during weird timing action."));
		return;
	}

	switch (Scenario.Type)
	{
	case ENomadicBxpShippingScenarioType::DestroyDuringConstruction:
		AdvanceDestroyDuringConstructionAction(Scenario, *Controller, *Nomadic);
		return;
	case ENomadicBxpShippingScenarioType::BuildThenCancelOwnerPack:
		AdvanceBuildThenCancelOwnerPackAction(Scenario, *Controller, *Nomadic);
		return;
	case ENomadicBxpShippingScenarioType::PackOwnerWithActivePreview:
		AdvancePackOwnerWithActivePreviewAction(Scenario, *Controller, *Nomadic);
		return;
	case ENomadicBxpShippingScenarioType::CancelConstructionThenBuildAgain:
		AdvanceCancelConstructionThenBuildAgainAction(Scenario, *Controller, *Nomadic);
		return;
	default:
		LogScenarioFailure(Scenario, TEXT("Unsupported weird timing action."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceDestroyDuringConstructionAction(
	FNomadicBxpShippingScenario& Scenario,
	ACPPController& Controller,
	ANomadicVehicle& Nomadic)
{
	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (TryGetOwnerInterface(Nomadic, Owner))
	{
		Controller.CancelBuildingExpansionConstruction(Owner, Scenario.Bxp.Get(), false);
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::VerifyNoLiveBxp);
		return;
	}

	LogScenarioFailure(Scenario, TEXT("Owner interface invalid while cancelling BXP construction."));
}

void FRTSNomadicBxpShippingTestRunner::AdvanceBuildThenCancelOwnerPackAction(
	FNomadicBxpShippingScenario& Scenario,
	ACPPController& Controller,
	ANomadicVehicle& Nomadic)
{
	if (not Scenario.bActionStarted)
	{
		Controller.ConvertBackToVehicle(&Nomadic);
		Scenario.bActionStarted = true;
		Scenario.StageStartedSeconds = RTS::UnitTests::NomadicBxpShipping::GetWorldSeconds(GetWorld());
		return;
	}

	Controller.CancelVehicleConversion(&Nomadic);
	SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForBxpBuilt);
}

void FRTSNomadicBxpShippingTestRunner::AdvancePackOwnerWithActivePreviewAction(
	FNomadicBxpShippingScenario& Scenario,
	ACPPController& Controller,
	ANomadicVehicle& Nomadic)
{
	if (not Scenario.bActionStarted)
	{
		Controller.ConvertBackToVehicle(&Nomadic);
		Scenario.bActionStarted = true;
		Scenario.StageStartedSeconds = RTS::UnitTests::NomadicBxpShipping::GetWorldSeconds(GetWorld());
		return;
	}

	if (TryCancelActiveBxpPreview(Scenario, false))
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForNomadicTruckAndNoLiveBxp);
		return;
	}

	LogScenarioFailure(Scenario, TEXT("Could not cancel BXP preview after packing owner."));
}

void FRTSNomadicBxpShippingTestRunner::AdvanceCancelConstructionThenBuildAgainAction(
	FNomadicBxpShippingScenario& Scenario,
	ACPPController& Controller,
	ANomadicVehicle& Nomadic)
{
	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (TryGetOwnerInterface(Nomadic, Owner))
	{
		Controller.CancelBuildingExpansionConstruction(Owner, Scenario.Bxp.Get(), false);
		Scenario.RetryCount++;
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForSlotClearBeforeRetry);
		return;
	}

	LogScenarioFailure(Scenario, TEXT("Owner interface invalid while cancelling BXP construction before retry."));
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForBxpBuilt(FNomadicBxpShippingScenario& Scenario)
{
	ABuildingExpansion* Bxp = GetScenarioSlotBxp(Scenario);
	if (RTS::UnitTests::NomadicBxpShipping::GetIsBxpBuilt(Bxp))
	{
		Scenario.Bxp = Bxp;
		if (Scenario.Type == ENomadicBxpShippingScenarioType::BuildPackAndRebuild)
		{
			ACPPController* Controller = GetPlayerController();
			ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
			if (IsValid(Controller) && IsValid(Nomadic))
			{
				Controller->ConvertBackToVehicle(Nomadic);
				SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForNomadicTruck);
				return;
			}
		}

		LogScenarioSuccess(Scenario);
		return;
	}

	if (not IsValid(Bxp))
	{
		LogScenarioFailure(Scenario, TEXT("BXP became invalid before it was built."));
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpBuildTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for BXP to finish construction."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForNomadicTruck(FNomadicBxpShippingScenario& Scenario)
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		LogScenarioFailure(Scenario, TEXT("Nomadic became invalid while packing to truck."));
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::Truck)
	{
		const FBuildingExpansionItem* Item = GetScenarioSlotItem(Scenario);
		if (Item == nullptr || Item->ExpansionStatus != EBuildingExpansionStatus::BXS_PackedUp)
		{
			LogScenarioFailure(Scenario, TEXT("BXP slot was not packed after nomadic converted to truck."));
			return;
		}

		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::RebuildNomadicBuilding);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::NomadicConversionTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for nomadic to pack into truck."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForNomadicTruckAndNoLiveBxp(
	FNomadicBxpShippingScenario& Scenario)
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		LogScenarioFailure(Scenario, TEXT("Nomadic became invalid while waiting for clean truck state."));
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::Truck && GetHasNoLiveBxpAtScenarioSlot(Scenario))
	{
		LogScenarioSuccess(Scenario);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::NomadicConversionTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for nomadic truck with no live BXP."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForSlotClearBeforeRetry(
	FNomadicBxpShippingScenario& Scenario)
{
	if (GetHasNoLiveBxpAtScenarioSlot(Scenario))
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::RequestBxpSpawn);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpSpawnTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for BXP slot to clear before retry."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceRebuildNomadicBuilding(FNomadicBxpShippingScenario& Scenario)
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		LogScenarioFailure(Scenario, TEXT("Nomadic invalid while rebuilding from packed BXP state."));
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::Truck && not Scenario.bActionStarted)
	{
		Scenario.bActionStarted = TryQueueNomadicBuildCommand(*Nomadic);
		return;
	}

	if (Nomadic->GetNomadicStatus() != ENomadStatus::Building)
	{
		if (GetIsStageTimedOut(
			Scenario,
			RTS::UnitTests::NomadicBxpShipping::NomadicConversionFastForwardDelaySeconds))
		{
			(void)TryFastForwardNomadicBuildingConversion(Scenario);
		}
	}

	if (Nomadic->GetNomadicStatus() != ENomadStatus::Building)
	{
		if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::NomadicConversionTimeoutSeconds))
		{
			LogScenarioFailure(Scenario, TEXT("Timed out rebuilding nomadic before packed BXP restore."));
		}
		return;
	}

	if (Scenario.BxpOption.BxpConstructionRules.ConstructionType != EBxpConstructionType::Free)
	{
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForRebuiltBxp);
		return;
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		LogScenarioFailure(Scenario, TEXT("Owner interface invalid for free packed BXP rebuild."));
		return;
	}

	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller))
	{
		LogScenarioFailure(Scenario, TEXT("Controller invalid for free packed BXP rebuild."));
		return;
	}

	Controller->RebuildPackedExpansion(Scenario.BxpOption, Owner, Scenario.ExpansionSlotIndex);
	SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForRebuildSpawn);
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForRebuildSpawn(FNomadicBxpShippingScenario& Scenario)
{
	ABuildingExpansion* RebuiltBxp = GetScenarioSlotBxp(Scenario);
	if (IsValid(RebuiltBxp))
	{
		Scenario.Bxp = RebuiltBxp;
		ACPPController* Controller = GetPlayerController();
		FVector ClickedLocation = FVector::ZeroVector;
		if (not IsValid(Controller) || not TryGetPlacementClickLocation(Scenario, ClickedLocation))
		{
			LogScenarioFailure(Scenario, TEXT("Could not place free packed BXP after rebuild spawn."));
			return;
		}
		if (not Controller->ShippingTest_PlaceLoadedBxpAtLocation(ClickedLocation))
		{
			LogScenarioFailure(Scenario, TEXT("Controller failed to place rebuilt free packed BXP."));
			return;
		}
		SetScenarioStage(Scenario, ENomadicBxpShippingScenarioStage::WaitForRebuiltBxp);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpSpawnTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for free packed BXP rebuild spawn."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceWaitForRebuiltBxp(FNomadicBxpShippingScenario& Scenario)
{
	ABuildingExpansion* RebuiltBxp = GetScenarioSlotBxp(Scenario);
	if (RTS::UnitTests::NomadicBxpShipping::GetIsBxpBuilt(RebuiltBxp))
	{
		Scenario.Bxp = RebuiltBxp;
		LogScenarioSuccess(Scenario);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpBuildTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for rebuilt BXP to finish construction."));
	}
}

void FRTSNomadicBxpShippingTestRunner::AdvanceVerifyNoLiveBxp(FNomadicBxpShippingScenario& Scenario)
{
	if (GetHasNoLiveBxpAtScenarioSlot(Scenario))
	{
		LogScenarioSuccess(Scenario);
		return;
	}

	if (GetIsStageTimedOut(Scenario, RTS::UnitTests::NomadicBxpShipping::BxpSpawnTimeoutSeconds))
	{
		LogScenarioFailure(Scenario, TEXT("Timed out waiting for BXP slot to clear."));
	}
}

bool FRTSNomadicBxpShippingTestRunner::GetIsStageTimedOut(
	const FNomadicBxpShippingScenario& Scenario,
	const float TimeoutSeconds) const
{
	const double Elapsed = RTS::UnitTests::NomadicBxpShipping::GetWorldSeconds(GetWorld()) - Scenario.StageStartedSeconds;
	return Elapsed > TimeoutSeconds;
}

bool FRTSNomadicBxpShippingTestRunner::TryQueueNomadicBuildCommand(ANomadicVehicle& Nomadic) const
{
	const FVector BuildLocation = Nomadic.GetActorLocation();
	const FRotator BuildRotation = Nomadic.GetActorRotation();
	const ECommandQueueError Error = Nomadic.CreateBuildingAtLocation(BuildLocation, BuildRotation, true);
	if (Error != ECommandQueueError::NoError)
	{
		UE_LOG(LogRTSNomadicBxpShippingTests, Error,
		       TEXT("Failed to queue nomadic build command for %s error=%d"),
		       *Nomadic.GetName(),
		       static_cast<int32>(Error));
		return false;
	}
	return true;
}

bool FRTSNomadicBxpShippingTestRunner::TryFastForwardNomadicBuildingConversion(
	FNomadicBxpShippingScenario& Scenario) const
{
	ANomadicVehicle* const Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		return false;
	}

	const ENomadStatus PreviousStatus = Nomadic->GetNomadicStatus();
	const bool bFinished = Nomadic->ShippingTest_ForceFinishConversionToBuilding();
	if (not bFinished)
	{
		return false;
	}

	UE_LOG(LogRTSNomadicBxpShippingTests, Warning,
	       TEXT("RTS_NOMADIC_BXP_CONVERSION_FAST_FORWARD name=%s previous_status=%d"),
	       *Scenario.Name,
	       static_cast<int32>(PreviousStatus));
	return true;
}

bool FRTSNomadicBxpShippingTestRunner::TryStartBxpSpawn(
	FNomadicBxpShippingScenario& Scenario,
	FString& OutFailureReason) const
{
	OutFailureReason.Empty();

	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Nomadic) || not IsValid(Controller))
	{
		OutFailureReason = TEXT("Nomadic actor or player controller invalid while starting BXP spawn.");
		return false;
	}

	const ARTSAsyncSpawner* AsyncSpawner = Controller->GetRTSAsyncSpawner();
	if (not IsValid(AsyncSpawner))
	{
		OutFailureReason = TEXT("RTS async spawner invalid while starting BXP spawn.");
		return false;
	}

	const EBuildingExpansionType ExpansionType = Scenario.BxpOption.ExpansionType;
	if (not AsyncSpawner->ShippingTest_GetHasBxpPreviewMeshMapping(ExpansionType))
	{
		OutFailureReason = TEXT("Missing BXP preview mesh mapping.");
		return false;
	}

	if (not AsyncSpawner->ShippingTest_GetHasBxpSpawnClassMapping(ExpansionType))
	{
		OutFailureReason = TEXT("Missing BXP spawn class mapping.");
		return false;
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		OutFailureReason = TEXT("Could not get building expansion owner interface.");
		return false;
	}

	Controller->ExpandBuildingWithBxpAndStartPreview(
		Scenario.BxpOption,
		Owner,
		Scenario.ExpansionSlotIndex,
		false);

	if (Controller->ShippingTest_GetIsBxpPlacementPreviewActive())
	{
		return true;
	}

	Controller->OnClickedCancelBxpPlacement(Owner, false, Scenario.BxpOption.ExpansionType);
	OutFailureReason = TEXT("BXP placement preview did not become active.");
	return false;
}

bool FRTSNomadicBxpShippingTestRunner::TryCancelActiveBxpPreview(
	const FNomadicBxpShippingScenario& Scenario,
	const bool bIsPackedPreview) const
{
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not Controller->ShippingTest_GetIsBxpPlacementPreviewActive())
	{
		return false;
	}

	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		return false;
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		return false;
	}

	Controller->OnClickedCancelBxpPlacement(Owner, bIsPackedPreview, Scenario.BxpOption.ExpansionType);
	return true;
}

bool FRTSNomadicBxpShippingTestRunner::TryGetOwnerInterface(
	ANomadicVehicle& Nomadic,
	TScriptInterface<IBuildingExpansionOwner>& OutOwner) const
{
	OutOwner.SetObject(&Nomadic);
	OutOwner.SetInterface(static_cast<IBuildingExpansionOwner*>(&Nomadic));
	return OutOwner.GetObject() != nullptr && OutOwner.GetInterface() != nullptr;
}

bool FRTSNomadicBxpShippingTestRunner::TryGetPlacementClickLocation(
	const FNomadicBxpShippingScenario& Scenario,
	FVector& OutClickedLocation) const
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		return false;
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		return false;
	}

	const EBxpConstructionType ConstructionType = Scenario.BxpOption.BxpConstructionRules.ConstructionType;
	if (ConstructionType == EBxpConstructionType::Socket)
	{
		UStaticMeshComponent* AttachMesh = Owner->GetAttachToMeshComponent();
		if (not IsValid(AttachMesh))
		{
			return false;
		}

		const FName ConfiguredSocketName = Scenario.BxpOption.BxpConstructionRules.SocketName;
		if (not ConfiguredSocketName.IsNone() && AttachMesh->DoesSocketExist(ConfiguredSocketName))
		{
			OutClickedLocation = AttachMesh->GetSocketLocation(ConfiguredSocketName);
			return true;
		}

		const TArray<UStaticMeshSocket*> FreeSockets = Owner->GetFreeSocketList(Scenario.ExpansionSlotIndex);
		if (FreeSockets.IsEmpty())
		{
			return false;
		}

		const int32 SocketIndex = M_CurrentScenarioIndex % FreeSockets.Num();
		const UStaticMeshSocket* Socket = FreeSockets[SocketIndex];
		if (Socket == nullptr)
		{
			return false;
		}

		OutClickedLocation = AttachMesh->GetSocketLocation(Socket->SocketName);
		return true;
	}

	if (ConstructionType == EBxpConstructionType::AtBuildingOrigin)
	{
		UStaticMeshComponent* AttachMesh = Owner->GetAttachToMeshComponent();
		if (not IsValid(AttachMesh))
		{
			return false;
		}

		OutClickedLocation = AttachMesh->GetComponentLocation();
		return true;
	}

	const float ExpansionRange = FMath::Max(Owner->GetBxpExpansionRange(), 600.0f);
	const float AngleDegrees = static_cast<float>((M_CurrentScenarioIndex * 47) % 360);
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	OutClickedLocation = Owner->GetBxpOwnerLocation() + FVector(
		FMath::Cos(AngleRadians) * ExpansionRange * 0.5f,
		FMath::Sin(AngleRadians) * ExpansionRange * 0.5f,
		0.0f);
	return true;
}

bool FRTSNomadicBxpShippingTestRunner::GetHasNoLiveBxpAtScenarioSlot(
	const FNomadicBxpShippingScenario& Scenario) const
{
	const ABuildingExpansion* Bxp = GetScenarioSlotBxp(Scenario);
	return not IsValid(Bxp);
}

bool FRTSNomadicBxpShippingTestRunner::GetScenarioNamePassesFilters(const FString& ScenarioName) const
{
	if (not M_NameFilter.IsEmpty() && not ScenarioName.Contains(M_NameFilter))
	{
		return false;
	}

	for (const FString& ExcludeNameFilter : M_ExcludeNameFilters)
	{
		if (not ExcludeNameFilter.IsEmpty() && ScenarioName.Contains(ExcludeNameFilter))
		{
			return false;
		}
	}

	return true;
}

ACPPController* FRTSNomadicBxpShippingTestRunner::GetPlayerController() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	return Cast<ACPPController>(World->GetFirstPlayerController());
}

ABuildingExpansion* FRTSNomadicBxpShippingTestRunner::GetScenarioSlotBxp(
	const FNomadicBxpShippingScenario& Scenario) const
{
	const FBuildingExpansionItem* Item = GetScenarioSlotItem(Scenario);
	return Item != nullptr ? Item->Expansion : nullptr;
}

FBuildingExpansionItem* FRTSNomadicBxpShippingTestRunner::GetScenarioSlotItem(
	const FNomadicBxpShippingScenario& Scenario) const
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		return nullptr;
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		return nullptr;
	}

	return Owner->GetBuildingExpansionItemAtIndex(Scenario.ExpansionSlotIndex);
}

FString FRTSNomadicBxpShippingTestRunner::GetScenarioTypeString(
	const ENomadicBxpShippingScenarioType Type) const
{
	switch (Type)
	{
	case ENomadicBxpShippingScenarioType::CancelDuringAsyncLoad:
		return TEXT("CancelDuringAsyncLoad");
	case ENomadicBxpShippingScenarioType::CancelAsyncThenBuildAgain:
		return TEXT("CancelAsyncThenBuildAgain");
	case ENomadicBxpShippingScenarioType::CancelBeforePlacement:
		return TEXT("CancelBeforePlacement");
	case ENomadicBxpShippingScenarioType::DestroyDuringConstruction:
		return TEXT("DestroyDuringConstruction");
	case ENomadicBxpShippingScenarioType::BuildThenCancelOwnerPack:
		return TEXT("BuildThenCancelOwnerPack");
	case ENomadicBxpShippingScenarioType::PackOwnerWithActivePreview:
		return TEXT("PackOwnerWithActivePreview");
	case ENomadicBxpShippingScenarioType::CancelConstructionThenBuildAgain:
		return TEXT("CancelConstructionThenBuildAgain");
	case ENomadicBxpShippingScenarioType::BuildPackAndRebuild:
		return TEXT("BuildPackAndRebuild");
	case ENomadicBxpShippingScenarioType::BuildAndDestroyCleanly:
		return TEXT("BuildAndDestroyCleanly");
	}
	return TEXT("Unknown");
}

FString FRTSNomadicBxpShippingTestRunner::GetScenarioStageString(
	const ENomadicBxpShippingScenarioStage Stage) const
{
	switch (Stage)
	{
	case ENomadicBxpShippingScenarioStage::NotStarted: return TEXT("NotStarted");
	case ENomadicBxpShippingScenarioStage::EnsureNomadicBuilding: return TEXT("EnsureNomadicBuilding");
	case ENomadicBxpShippingScenarioStage::RequestBxpSpawn: return TEXT("RequestBxpSpawn");
	case ENomadicBxpShippingScenarioStage::WaitForBxpSpawn: return TEXT("WaitForBxpSpawn");
	case ENomadicBxpShippingScenarioStage::PlaceBxp: return TEXT("PlaceBxp");
	case ENomadicBxpShippingScenarioStage::WaitBeforeWeirdAction: return TEXT("WaitBeforeWeirdAction");
	case ENomadicBxpShippingScenarioStage::WaitForBxpBuilt: return TEXT("WaitForBxpBuilt");
	case ENomadicBxpShippingScenarioStage::WaitForNomadicTruck: return TEXT("WaitForNomadicTruck");
	case ENomadicBxpShippingScenarioStage::WaitForNomadicTruckAndNoLiveBxp:
		return TEXT("WaitForNomadicTruckAndNoLiveBxp");
	case ENomadicBxpShippingScenarioStage::WaitForSlotClearBeforeRetry:
		return TEXT("WaitForSlotClearBeforeRetry");
	case ENomadicBxpShippingScenarioStage::RebuildNomadicBuilding: return TEXT("RebuildNomadicBuilding");
	case ENomadicBxpShippingScenarioStage::WaitForRebuildSpawn: return TEXT("WaitForRebuildSpawn");
	case ENomadicBxpShippingScenarioStage::WaitForRebuiltBxp: return TEXT("WaitForRebuiltBxp");
	case ENomadicBxpShippingScenarioStage::VerifyNoLiveBxp: return TEXT("VerifyNoLiveBxp");
	case ENomadicBxpShippingScenarioStage::Done: return TEXT("Done");
	}
	return TEXT("Unknown");
}

void FRTSNomadicBxpShippingTestRunner::AddScenario(
	ANomadicVehicle& Nomadic,
	const FBxpOptionData& BxpOption,
	const ENomadicBxpShippingScenarioType Type)
{
	FNomadicBxpShippingScenario Scenario;
	Scenario.Nomadic = &Nomadic;
	Scenario.BxpOption = BxpOption;
	Scenario.Type = Type;
	Scenario.ExpansionSlotIndex = 0;
	Scenario.Name = FString::Printf(
		TEXT("%s.%s.%s"),
		*Nomadic.GetName(),
		*Global_GetBxpTypeEnumAsString(BxpOption.ExpansionType),
		*GetScenarioTypeString(Type));

	if (not GetScenarioNamePassesFilters(Scenario.Name))
	{
		return;
	}

	if (M_MaxScenarios > 0 && M_Scenarios.Num() >= M_MaxScenarios)
	{
		return;
	}

	M_Scenarios.Add(Scenario);
}

void FRTSNomadicBxpShippingTestRunner::LogScenarioFailure(
	FNomadicBxpShippingScenario& Scenario,
	const FString& Reason)
{
	M_FailedScenarioCount++;
	UE_LOG(LogRTSNomadicBxpShippingTests, Error,
	       TEXT("RTS_NOMADIC_BXP_SCENARIO_FAIL index=%d name=%s stage=%s reason=%s"),
	       M_CurrentScenarioIndex,
	       *Scenario.Name,
	       *GetScenarioStageString(Scenario.Stage),
	       *Reason);

	CleanupScenario(Scenario);
	Scenario.Stage = ENomadicBxpShippingScenarioStage::Done;
	StartNextScenario();
}

void FRTSNomadicBxpShippingTestRunner::LogScenarioSuccess(FNomadicBxpShippingScenario& Scenario)
{
	M_PassedScenarioCount++;
	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_SCENARIO_PASS index=%d name=%s"),
	       M_CurrentScenarioIndex,
	       *Scenario.Name);

	CleanupScenario(Scenario);
	Scenario.Stage = ENomadicBxpShippingScenarioStage::Done;
	StartNextScenario();
}

void FRTSNomadicBxpShippingTestRunner::CleanupScenario(FNomadicBxpShippingScenario& Scenario) const
{
	ANomadicVehicle* Nomadic = Scenario.Nomadic.Get();
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Nomadic) || not IsValid(Controller))
	{
		return;
	}

	if (Nomadic->GetNomadicStatus() == ENomadStatus::CreatingTruck)
	{
		Controller->CancelVehicleConversion(Nomadic);
	}

	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not TryGetOwnerInterface(*Nomadic, Owner))
	{
		return;
	}

	if (Controller->ShippingTest_GetIsBxpPlacementPreviewActive())
	{
		const bool bIsPackedPreview = Scenario.Stage == ENomadicBxpShippingScenarioStage::WaitForRebuildSpawn;
		(void)TryCancelActiveBxpPreview(Scenario, bIsPackedPreview);
	}

	ABuildingExpansion* Bxp = GetScenarioSlotBxp(Scenario);
	if (IsValid(Bxp))
	{
		Controller->CancelBuildingExpansionConstruction(Owner, Bxp, false);
	}
}

#endif
