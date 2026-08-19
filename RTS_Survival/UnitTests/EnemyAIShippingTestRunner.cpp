// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CoreMinimal.h"

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyWaves/EnemyWaveController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActionRequirements/StrategicAIActionRequirements.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/StrategicTrainingState/StrategicTrainingState.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Stats/Stats.h"
#include "Tickable.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyAIShippingTests, Display, All);

namespace RTS::UnitTests::EnemyAIShipping
{
	constexpr float RuntimeReadyTimeoutSeconds = 15.f;
	constexpr float CooldownObservationSeconds = 4.f;
	// The strategic AI populates the blackboard through a chain of async requests (base clusters ->
	// defense positions -> construction -> mines), each on a ~20s cadence, so allow ample time before
	// concluding a value could not be produced. If it is still missing after this, it is a real defect.
	constexpr float StrategicStateTimeoutSeconds = 180.f;
	constexpr float MissionActivityTimeoutSeconds = 90.f;
	constexpr float GarbageCollectionSettleSeconds = 1.5f;
	constexpr float StochasticObservationSeconds = 12.f;
	constexpr float TrainingPressureTolerance = 0.1f;
	constexpr float TrainingRequirementTimeMarginSeconds = 1.f;
	constexpr float SyntheticTrainingLocationOffset = 100.f;
	constexpr int32 DefaultGarbageCollectionCycles = 3;
	constexpr int32 DefaultDifficultyPercentage = 100;
	constexpr int32 MaxTestMapTravelAttempts = 3;
	constexpr int32 ExpectedRoadSplineCount = 3;
	constexpr int32 ExpectedPlayerUnitBulkCount = 2;
	constexpr int32 MinimumCompletedAttackMoveWaves = 2;
	constexpr int32 MinimumCompletedPatrolWaves = 1;
	constexpr int32 ExpectedLightTankRequirementAmount = 5;
	constexpr int32 ExpectedAnyTankRequirementAmount = 3;
	const TCHAR* TestMapPackageName = TEXT("/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI");

	struct FExpectedNativeRequirementDefinition
	{
		const UClass* RequirementClass = nullptr;
		int32 RequiredAmount = INDEX_NONE;
		EAITrainingFocus MissingUnitFocusPressure = EAITrainingFocus::NoFocus;
	};

	struct FExpectedSubActionDefinition
	{
		ESubtypeAction SubtypeAction;
		EStrategicAITopLevelAction TopLevelAction;
		EAITrainingFocus TrainingFocus;
		EAITrainingFocusSpecialty TrainingSpecialty;
		TArray<FExpectedNativeRequirementDefinition> NativeRequirements;
	};

	struct FDecisionTreeLayoutSnapshot
	{
		TMap<EStrategicAITopLevelAction, int32> TopLevelActionCounts;
		TMap<ESubtypeAction, EStrategicAITopLevelAction> SubActionMembership;
		int32 SubActionCount = 0;
		int32 DuplicateSubActionCount = 0;
		int32 UnexpectedSubActionCount = 0;
		int32 InvalidObjectCount = 0;
	};

	const TArray<FExpectedSubActionDefinition>& GetExpectedSubActionDefinitions()
	{
		static const TArray<FExpectedSubActionDefinition> ExpectedDefinitions =
		{
			{
				ESubtypeAction::AttackMoveToPlayerUnits,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::Infantry,
				EAITrainingFocusSpecialty::AntiInfantry,
				{},
			},
			{
				ESubtypeAction::AttackMoveToPlayerHQ,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::MediumTanks,
				EAITrainingFocusSpecialty::SpecialWeaponCarrier,
				{
					{ UStrategicAIHasPlayerHQLocationRequirement::StaticClass() },
				},
			},
			{
				ESubtypeAction::AttackMoveToPlayerResourceBuildings,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::LightTanks,
				EAITrainingFocusSpecialty::GeneralPurpose,
				{
					{ UStrategicAIHasPlayerResourceBuildingLocationsRequirement::StaticClass() },
				},
			},
			{
				ESubtypeAction::AttackMoveLightTanksToPlayerUnits,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::LightTanks,
				EAITrainingFocusSpecialty::GeneralPurpose,
				{
					{ UStrategicAIHasEnoughLightTanks::StaticClass(), ExpectedLightTankRequirementAmount },
				},
			},
			{
				ESubtypeAction::FlankPlayerHeavies,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::LightTanks,
				EAITrainingFocusSpecialty::AntiTank,
				{
					{ UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions::StaticClass() },
					{
						UStrategicAIHasAtLeastAnyIdleTanks::StaticClass(),
						ExpectedAnyTankRequirementAmount,
						EAITrainingFocus::LightTanks,
					},
				},
			},
			{
				// UT_EnemyAI authors a single Attack action group that holds every sub-action, including
				// DefendBase (see the map's StochasticDecisionTree_0 layout). The defend behaviour lives
				// under the Attack top-level action rather than a separate Defend branch.
				ESubtypeAction::DefendBase,
				EStrategicAITopLevelAction::Attack,
				EAITrainingFocus::Infantry,
				// The map's DefendBase instance intentionally emits no specialty pressure (its serialized
				// M_SpecialtyPressure overrides the C++ constructor default), so it contributes focus only.
				EAITrainingFocusSpecialty::NoTrainingPressure,
				{},
			},
		};
		return ExpectedDefinitions;
	}

	const FExpectedSubActionDefinition* FindExpectedSubActionDefinition(const ESubtypeAction SubtypeAction)
	{
		return GetExpectedSubActionDefinitions().FindByPredicate(
			[SubtypeAction](const FExpectedSubActionDefinition& Definition)
			{
				return Definition.SubtypeAction == SubtypeAction;
			});
	}

	UStrategicAISubAction* FindSubActionByType(
		const TArray<FStrategicAIAction>& ActionDefinitions,
		const ESubtypeAction SubtypeAction)
	{
		for (const FStrategicAIAction& Action : ActionDefinitions)
		{
			for (UStrategicAISubAction* SubAction : Action.GetSubActions())
			{
				if (IsValid(SubAction) && SubAction->SubtypeAction == SubtypeAction)
				{
					return SubAction;
				}
			}
		}

		return nullptr;
	}

	enum class EEnemyAIShippingTestPhase : uint8
	{
		WaitForRuntime,
		ObserveAbilityCooldowns,
		WaitForStrategicState,
		ValidateTrainingBuckets,
		WaitForMissionActivity,
		GarbageCollectionStress,
		ObserveStochasticTree,
		Complete,
	};

	bool GetIsCurrentMapSupported(const UWorld& World)
	{
		return World.GetMapName().Contains(TEXT("UT_EnemyAI"));
	}

	bool GetWasRequestedOnCommandLine()
	{
		FString RequestedMapTest;
		const bool bHasNamedRun = FParse::Value(FCommandLine::Get(), TEXT("RTSRunMapTest="), RequestedMapTest);
		return FParse::Param(FCommandLine::Get(), TEXT("RTSRunEnemyAITests"))
			|| (bHasNamedRun && (RequestedMapTest == TEXT("EnemyAI") || RequestedMapTest == TEXT("All")));
	}

	bool GetIsUsableStrategicLocation(const FVector& Location)
	{
		return not Location.ContainsNaN() && not Location.IsNearlyZero();
	}

	template <typename TKey>
	float GetBucketValue(const TMap<TKey, float>& Buckets, const TKey Key)
	{
		const float* Value = Buckets.Find(Key);
		return Value != nullptr ? *Value : 0.f;
	}

	void AddSyntheticIdleUnitEntries(
		FStrategicAIBlackboard& Blackboard,
		AActor& SyntheticUnitActor,
		const EAllUnitType UnitType,
		const int32 UnitSubtypeRaw,
		const int32 Amount)
	{
		for (int32 EntryIndex = 0; EntryIndex < FMath::Max(0, Amount); ++EntryIndex)
		{
			FBlackboardIdleUnitEntry Entry(&SyntheticUnitActor);
			Entry.UnitType = UnitType;
			Entry.UnitSubtypeRaw = UnitSubtypeRaw;
			Blackboard.IdleDirectControlUnits.Add(Entry);
		}
	}

	bool ConfigureControlledTypedUnitRequirement(
		const UStrategicAIActionRequirement& Requirement,
		FStrategicAIBlackboard& Blackboard,
		AActor& SyntheticUnitActor)
	{
		if (const UStrategicAIHasAtLeastIdleSquads* SquadRequirement =
			Cast<UStrategicAIHasAtLeastIdleSquads>(&Requirement))
		{
			AddSyntheticIdleUnitEntries(
				Blackboard,
				SyntheticUnitActor,
				EAllUnitType::UNType_Squad,
				static_cast<int32>(SquadRequirement->RequiredSquadSubtype),
				SquadRequirement->AmountIdleSpecificSquadsNeeded);
			return true;
		}

		if (const UStrategicAIHasAtLeastIdleTanks* TankRequirement =
			Cast<UStrategicAIHasAtLeastIdleTanks>(&Requirement))
		{
			AddSyntheticIdleUnitEntries(
				Blackboard,
				SyntheticUnitActor,
				EAllUnitType::UNType_Tank,
				static_cast<int32>(TankRequirement->RequiredTankSubtype),
				TankRequirement->AmountIdleOfSpecificTanksNeeded);
			return true;
		}

		if (const UStrategicAIHasAtLeastIdleAircraft* AircraftRequirement =
			Cast<UStrategicAIHasAtLeastIdleAircraft>(&Requirement))
		{
			AddSyntheticIdleUnitEntries(
				Blackboard,
				SyntheticUnitActor,
				EAllUnitType::UNType_Aircraft,
				static_cast<int32>(AircraftRequirement->RequiredAircraftSubtype),
				AircraftRequirement->AmountIdleAircraftNeeded);
			return true;
		}

		return false;
	}

	bool ConfigureControlledCategoryUnitRequirement(
		const UStrategicAIActionRequirement& Requirement,
		FStrategicAIBlackboard& Blackboard,
		AActor& SyntheticUnitActor)
	{
		EAllUnitType UnitType = EAllUnitType::UNType_None;
		ETankSubtype TankSubtype = ETankSubtype::Tank_None;
		int32 RequiredAmount = 0;
		if (const UStrategicAIHasAtLeastAnyIdleUnits* AnyUnitRequirement =
			Cast<UStrategicAIHasAtLeastAnyIdleUnits>(&Requirement))
		{
			UnitType = EAllUnitType::UNType_Nomadic;
			RequiredAmount = AnyUnitRequirement->AmountIdleNeeded;
		}
		else if (const UStrategicAIHasAtLeastAnyIdleTanks* AnyTankRequirement =
			Cast<UStrategicAIHasAtLeastAnyIdleTanks>(&Requirement))
		{
			UnitType = EAllUnitType::UNType_Tank;
			RequiredAmount = AnyTankRequirement->AmountIdleTanksNeeded;
		}
		else if (const UStrategicAIHasEnoughLightTanks* LightTankRequirement =
			Cast<UStrategicAIHasEnoughLightTanks>(&Requirement))
		{
			UnitType = EAllUnitType::UNType_Tank;
			TankSubtype = ETankSubtype::Tank_T70;
			RequiredAmount = LightTankRequirement->AmountIdleLightTanksNeeded;
		}
		else if (const UStrategicAIHasEnoughHeavyTanks* HeavyTankRequirement =
			Cast<UStrategicAIHasEnoughHeavyTanks>(&Requirement))
		{
			UnitType = EAllUnitType::UNType_Tank;
			TankSubtype = ETankSubtype::Tank_KV_1;
			RequiredAmount = HeavyTankRequirement->AmountIdleHeavyTanksNeeded;
		}
		else
		{
			return false;
		}

		AddSyntheticIdleUnitEntries(
			Blackboard,
			SyntheticUnitActor,
			UnitType,
			static_cast<int32>(TankSubtype),
			RequiredAmount);
		return true;
	}

	void ConfigureControlledNonUnitRequirement(
		const UStrategicAIActionRequirement& Requirement,
		FStrategicAIBlackboard& Blackboard,
		AActor& SyntheticUnitActor,
		float& InOutGameTimeSeconds)
	{
		if (const UStrategicAIGameTimePassedRequirement* TimeRequirement =
			Cast<UStrategicAIGameTimePassedRequirement>(&Requirement))
		{
			InOutGameTimeSeconds = FMath::Max(
				InOutGameTimeSeconds,
				TimeRequirement->GetRequiredGameTimeSeconds() + TrainingRequirementTimeMarginSeconds);
			return;
		}

		const FVector SyntheticLocation = SyntheticUnitActor.GetActorLocation()
			+ FVector(SyntheticTrainingLocationOffset, SyntheticTrainingLocationOffset, 0.f);
		if (Requirement.IsA<UStrategicAIHasPlayerHQLocationRequirement>())
		{
			Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation = SyntheticLocation;
			return;
		}
		if (Requirement.IsA<UStrategicAIHasPlayerResourceBuildingLocationsRequirement>())
		{
			Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings.AddUnique(SyntheticLocation);
			return;
		}
		if (const UStrategicAIDoesPlayerHaveHeavyTanks* PlayerHeavyRequirement =
			Cast<UStrategicAIDoesPlayerHaveHeavyTanks>(&Requirement))
		{
			Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks = FMath::Max(
				Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks,
				PlayerHeavyRequirement->AmountPlayerHeaviesNeeded);
			return;
		}
		if (not Requirement.IsA<UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions>())
		{
			return;
		}

		FWeakActorLocations SyntheticFlankLocations;
		SyntheticFlankLocations.Actor = &SyntheticUnitActor;
		SyntheticFlankLocations.Locations.Add(SyntheticLocation);
		FResultClosestFlankableEnemyHeavy SyntheticFlankResult;
		SyntheticFlankResult.FlankLocationsAroundHeavyTank.Add(MoveTemp(SyntheticFlankLocations));
		Blackboard.AgreggatedHeavyTankFlankingResults.Add(MoveTemp(SyntheticFlankResult));
	}

	void ConfigureControlledTrainingRequirement(
		const UStrategicAIActionRequirement& Requirement,
		FStrategicAIBlackboard& Blackboard,
		AActor& SyntheticUnitActor,
		float& InOutGameTimeSeconds)
	{
		if (ConfigureControlledTypedUnitRequirement(Requirement, Blackboard, SyntheticUnitActor)
			|| ConfigureControlledCategoryUnitRequirement(Requirement, Blackboard, SyntheticUnitActor))
		{
			return;
		}

		ConfigureControlledNonUnitRequirement(
			Requirement,
			Blackboard,
			SyntheticUnitActor,
			InOutGameTimeSeconds);
	}

	class FEnemyAIShippingTestRunner final : public FTickableGameObject
	{
	public:
		explicit FEnemyAIShippingTestRunner(UWorld& InWorld)
			: M_World(&InWorld)
		{
		}

		virtual void Tick(float DeltaTime) override;
		virtual TStatId GetStatId() const override;
		virtual bool IsTickable() const override;
		virtual bool IsTickableWhenPaused() const override;

		void StartRun(const FString& Reason);
		bool GetIsForWorld(const UWorld* World) const;

	private:
		void AdvanceWaitForRuntime();
		void AdvanceObserveAbilityCooldowns();
		void AdvanceWaitForStrategicState();
		void AdvanceValidateTrainingBuckets();
		void AdvanceWaitForMissionActivity();
		void AdvanceGarbageCollectionStress();
		void AdvanceObserveStochasticTree();
		void SetPhase(EEnemyAIShippingTestPhase NewPhase);

		bool TryCacheRuntimeObjects();
		bool GetIsStartupStateReady() const;
		bool GetLiveIdleSquadAndTankCounts(int32& OutSquadCount, int32& OutTankCount) const;
		FString GetStartupReadinessDetails() const;
		void EnablePressureOnlyTrainingModeIfReady();
		bool GetIsStrategicStateReady() const;
		bool GetIsMissionActivityReady() const;
		void CollectRuntimeObservations();
		void CollectFormationObservations();
		void CollectStochasticActionObservations();

		void ValidateStartupState(bool bTimedOut = false);
		void ValidateIdleUnitBlackboard(const FString& TestPhase, bool bRequireSquadsAndTanks);
		void ValidateDecisionTreeLayout(bool bRecordExecutionBaseline);
		void GatherDecisionTreeLayout(
			const TArray<FStrategicAIAction>& ActionDefinitions,
			bool bRecordExecutionBaseline,
			FDecisionTreeLayoutSnapshot& OutLayoutSnapshot);
		void ValidateDecisionTreeLayoutSnapshot(
			const TArray<FStrategicAIAction>& ActionDefinitions,
			const FDecisionTreeLayoutSnapshot& LayoutSnapshot);
		void ValidateNativeRequirements(
			const UStrategicAISubAction& SubAction,
			const FExpectedSubActionDefinition& ExpectedDefinition);
		void ValidateNativeRequirementValue(
			const UStrategicAIActionRequirement& Requirement,
			const FExpectedNativeRequirementDefinition& ExpectedRequirement,
			ESubtypeAction SubtypeAction);
		void ValidateGlobalAbilityLoadout();
		void ValidateAbilityCooldownProgress();
		void ValidateStrategicBlackboard();
		void LogWorldStateDiagnostics() const;
		void ValidateRoadSplines(const FStrategicAIBlackboard& Blackboard);
		void ValidatePlayerUnitData(const FStrategicAIBlackboard& Blackboard);
		void ValidateEnemyBaseData(const FStrategicAIBlackboard& Blackboard);
		void ValidateGeneratedStrategicLocations(const FStrategicAIBlackboard& Blackboard);
		void ValidateTrainingPressureDeltas();
		void ValidateIndependentTrainingActionRouting(
			const UStochasticDecisionTree& DecisionTree,
			const FStrategicAIBlackboard& Blackboard);
		void ValidateIndependentTrainingActionRoutingCase(
			UStrategicAISubAction& SubAction,
			const FExpectedSubActionDefinition& ExpectedDefinition,
			const FStrategicAIBlackboard& Blackboard,
			AEnemyController& SyntheticUnitActor);
		void ValidateTrainingPointIncome();
		int32 CollectExpectedTrainingPressure(
			const UStochasticDecisionTree& DecisionTree,
			const FStrategicAIBlackboard& Blackboard,
			float GameTimeSeconds,
			TMap<EAITrainingFocus, float>& OutExpectedFocus,
			TMap<EAITrainingFocusSpecialty, float>& OutExpectedSpecialty);
		void ValidateMissionActivity();
		void ValidateNoUnexpectedTrainingSpawns();
		void ValidateAfterGarbageCollection();
		void ValidateFormationSnapshots();
		void ValidateStochasticExecution();
		void ValidateCooldownDelta(
			const FString& TestName,
			int32 InitialCooldown,
			int32 CurrentCooldown,
			double ElapsedSeconds);

		void AddExpectedTrainingContribution(
			ESubtypeAction SubtypeAction,
			const FEnemyStrategicTrainingPressureContribution& Contribution,
			TMap<EAITrainingFocus, float>& OutExpectedFocus,
			TMap<EAITrainingFocusSpecialty, float>& OutExpectedSpecialty);
		void ValidateTrainingContributionRouting(
			ESubtypeAction SubtypeAction,
			const FEnemyStrategicTrainingPressureContribution& Contribution,
			const FString& TestName);
		void ValidateControlledTrainingBucketApplication(
			const FExpectedSubActionDefinition& ExpectedDefinition,
			const FEnemyStrategicTrainingPressureContribution& Contribution);
		void CompareTrainingFocusDeltas(
			const TMap<EAITrainingFocus, float>& Before,
			const TMap<EAITrainingFocus, float>& After,
			const TMap<EAITrainingFocus, float>& Expected);
		void CompareTrainingSpecialtyDeltas(
			const TMap<EAITrainingFocusSpecialty, float>& Before,
			const TMap<EAITrainingFocusSpecialty, float>& After,
			const TMap<EAITrainingFocusSpecialty, float>& Expected);

		void RecordPass(const FString& TestName, const FString& Details = FString());
		void RecordFailure(const FString& TestName, const FString& Details);
		void RecordExpectation(bool bCondition, const FString& TestName, const FString& Details);
		void FinishRun();
		void WriteSummary() const;
		void ResetRunState();
		void ResumeTestWorldIfPaused() const;

		double GetWorldSeconds() const;
		double GetRealTimeSeconds() const;
		double GetRunElapsedSeconds() const;
		double GetPhaseElapsedSeconds() const;

		TWeakObjectPtr<UWorld> M_World;
		TWeakObjectPtr<AEnemyController> M_EnemyController;
		TWeakObjectPtr<UEnemyStrategicAIComponent> M_StrategicAIComponent;
		TWeakObjectPtr<UEnemyFormationController> M_FormationController;
		TWeakObjectPtr<UEnemyWaveController> M_WaveController;
		TWeakObjectPtr<UEnemyDirectControlComponent> M_DirectControlComponent;
		TWeakObjectPtr<UGlobalAbilitiesManager> M_GlobalAbilitiesManager;
		TMap<ESubtypeAction, float> M_InitialActionExecutionTimes;
		TSet<ESubtypeAction> M_ObservedExecutedActions;
		TSet<int32> M_SeenAttackMoveFormationIDs;
		TSet<int32> M_SeenPatrolFormationIDs;
		TArray<FString> M_ResultLines;
		FString M_RunReason;
		double M_RunStartedSeconds = 0.0;
		double M_PhaseStartedSeconds = 0.0;
		double M_CooldownBaselineSeconds = 0.0;
		double M_CooldownBaselineWorldSeconds = 0.0;
		int32 M_InitialT34CooldownRemaining = INDEX_NONE;
		int32 M_InitialCarpetCooldownRemaining = INDEX_NONE;
		int32 M_InitialTrainingSpawnRequestCount = 0;
		int32 M_InitialTrainingSpawnSuccessCount = 0;
		int32 M_RequestedGarbageCollectionCycles = DefaultGarbageCollectionCycles;
		int32 M_CompletedGarbageCollectionCycles = 0;
		int32 M_PassedTestCount = 0;
		int32 M_FailedTestCount = 0;
		EEnemyAIShippingTestPhase M_Phase = EEnemyAIShippingTestPhase::WaitForRuntime;
		bool bM_IsRunning = false;
		bool bM_ExitOnComplete = false;
		bool bM_PressureOnlyTrainingEnabled = false;
	};

	TArray<TUniquePtr<FEnemyAIShippingTestRunner>> GActiveRunners;
	int32 GTestMapTravelAttempts = 0;

	FEnemyAIShippingTestRunner* FindRunnerForWorld(const UWorld* World)
	{
		for (const TUniquePtr<FEnemyAIShippingTestRunner>& Runner : GActiveRunners)
		{
			if (Runner.IsValid() && Runner->GetIsForWorld(World))
			{
				return Runner.Get();
			}
		}

		return nullptr;
	}

	void StartRunnerForWorld(UWorld& World, const FString& Reason)
	{
		if (not World.IsGameWorld())
		{
			return;
		}

		FEnemyAIShippingTestRunner* ExistingRunner = FindRunnerForWorld(&World);
		if (ExistingRunner != nullptr)
		{
			ExistingRunner->StartRun(Reason);
			return;
		}

		TUniquePtr<FEnemyAIShippingTestRunner> NewRunner = MakeUnique<FEnemyAIShippingTestRunner>(World);
		NewRunner->StartRun(Reason);
		GActiveRunners.Add(MoveTemp(NewRunner));
	}

	void RemoveRunnerForWorld(const UWorld* World)
	{
		for (int32 RunnerIndex = GActiveRunners.Num() - 1; RunnerIndex >= 0; --RunnerIndex)
		{
			if (GActiveRunners[RunnerIndex].IsValid() && GActiveRunners[RunnerIndex]->GetIsForWorld(World))
			{
				GActiveRunners.RemoveAtSwap(RunnerIndex);
			}
		}
	}

	void ApplyDefaultRuntimeStateForTestMap(UWorld& World)
	{
		URTSGameInstance* GameInstance = World.GetGameInstance<URTSGameInstance>();
		if (not IsValid(GameInstance))
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Error,
			       TEXT("Cannot prepare Enemy AI test map: RTS game instance is invalid."));
			return;
		}

		FRTSGameDifficulty GameDifficulty;
		// Run at the highest difficulty so difficulty-gated mission content (e.g. the random-patrol wave,
		// created via CreateSingleRandomPatrolWithAttackMoveDifficultyConditional) is exercised.
		GameDifficulty.DifficultyLevel = ERTSGameDifficulty::Ironman;
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
			UE_LOG(LogRTSEnemyAIShippingTests, Error,
			       TEXT("RTS_ENEMY_AI_TEST_RESULT FAIL could not travel to %s after %d attempts."),
			       TestMapPackageName,
			       GTestMapTravelAttempts);
			if (FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete")))
			{
				FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("RTS Enemy AI Shipping Tests"));
			}
			return;
		}

		GTestMapTravelAttempts++;
		ApplyDefaultRuntimeStateForTestMap(World);
		TWeakObjectPtr<UWorld> WeakWorld = &World;
		World.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld]()
		{
			UWorld* TravelWorld = WeakWorld.Get();
			if (not IsValid(TravelWorld))
			{
				return;
			}

			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_TEST_TRAVEL map=%s attempt=%d"),
			       TestMapPackageName,
			       GTestMapTravelAttempts);
			UGameplayStatics::OpenLevel(TravelWorld, FName(TestMapPackageName));
		}));
	}

	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues)
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
		StartRunnerForWorld(*World, TEXT("command-line"));
	}

	void HandleWorldCleanup(UWorld* World, bool, bool)
	{
		RemoveRunnerForWorld(World);
	}

	void RunEnemyAITestsCommand(const TArray<FString>&, UWorld* World)
	{
		if (not IsValid(World))
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Error, TEXT("RTS.UnitTests.EnemyAI.Run failed: world is invalid."));
			return;
		}

		StartRunnerForWorld(*World, TEXT("console-command"));
	}

	FAutoConsoleCommandWithWorldAndArgs GRunEnemyAITestsCommand(
		TEXT("RTS.UnitTests.EnemyAI.Run"),
		TEXT("Runs the staged Enemy AI Shipping tests on UT_EnemyAI."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunEnemyAITestsCommand));

	struct FEnemyAIShippingTestRegistration
	{
		FDelegateHandle PostWorldInitializationHandle;
		FDelegateHandle WorldCleanupHandle;

		FEnemyAIShippingTestRegistration()
		{
			PostWorldInitializationHandle =
				FWorldDelegates::OnPostWorldInitialization.AddStatic(&HandlePostWorldInitialization);
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
		}

		~FEnemyAIShippingTestRegistration()
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}
	};

	FEnemyAIShippingTestRegistration GRegistration;

	void FEnemyAIShippingTestRunner::Tick(const float DeltaTime)
	{
		(void)DeltaTime;
		ResumeTestWorldIfPaused();
		CollectRuntimeObservations();

		switch (M_Phase)
		{
		case EEnemyAIShippingTestPhase::WaitForRuntime:
			AdvanceWaitForRuntime();
			break;
		case EEnemyAIShippingTestPhase::ObserveAbilityCooldowns:
			AdvanceObserveAbilityCooldowns();
			break;
		case EEnemyAIShippingTestPhase::WaitForStrategicState:
			AdvanceWaitForStrategicState();
			break;
		case EEnemyAIShippingTestPhase::ValidateTrainingBuckets:
			AdvanceValidateTrainingBuckets();
			break;
		case EEnemyAIShippingTestPhase::WaitForMissionActivity:
			AdvanceWaitForMissionActivity();
			break;
		case EEnemyAIShippingTestPhase::GarbageCollectionStress:
			AdvanceGarbageCollectionStress();
			break;
		case EEnemyAIShippingTestPhase::ObserveStochasticTree:
			AdvanceObserveStochasticTree();
			break;
		case EEnemyAIShippingTestPhase::Complete:
			break;
		}
	}

	TStatId FEnemyAIShippingTestRunner::GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FEnemyAIShippingTestRunner, STATGROUP_Tickables);
	}

	bool FEnemyAIShippingTestRunner::IsTickable() const
	{
		return bM_IsRunning && M_World.IsValid();
	}

	bool FEnemyAIShippingTestRunner::IsTickableWhenPaused() const
	{
		return true;
	}

	void FEnemyAIShippingTestRunner::StartRun(const FString& Reason)
	{
		if (bM_IsRunning)
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Warning, TEXT("Enemy AI Shipping tests are already running."));
			return;
		}

		UWorld* World = M_World.Get();
		if (not IsValid(World) || not GetIsCurrentMapSupported(*World))
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Error, TEXT("Enemy AI Shipping tests require UT_EnemyAI."));
			return;
		}

		ResetRunState();
		FParse::Value(
			FCommandLine::Get(),
			TEXT("RTSEnemyAITestGCCycles="),
			M_RequestedGarbageCollectionCycles);
		M_RequestedGarbageCollectionCycles = FMath::Max(1, M_RequestedGarbageCollectionCycles);
		bM_ExitOnComplete = FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete"));
		M_RunReason = Reason;
		ResumeTestWorldIfPaused();
		M_RunStartedSeconds = GetRealTimeSeconds();
		M_PhaseStartedSeconds = M_RunStartedSeconds;
		M_Phase = EEnemyAIShippingTestPhase::WaitForRuntime;
		bM_IsRunning = true;

		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_TEST_BEGIN reason=%s map=%s gc_cycles=%d"),
		       *M_RunReason,
		       *World->GetMapName(),
		       M_RequestedGarbageCollectionCycles);
	}

	bool FEnemyAIShippingTestRunner::GetIsForWorld(const UWorld* World) const
	{
		return M_World.Get() == World;
	}

	void FEnemyAIShippingTestRunner::AdvanceWaitForRuntime()
	{
		const bool bCoreRuntimeObjectsReady = TryCacheRuntimeObjects();
		EnablePressureOnlyTrainingModeIfReady();
		if (bCoreRuntimeObjectsReady && GetIsStartupStateReady())
		{
			ValidateStartupState();
			SetPhase(EEnemyAIShippingTestPhase::ObserveAbilityCooldowns);
			return;
		}

		if (GetRunElapsedSeconds() >= RuntimeReadyTimeoutSeconds)
		{
			ValidateStartupState(true);
			FinishRun();
		}
	}

	void FEnemyAIShippingTestRunner::AdvanceObserveAbilityCooldowns()
	{
		if (GetRealTimeSeconds() - M_CooldownBaselineSeconds < CooldownObservationSeconds)
		{
			return;
		}

		ValidateAbilityCooldownProgress();
		SetPhase(EEnemyAIShippingTestPhase::WaitForStrategicState);
	}

	void FEnemyAIShippingTestRunner::AdvanceWaitForStrategicState()
	{
		if (GetIsStrategicStateReady())
		{
			ValidateStrategicBlackboard();
			SetPhase(EEnemyAIShippingTestPhase::ValidateTrainingBuckets);
			return;
		}

		if (GetRunElapsedSeconds() < StrategicStateTimeoutSeconds)
		{
			return;
		}

		RecordFailure(
			TEXT("Blackboard.AsyncReadiness"),
			TEXT("Timed out waiting for player counts, bulks, base clusters, construction, and mine results."));
		ValidateStrategicBlackboard();
		SetPhase(EEnemyAIShippingTestPhase::ValidateTrainingBuckets);
	}

	void FEnemyAIShippingTestRunner::AdvanceValidateTrainingBuckets()
	{
		ValidateTrainingPressureDeltas();
		SetPhase(EEnemyAIShippingTestPhase::WaitForMissionActivity);
	}

	void FEnemyAIShippingTestRunner::AdvanceWaitForMissionActivity()
	{
		if (GetIsMissionActivityReady())
		{
			ValidateMissionActivity();
			SetPhase(EEnemyAIShippingTestPhase::GarbageCollectionStress);
			return;
		}

		if (GetPhaseElapsedSeconds() < MissionActivityTimeoutSeconds)
		{
			return;
		}

		RecordFailure(TEXT("Missions.ActivityReadiness"), TEXT("Timed out waiting for attack-move and patrol waves."));
		ValidateMissionActivity();
		SetPhase(EEnemyAIShippingTestPhase::GarbageCollectionStress);
	}

	void FEnemyAIShippingTestRunner::AdvanceGarbageCollectionStress()
	{
		if (M_CompletedGarbageCollectionCycles > 0
			&& GetPhaseElapsedSeconds() < GarbageCollectionSettleSeconds)
		{
			return;
		}

		if (M_CompletedGarbageCollectionCycles >= M_RequestedGarbageCollectionCycles)
		{
			ValidateAfterGarbageCollection();
			SetPhase(EEnemyAIShippingTestPhase::ObserveStochasticTree);
			return;
		}

		if (GEngine == nullptr)
		{
			RecordFailure(TEXT("GC.EngineAvailable"), TEXT("GEngine was null while requesting garbage collection."));
			M_CompletedGarbageCollectionCycles = M_RequestedGarbageCollectionCycles;
			return;
		}

		GEngine->ForceGarbageCollection(true);
		M_CompletedGarbageCollectionCycles++;
		M_PhaseStartedSeconds = GetRealTimeSeconds();
		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_TEST_GC cycle=%d/%d"),
		       M_CompletedGarbageCollectionCycles,
		       M_RequestedGarbageCollectionCycles);
	}

	void FEnemyAIShippingTestRunner::AdvanceObserveStochasticTree()
	{
		if (GetPhaseElapsedSeconds() < StochasticObservationSeconds)
		{
			return;
		}

		ValidateStochasticExecution();
		FinishRun();
	}

	void FEnemyAIShippingTestRunner::SetPhase(const EEnemyAIShippingTestPhase NewPhase)
	{
		M_Phase = NewPhase;
		M_PhaseStartedSeconds = GetRealTimeSeconds();
	}

	bool FEnemyAIShippingTestRunner::TryCacheRuntimeObjects()
	{
		UWorld* World = M_World.Get();
		if (not IsValid(World))
		{
			return false;
		}

		AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(World);
		if (not IsValid(EnemyController))
		{
			return false;
		}
		M_EnemyController = EnemyController;

		UEnemyStrategicAIComponent* StrategicAIComponent = EnemyController->GetEnemyStrategicAIComponent();
		UEnemyFormationController* FormationController = EnemyController->GetEnemyFormationController();
		UEnemyWaveController* WaveController = EnemyController->ShippingTest_GetEnemyWaveController();
		UEnemyDirectControlComponent* DirectControlComponent = EnemyController->GetEnemyDirectControlComponent();
		UGlobalAbilitiesManager* GlobalAbilitiesManager = EnemyController->GetEnemyGlobalAbilitiesManager();
		if (IsValid(StrategicAIComponent))
		{
			M_StrategicAIComponent = StrategicAIComponent;
		}
		if (IsValid(FormationController))
		{
			M_FormationController = FormationController;
		}
		if (IsValid(WaveController))
		{
			M_WaveController = WaveController;
		}
		if (IsValid(DirectControlComponent))
		{
			M_DirectControlComponent = DirectControlComponent;
		}
		if (IsValid(GlobalAbilitiesManager))
		{
			M_GlobalAbilitiesManager = GlobalAbilitiesManager;
		}

		if (not IsValid(StrategicAIComponent)
			|| not IsValid(FormationController)
			|| not IsValid(WaveController)
			|| not IsValid(DirectControlComponent)
			|| not IsValid(GlobalAbilitiesManager)
			|| not IsValid(EnemyController->GetStrategicDecisionTree()))
		{
			return false;
		}

		return true;
	}

	bool FEnemyAIShippingTestRunner::GetIsStartupStateReady() const
	{
		AEnemyController* EnemyController = M_EnemyController.Get();
		const UEnemyDirectControlComponent* DirectControlComponent = M_DirectControlComponent.Get();
		const UGlobalAbilitiesManager* GlobalAbilitiesManager = M_GlobalAbilitiesManager.Get();
		if (not IsValid(EnemyController)
			|| not IsValid(M_StrategicAIComponent.Get())
			|| not IsValid(M_FormationController.Get())
			|| not IsValid(M_WaveController.Get())
			|| not IsValid(DirectControlComponent)
			|| not IsValid(GlobalAbilitiesManager)
			|| not IsValid(EnemyController->GetStrategicDecisionTree()))
		{
			return false;
		}

		int32 IdleSquadCount = 0;
		int32 IdleTankCount = 0;
		GetLiveIdleSquadAndTankCounts(IdleSquadCount, IdleTankCount);
		return DirectControlComponent->ShippingTest_GetSuccessfulSquadRegistrationCount() > 0
			&& DirectControlComponent->ShippingTest_GetSuccessfulTankRegistrationCount() > 0
			&& IdleSquadCount > 0
			&& IdleTankCount > 0
			&& IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr))
			&& IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_SovietT34Drop));
	}

	bool FEnemyAIShippingTestRunner::GetLiveIdleSquadAndTankCounts(
		int32& OutSquadCount,
		int32& OutTankCount) const
	{
		OutSquadCount = 0;
		OutTankCount = 0;
		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			return false;
		}

		for (const FBlackboardIdleUnitEntry& IdleUnit :
			StrategicAIComponent->GetStrategicAIBlackboard().IdleDirectControlUnits)
		{
			if (not IdleUnit.IsValid())
			{
				continue;
			}

			OutSquadCount += IdleUnit.UnitType == EAllUnitType::UNType_Squad ? 1 : 0;
			OutTankCount += IdleUnit.UnitType == EAllUnitType::UNType_Tank ? 1 : 0;
		}

		return OutSquadCount > 0 && OutTankCount > 0;
	}

	FString FEnemyAIShippingTestRunner::GetStartupReadinessDetails() const
	{
		const UEnemyDirectControlComponent* DirectControlComponent = M_DirectControlComponent.Get();
		const UGlobalAbilitiesManager* GlobalAbilitiesManager = M_GlobalAbilitiesManager.Get();
		int32 IdleSquadCount = 0;
		int32 IdleTankCount = 0;
		GetLiveIdleSquadAndTankCounts(IdleSquadCount, IdleTankCount);
		const int32 RegisteredSquadCount = IsValid(DirectControlComponent)
			? DirectControlComponent->ShippingTest_GetSuccessfulSquadRegistrationCount()
			: 0;
		const int32 RegisteredTankCount = IsValid(DirectControlComponent)
			? DirectControlComponent->ShippingTest_GetSuccessfulTankRegistrationCount()
			: 0;
		const bool bCarpetLoaded = IsValid(GlobalAbilitiesManager)
			&& IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr));
		const bool bT34Loaded = IsValid(GlobalAbilitiesManager)
			&& IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_SovietT34Drop));
		return FString::Printf(
			TEXT("controller=%d strategic=%d formation=%d wave=%d direct=%d abilities=%d tree=%d "
				"registered_squads=%d registered_tanks=%d idle_squads=%d idle_tanks=%d carpet_loaded=%d t34_loaded=%d"),
			M_EnemyController.IsValid(),
			M_StrategicAIComponent.IsValid(),
			M_FormationController.IsValid(),
			M_WaveController.IsValid(),
			M_DirectControlComponent.IsValid(),
			M_GlobalAbilitiesManager.IsValid(),
			M_EnemyController.IsValid() && IsValid(M_EnemyController->GetStrategicDecisionTree()),
			RegisteredSquadCount,
			RegisteredTankCount,
			IdleSquadCount,
			IdleTankCount,
			bCarpetLoaded,
			bT34Loaded);
	}

	void FEnemyAIShippingTestRunner::EnablePressureOnlyTrainingModeIfReady()
	{
		UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (bM_PressureOnlyTrainingEnabled || not IsValid(StrategicAIComponent))
		{
			return;
		}

		StrategicAIComponent->ShippingTest_EnablePressureOnlyTrainingMode();
		M_InitialTrainingSpawnRequestCount = StrategicAIComponent->ShippingTest_GetTrainingSpawnRequestCount();
		M_InitialTrainingSpawnSuccessCount = StrategicAIComponent->ShippingTest_GetTrainingSpawnSuccessCount();
		bM_PressureOnlyTrainingEnabled = true;
	}

	bool FEnemyAIShippingTestRunner::GetIsStrategicStateReady() const
	{
		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			return false;
		}

		const FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetStrategicAIBlackboard();
		const FResultPlayerUnitCounts& PlayerCounts = Blackboard.CurrentPlayerUnitCounts;
		return GetIsUsableStrategicLocation(PlayerCounts.PlayerHQLocation)
			&& not PlayerCounts.PlayerResourceBuildings.IsEmpty()
			&& Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.Num() >= ExpectedPlayerUnitBulkCount
			&& not Blackboard.EnemyBasePoints.IsEmpty()
			&& not Blackboard.CurrentConstructionLocations.ConstructionLocations.IsEmpty()
			&& not Blackboard.CurrentMineLocations.RoadMineLocations.IsEmpty()
			&& not Blackboard.CurrentMineLocations.DefenseArcMineLocations.IsEmpty();
	}

	bool FEnemyAIShippingTestRunner::GetIsMissionActivityReady() const
	{
		const UEnemyWaveController* WaveController = M_WaveController.Get();
		return IsValid(WaveController)
			&& WaveController->ShippingTest_GetCompletedAttackMoveWaveCount() >= MinimumCompletedAttackMoveWaves
			&& WaveController->ShippingTest_GetCompletedPatrolWaveCount() >= MinimumCompletedPatrolWaves;
	}

	void FEnemyAIShippingTestRunner::CollectRuntimeObservations()
	{
		if (not M_EnemyController.IsValid())
		{
			return;
		}

		CollectFormationObservations();
		CollectStochasticActionObservations();
	}

	void FEnemyAIShippingTestRunner::CollectFormationObservations()
	{
		UEnemyFormationController* FormationController = M_FormationController.Get();
		if (not IsValid(FormationController))
		{
			return;
		}

		TArray<FFormationData> Formations;
		FormationController->GetActiveFormationData(Formations);
		for (const FFormationData& Formation : Formations)
		{
			if (Formation.bIsRandomPatrolWithAttackMoveFormation)
			{
				M_SeenPatrolFormationIDs.Add(Formation.FormationID);
			}
			else if (Formation.bIsAttackMoveFormation)
			{
				M_SeenAttackMoveFormationIDs.Add(Formation.FormationID);
			}
		}

		// The persistent progress history also records formations that were created and finished between
		// observation ticks (e.g. the single random-patrol formation), so brief formations are not missed.
		for (const FEnemyAIShippingFormationProgress& Progress :
			FormationController->ShippingTest_GetFormationProgressHistory())
		{
			if (Progress.FormationKind == EEnemyAIShippingFormationKind::RandomPatrolWithAttackMove)
			{
				M_SeenPatrolFormationIDs.Add(Progress.FormationID);
			}
			else if (Progress.FormationKind == EEnemyAIShippingFormationKind::AttackMove)
			{
				M_SeenAttackMoveFormationIDs.Add(Progress.FormationID);
			}
		}
	}

	void FEnemyAIShippingTestRunner::CollectStochasticActionObservations()
	{
		AEnemyController* EnemyController = M_EnemyController.Get();
		if (not IsValid(EnemyController))
		{
			return;
		}

		UStochasticDecisionTree* DecisionTree = EnemyController->GetStrategicDecisionTree();
		if (not IsValid(DecisionTree))
		{
			return;
		}

		for (const FStrategicAIAction& Action : DecisionTree->GetActionDefinitions())
		{
			for (UStrategicAISubAction* SubAction : Action.GetSubActions())
			{
				if (not IsValid(SubAction))
				{
					continue;
				}

				const float InitialTime = M_InitialActionExecutionTimes.FindRef(SubAction->SubtypeAction);
				if (SubAction->GetLastTimeActionExecuted() > InitialTime + KINDA_SMALL_NUMBER)
				{
					M_ObservedExecutedActions.Add(SubAction->SubtypeAction);
				}
			}
		}
	}

	void FEnemyAIShippingTestRunner::ValidateStartupState(const bool bTimedOut)
	{
		UEnemyDirectControlComponent* DirectControlComponent = M_DirectControlComponent.Get();
		if (not IsValid(DirectControlComponent))
		{
			RecordFailure(
				TEXT("Startup.RuntimeReady"),
				FString::Printf(
					TEXT("%s%s"),
					bTimedOut ? TEXT("Timed out waiting for Enemy AI startup state. ") : TEXT(""),
					*GetStartupReadinessDetails()));
			return;
		}

		EnablePressureOnlyTrainingModeIfReady();
		const FString ReadinessDetails = GetStartupReadinessDetails();
		RecordExpectation(
			GetIsStartupStateReady(),
			TEXT("Startup.RuntimeReady"),
			bTimedOut
				? FString(TEXT("Timed out waiting for Enemy AI startup state. ")) + ReadinessDetails
				: ReadinessDetails);
		RecordExpectation(
			DirectControlComponent->ShippingTest_GetSuccessfulRegistrationCount() > 0,
			TEXT("Startup.EnemyUnitsRegistered"),
			FString::Printf(
				TEXT("successful=%d"),
				DirectControlComponent->ShippingTest_GetSuccessfulRegistrationCount()));
		RecordExpectation(
			DirectControlComponent->ShippingTest_GetSuccessfulSquadRegistrationCount() > 0,
			TEXT("Startup.EnemySquadsRegistered"),
			FString::Printf(TEXT("squads=%d"), DirectControlComponent->ShippingTest_GetSuccessfulSquadRegistrationCount()));
		RecordExpectation(
			DirectControlComponent->ShippingTest_GetSuccessfulTankRegistrationCount() > 0,
			TEXT("Startup.EnemyTanksRegistered"),
			FString::Printf(TEXT("tanks=%d"), DirectControlComponent->ShippingTest_GetSuccessfulTankRegistrationCount()));
		const ACPPController* PlayerController = FRTS_Statics::GetRTSController(M_World.Get());
		RecordExpectation(
			IsValid(PlayerController) && not PlayerController->GetIsGameOnPause(),
			TEXT("Startup.GameStartedAndUnpaused"),
			FString::Printf(
				TEXT("has_controller=%d paused=%d"),
				IsValid(PlayerController),
				IsValid(PlayerController) ? PlayerController->GetIsGameOnPause() : 1));
		ValidateIdleUnitBlackboard(TEXT("Startup"), true);
		ValidateDecisionTreeLayout(true);
		ValidateGlobalAbilityLoadout();
	}

	void FEnemyAIShippingTestRunner::ValidateIdleUnitBlackboard(
		const FString& TestPhase,
		const bool bRequireSquadsAndTanks)
	{
		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TestPhase + TEXT(".IdleUnits.RuntimeValid"), TEXT("Strategic AI component is invalid."));
			return;
		}

		const TArray<FBlackboardIdleUnitEntry>& IdleUnits =
			StrategicAIComponent->GetStrategicAIBlackboard().IdleDirectControlUnits;
		TSet<const AActor*> UniqueIdleActors;
		int32 InvalidActorCount = 0;
		int32 DuplicateActorCount = 0;
		int32 CachedTypeMismatchCount = 0;
		int32 SquadCount = 0;
		int32 TankCount = 0;
		for (const FBlackboardIdleUnitEntry& IdleUnit : IdleUnits)
		{
			AActor* IdleActor = IdleUnit.Get();
			if (not IsValid(IdleActor))
			{
				InvalidActorCount++;
				continue;
			}

			DuplicateActorCount += UniqueIdleActors.Contains(IdleActor) ? 1 : 0;
			UniqueIdleActors.Add(IdleActor);
			const URTSComponent* RTSComponent = IdleActor->FindComponentByClass<URTSComponent>();
			if (not IsValid(RTSComponent)
				|| RTSComponent->GetUnitType() != IdleUnit.UnitType
				|| static_cast<int32>(RTSComponent->GetUnitSubType()) != IdleUnit.UnitSubtypeRaw)
			{
				CachedTypeMismatchCount++;
				continue;
			}

			SquadCount += IdleUnit.UnitType == EAllUnitType::UNType_Squad ? 1 : 0;
			TankCount += IdleUnit.UnitType == EAllUnitType::UNType_Tank ? 1 : 0;
		}

		RecordExpectation(
			InvalidActorCount == 0 && DuplicateActorCount == 0 && CachedTypeMismatchCount == 0,
			TestPhase + TEXT(".IdleUnits.ValidUniqueAndTyped"),
			FString::Printf(
				TEXT("entries=%d unique=%d invalid=%d duplicates=%d type_mismatches=%d"),
				IdleUnits.Num(),
				UniqueIdleActors.Num(),
				InvalidActorCount,
				DuplicateActorCount,
				CachedTypeMismatchCount));
		if (bRequireSquadsAndTanks)
		{
			RecordExpectation(
				SquadCount > 0 && TankCount > 0,
				TestPhase + TEXT(".IdleUnits.ExpectedPlacedTypes"),
				FString::Printf(TEXT("squads=%d tanks=%d"), SquadCount, TankCount));
		}
	}

	void FEnemyAIShippingTestRunner::ValidateDecisionTreeLayout(const bool bRecordExecutionBaseline)
	{
		AEnemyController* EnemyController = M_EnemyController.Get();
		if (not IsValid(EnemyController))
		{
			RecordFailure(TEXT("Tree.Valid"), TEXT("Enemy controller is invalid."));
			return;
		}

		UStochasticDecisionTree* DecisionTree = EnemyController->GetStrategicDecisionTree();
		if (not IsValid(DecisionTree))
		{
			RecordFailure(TEXT("Tree.Valid"), TEXT("Stochastic decision tree is invalid."));
			return;
		}

		const TArray<FStrategicAIAction>& ActionDefinitions = DecisionTree->GetActionDefinitions();
		FDecisionTreeLayoutSnapshot LayoutSnapshot;
		GatherDecisionTreeLayout(ActionDefinitions, bRecordExecutionBaseline, LayoutSnapshot);
		ValidateDecisionTreeLayoutSnapshot(ActionDefinitions, LayoutSnapshot);
	}

	void FEnemyAIShippingTestRunner::GatherDecisionTreeLayout(
		const TArray<FStrategicAIAction>& ActionDefinitions,
		const bool bRecordExecutionBaseline,
		FDecisionTreeLayoutSnapshot& OutLayoutSnapshot)
	{
		for (const FStrategicAIAction& Action : ActionDefinitions)
		{
			OutLayoutSnapshot.TopLevelActionCounts.FindOrAdd(Action.GetActionEnum())++;
			if (Action.GetScore() <= 0.f || Action.GetSubActions().IsEmpty())
			{
				OutLayoutSnapshot.InvalidObjectCount++;
			}

			for (UStrategicAISubAction* SubAction : Action.GetSubActions())
			{
				if (not IsValid(SubAction))
				{
					OutLayoutSnapshot.InvalidObjectCount++;
					continue;
				}

				OutLayoutSnapshot.SubActionCount++;
				if (OutLayoutSnapshot.SubActionMembership.Contains(SubAction->SubtypeAction))
				{
					OutLayoutSnapshot.DuplicateSubActionCount++;
				}
				else
				{
					OutLayoutSnapshot.SubActionMembership.Add(SubAction->SubtypeAction, Action.GetActionEnum());
				}
				OutLayoutSnapshot.UnexpectedSubActionCount +=
					FindExpectedSubActionDefinition(SubAction->SubtypeAction) == nullptr ? 1 : 0;
				if (bRecordExecutionBaseline)
				{
					M_InitialActionExecutionTimes.FindOrAdd(SubAction->SubtypeAction) =
						SubAction->GetLastTimeActionExecuted();
				}

				for (UStrategicAIActionRequirement* Requirement : SubAction->GetRequirements())
				{
					OutLayoutSnapshot.InvalidObjectCount += IsValid(Requirement) ? 0 : 1;
				}
				for (UStrategicAIActionRequirement* Requirement : SubAction->GetNativeVisibleRequirements())
				{
					OutLayoutSnapshot.InvalidObjectCount += IsValid(Requirement) ? 0 : 1;
				}

				UE_LOG(LogRTSEnemyAIShippingTests, Display,
				       TEXT("RTS_ENEMY_AI_TREE_ACTION subtype=%s debug=%s"),
				       *UEnum::GetValueAsString(SubAction->SubtypeAction),
				       *SubAction->GetDebugString());
			}
		}
	}

	void FEnemyAIShippingTestRunner::ValidateDecisionTreeLayoutSnapshot(
		const TArray<FStrategicAIAction>& ActionDefinitions,
		const FDecisionTreeLayoutSnapshot& LayoutSnapshot)
	{
		const TArray<FExpectedSubActionDefinition>& ExpectedDefinitions = GetExpectedSubActionDefinitions();
		// UT_EnemyAI authors exactly one top-level action (Attack) that owns every sub-action; there is no
		// separate Defend branch. The stochastic tree still routes among the sub-actions under this action.
		const int32 ExpectedTopLevelActionCount = 1;
		RecordExpectation(
			ActionDefinitions.Num() == ExpectedTopLevelActionCount,
			TEXT("Tree.TopLevelActionCount"),
			FString::Printf(TEXT("expected=%d actual=%d"), ExpectedTopLevelActionCount, ActionDefinitions.Num()));
		RecordExpectation(
			LayoutSnapshot.TopLevelActionCounts.FindRef(EStrategicAITopLevelAction::Attack) == 1
				&& LayoutSnapshot.TopLevelActionCounts.FindRef(EStrategicAITopLevelAction::Defend) == 0
				&& LayoutSnapshot.TopLevelActionCounts.Num() == ExpectedTopLevelActionCount,
			TEXT("Tree.TopLevelMembership"),
			FString::Printf(
				TEXT("attack=%d defend=%d unique=%d"),
				LayoutSnapshot.TopLevelActionCounts.FindRef(EStrategicAITopLevelAction::Attack),
				LayoutSnapshot.TopLevelActionCounts.FindRef(EStrategicAITopLevelAction::Defend),
				LayoutSnapshot.TopLevelActionCounts.Num()));
		RecordExpectation(
			LayoutSnapshot.SubActionCount == ExpectedDefinitions.Num(),
			TEXT("Tree.SubActionCount"),
			FString::Printf(TEXT("expected=%d actual=%d"), ExpectedDefinitions.Num(), LayoutSnapshot.SubActionCount));
		RecordExpectation(
			LayoutSnapshot.DuplicateSubActionCount == 0,
			TEXT("Tree.NoDuplicateSubActions"),
			FString::Printf(TEXT("duplicates=%d"), LayoutSnapshot.DuplicateSubActionCount));
		RecordExpectation(
			LayoutSnapshot.UnexpectedSubActionCount == 0,
			TEXT("Tree.NoUnexpectedSubActions"),
			FString::Printf(TEXT("unexpected=%d"), LayoutSnapshot.UnexpectedSubActionCount));
		RecordExpectation(
			LayoutSnapshot.InvalidObjectCount == 0,
			TEXT("Tree.OwnedObjectsValid"),
			FString::Printf(TEXT("invalid_or_empty=%d"), LayoutSnapshot.InvalidObjectCount));
		for (const FExpectedSubActionDefinition& ExpectedDefinition : ExpectedDefinitions)
		{
			const EStrategicAITopLevelAction* ActualTopLevel =
				LayoutSnapshot.SubActionMembership.Find(ExpectedDefinition.SubtypeAction);
			RecordExpectation(
				ActualTopLevel != nullptr && *ActualTopLevel == ExpectedDefinition.TopLevelAction,
				FString(TEXT("Tree.ExpectedAction.")) + UEnum::GetValueAsString(ExpectedDefinition.SubtypeAction),
				FString::Printf(
					TEXT("expected_top_level=%s actual_top_level=%s"),
					*UEnum::GetValueAsString(ExpectedDefinition.TopLevelAction),
					ActualTopLevel == nullptr
						? TEXT("Missing")
						: *UEnum::GetValueAsString(*ActualTopLevel)));

			UStrategicAISubAction* SubAction = FindSubActionByType(
				ActionDefinitions,
				ExpectedDefinition.SubtypeAction);
			if (IsValid(SubAction))
			{
				ValidateNativeRequirements(*SubAction, ExpectedDefinition);
			}
		}
	}

	void FEnemyAIShippingTestRunner::ValidateNativeRequirements(
		const UStrategicAISubAction& SubAction,
		const FExpectedSubActionDefinition& ExpectedDefinition)
	{
		TMap<const UClass*, int32> ExpectedClassCounts;
		for (const FExpectedNativeRequirementDefinition& ExpectedRequirement : ExpectedDefinition.NativeRequirements)
		{
			ExpectedClassCounts.FindOrAdd(ExpectedRequirement.RequirementClass)++;
		}

		TMap<const UClass*, int32> ActualClassCounts;
		for (const UStrategicAIActionRequirement* Requirement : SubAction.GetNativeVisibleRequirements())
		{
			if (IsValid(Requirement))
			{
				ActualClassCounts.FindOrAdd(Requirement->GetClass())++;
			}
		}

		bool bClassMultisetMatches =
			SubAction.GetNativeVisibleRequirements().Num() == ExpectedDefinition.NativeRequirements.Num()
			&& ActualClassCounts.Num() == ExpectedClassCounts.Num();
		for (const TPair<const UClass*, int32>& ExpectedClassCount : ExpectedClassCounts)
		{
			bClassMultisetMatches = bClassMultisetMatches
				&& ActualClassCounts.FindRef(ExpectedClassCount.Key) == ExpectedClassCount.Value;
		}

		const FString SubtypeName = UEnum::GetValueAsString(ExpectedDefinition.SubtypeAction);
		RecordExpectation(
			bClassMultisetMatches,
			FString(TEXT("Tree.NativeRequirements.ClassMultiset.")) + SubtypeName,
			FString::Printf(
				TEXT("expected_total=%d expected_classes=%d actual_total=%d actual_classes=%d"),
				ExpectedDefinition.NativeRequirements.Num(),
				ExpectedClassCounts.Num(),
				SubAction.GetNativeVisibleRequirements().Num(),
				ActualClassCounts.Num()));

		for (const FExpectedNativeRequirementDefinition& ExpectedRequirement : ExpectedDefinition.NativeRequirements)
		{
			const TObjectPtr<UStrategicAIActionRequirement>* ActualRequirement =
				SubAction.GetNativeVisibleRequirements().FindByPredicate(
					[&ExpectedRequirement](const TObjectPtr<UStrategicAIActionRequirement>& Requirement)
					{
						return IsValid(Requirement) && Requirement->GetClass() == ExpectedRequirement.RequirementClass;
					});
			if (ActualRequirement != nullptr && IsValid(ActualRequirement->Get()))
			{
				ValidateNativeRequirementValue(
					*ActualRequirement->Get(),
					ExpectedRequirement,
					ExpectedDefinition.SubtypeAction);
			}
		}
	}

	void FEnemyAIShippingTestRunner::ValidateNativeRequirementValue(
		const UStrategicAIActionRequirement& Requirement,
		const FExpectedNativeRequirementDefinition& ExpectedRequirement,
		const ESubtypeAction SubtypeAction)
	{
		const FString TestSuffix = UEnum::GetValueAsString(SubtypeAction)
			+ TEXT(".")
			+ GetNameSafe(ExpectedRequirement.RequirementClass);
		if (const UStrategicAIHasEnoughLightTanks* LightTankRequirement =
			Cast<UStrategicAIHasEnoughLightTanks>(&Requirement))
		{
			RecordExpectation(
				LightTankRequirement->AmountIdleLightTanksNeeded == ExpectedRequirement.RequiredAmount,
				FString(TEXT("Tree.NativeRequirements.Amount.")) + TestSuffix,
				FString::Printf(
					TEXT("expected=%d actual=%d"),
					ExpectedRequirement.RequiredAmount,
					LightTankRequirement->AmountIdleLightTanksNeeded));
			return;
		}

		const UStrategicAIHasAtLeastAnyIdleTanks* AnyTankRequirement =
			Cast<UStrategicAIHasAtLeastAnyIdleTanks>(&Requirement);
		if (not IsValid(AnyTankRequirement))
		{
			return;
		}

		RecordExpectation(
			AnyTankRequirement->AmountIdleTanksNeeded == ExpectedRequirement.RequiredAmount,
			FString(TEXT("Tree.NativeRequirements.Amount.")) + TestSuffix,
			FString::Printf(
				TEXT("expected=%d actual=%d"),
				ExpectedRequirement.RequiredAmount,
				AnyTankRequirement->AmountIdleTanksNeeded));
		RecordExpectation(
			AnyTankRequirement->MissingUnitFocusPressure == ExpectedRequirement.MissingUnitFocusPressure,
			FString(TEXT("Tree.NativeRequirements.MissingUnitFocus.")) + TestSuffix,
			FString::Printf(
				TEXT("expected=%s actual=%s"),
				*UEnum::GetValueAsString(ExpectedRequirement.MissingUnitFocusPressure),
				*UEnum::GetValueAsString(AnyTankRequirement->MissingUnitFocusPressure)));
	}

	void FEnemyAIShippingTestRunner::ValidateGlobalAbilityLoadout()
	{
		AEnemyController* EnemyController = M_EnemyController.Get();
		UGlobalAbilitiesManager* GlobalAbilitiesManager = M_GlobalAbilitiesManager.Get();
		if (not IsValid(EnemyController) || not IsValid(GlobalAbilitiesManager))
		{
			RecordFailure(TEXT("Abilities.RuntimeValid"), TEXT("Enemy controller or global abilities manager is invalid."));
			return;
		}

		const TArray<FEnemyGlobalAbilityLoadoutEntry>& Loadout =
			EnemyController->ShippingTest_GetEnemyGlobalAbilityLoadout();
		const auto FindEntry = [&Loadout](const EGlobalAbility AbilityType)
		{
			return Loadout.FindByPredicate([AbilityType](const FEnemyGlobalAbilityLoadoutEntry& Entry)
			{
				return Entry.AbilityType == AbilityType;
			});
		};

		const FEnemyGlobalAbilityLoadoutEntry* CarpetEntry = FindEntry(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr);
		const FEnemyGlobalAbilityLoadoutEntry* T34Entry = FindEntry(EGlobalAbility::GA_SovietT34Drop);
		RecordExpectation(
			Loadout.Num() == 2 && CarpetEntry != nullptr && T34Entry != nullptr,
			TEXT("Abilities.ConfiguredLoadout"),
			FString::Printf(TEXT("entries=%d"), Loadout.Num()));
		if (CarpetEntry == nullptr || T34Entry == nullptr)
		{
			return;
		}

		RecordExpectation(
			CarpetEntry->bStartOnCooldown && CarpetEntry->CooldownTimeOverride == 10,
			TEXT("Abilities.CarpetConfiguration"),
			FString::Printf(TEXT("start=%d cooldown=%d"), CarpetEntry->bStartOnCooldown, CarpetEntry->CooldownTimeOverride));
		RecordExpectation(
			T34Entry->bStartOnCooldown && T34Entry->CooldownTimeOverride == 8,
			TEXT("Abilities.T34Configuration"),
			FString::Printf(TEXT("start=%d cooldown=%d"), T34Entry->bStartOnCooldown, T34Entry->CooldownTimeOverride));

		UGlobalAbility* CarpetAbility =
			GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr);
		UGlobalAbility* T34Ability =
			GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_SovietT34Drop);
		RecordExpectation(
			IsValid(CarpetAbility) && IsValid(T34Ability),
			TEXT("Abilities.LoadedObjects"),
			TEXT("Both configured ability UObjects must be owned by the manager."));
		if (not IsValid(CarpetAbility) || not IsValid(T34Ability))
		{
			return;
		}

		M_InitialCarpetCooldownRemaining = CarpetAbility->GetAbilityCosts().CoolDownRemaining;
		M_InitialT34CooldownRemaining = T34Ability->GetAbilityCosts().CoolDownRemaining;
		M_CooldownBaselineSeconds = GetRealTimeSeconds();
		M_CooldownBaselineWorldSeconds = GetWorldSeconds();
		RecordExpectation(
			CarpetAbility->GetAbilityCosts().CoolDownTime == 10 && M_InitialCarpetCooldownRemaining > 0,
			TEXT("Abilities.CarpetInitialCooldown"),
			FString::Printf(
				TEXT("time=%d remaining=%d"),
				CarpetAbility->GetAbilityCosts().CoolDownTime,
				M_InitialCarpetCooldownRemaining));
		RecordExpectation(
			T34Ability->GetAbilityCosts().CoolDownTime == 8 && M_InitialT34CooldownRemaining > 0,
			TEXT("Abilities.T34InitialCooldown"),
			FString::Printf(
				TEXT("time=%d remaining=%d"),
				T34Ability->GetAbilityCosts().CoolDownTime,
				M_InitialT34CooldownRemaining));
	}

	void FEnemyAIShippingTestRunner::ValidateAbilityCooldownProgress()
	{
		UGlobalAbilitiesManager* GlobalAbilitiesManager = M_GlobalAbilitiesManager.Get();
		if (not IsValid(GlobalAbilitiesManager))
		{
			RecordFailure(TEXT("Abilities.CooldownRuntimeValid"), TEXT("Global abilities manager became invalid."));
			return;
		}

		UGlobalAbility* CarpetAbility =
			GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr);
		UGlobalAbility* T34Ability =
			GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_SovietT34Drop);
		if (not IsValid(CarpetAbility) || not IsValid(T34Ability))
		{
			RecordFailure(TEXT("Abilities.CooldownObjectsSurvived"), TEXT("Ability UObject became invalid during cooldown observation."));
			return;
		}

		const int32 CarpetRemaining = CarpetAbility->GetAbilityCosts().CoolDownRemaining;
		const int32 T34Remaining = T34Ability->GetAbilityCosts().CoolDownRemaining;
		const double ElapsedSeconds = GetWorldSeconds() - M_CooldownBaselineWorldSeconds;
		ValidateCooldownDelta(
			TEXT("Abilities.CarpetCooldownUsesSeconds"),
			M_InitialCarpetCooldownRemaining,
			CarpetRemaining,
			ElapsedSeconds);
		ValidateCooldownDelta(
			TEXT("Abilities.T34CooldownUsesSeconds"),
			M_InitialT34CooldownRemaining,
			T34Remaining,
			ElapsedSeconds);
	}

	void FEnemyAIShippingTestRunner::ValidateCooldownDelta(
		const FString& TestName,
		const int32 InitialCooldown,
		const int32 CurrentCooldown,
		const double ElapsedSeconds)
	{
		const int32 MaximumPossibleDecrease = FMath::Max(0, InitialCooldown);
		const int32 MinimumExpectedDecrease = FMath::Min(
			MaximumPossibleDecrease,
			FMath::Max(1, FMath::FloorToInt(ElapsedSeconds) - 1));
		const int32 MaximumExpectedDecrease = FMath::Min(
			MaximumPossibleDecrease,
			FMath::Max(MinimumExpectedDecrease, FMath::CeilToInt(ElapsedSeconds) + 1));
		const int32 ActualDecrease = InitialCooldown - CurrentCooldown;
		const bool bCooldownDeltaIsBounded = InitialCooldown != INDEX_NONE
			&& CurrentCooldown >= 0
			&& CurrentCooldown <= InitialCooldown
			&& ActualDecrease >= MinimumExpectedDecrease
			&& ActualDecrease <= MaximumExpectedDecrease;
		RecordExpectation(
			bCooldownDeltaIsBounded,
			TestName,
			FString::Printf(
				TEXT("initial=%d current=%d decrease=%d expected=[%d,%d] elapsed=%.2f"),
				InitialCooldown,
				CurrentCooldown,
				ActualDecrease,
				MinimumExpectedDecrease,
				MaximumExpectedDecrease,
				ElapsedSeconds));
	}

	void FEnemyAIShippingTestRunner::ValidateStrategicBlackboard()
	{
		UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TEXT("Blackboard.ComponentValid"), TEXT("Strategic AI component is invalid."));
			return;
		}

		const FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetStrategicAIBlackboard();
		LogWorldStateDiagnostics();
		ValidateRoadSplines(Blackboard);
		ValidatePlayerUnitData(Blackboard);
		ValidateEnemyBaseData(Blackboard);
		ValidateGeneratedStrategicLocations(Blackboard);
		RecordExpectation(
			Blackboard.TrainerComponents.IsEmpty(),
			TEXT("Training.NoTrainerComponentsExpected"),
			FString::Printf(TEXT("trainers=%d"), Blackboard.TrainerComponents.Num()));
	}

	void FEnemyAIShippingTestRunner::LogWorldStateDiagnostics() const
	{
		UWorld* World = M_World.Get();
		if (not IsValid(World))
		{
			return;
		}

		TArray<AActor*> RoadSplineActorsInWorld;
		UGameplayStatics::GetAllActorsOfClass(World, ARoadSplineActor::StaticClass(), RoadSplineActorsInWorld);
		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_DIAG road_spline_actors_in_world=%d"),
		       RoadSplineActorsInWorld.Num());

		if (const URTSGameInstance* GameInstance = World->GetGameInstance<URTSGameInstance>())
		{
			const FRTSGameDifficulty Difficulty = GameInstance->GetSelectedGameDifficulty();
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_DIAG difficulty=%s percentage=%d initialized=%d"),
			       *UEnum::GetValueAsString(Difficulty.DifficultyLevel),
			       Difficulty.DifficultyPercentage,
			       Difficulty.bIsInitialized);
		}

		if (const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(World))
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_DIAG game_unit_manager_player_hq_valid=%d"),
			       IsValid(GameUnitManager->GetPlayerHQ()));
		}

		if (const AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(World))
		{
			const FRTSGameDifficulty MissionDifficulty = MissionManager->GetCurrentGameDifficulty();
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_DIAG mission_manager_difficulty=%s initialized=%d"),
			       *UEnum::GetValueAsString(MissionDifficulty.DifficultyLevel),
			       MissionDifficulty.bIsInitialized);
		}

		if (const UEnemyStrategicAIComponent* StrategicForClusters = M_StrategicAIComponent.Get())
		{
			const FFindEnemyBaseClusters& ClusterRequest = StrategicForClusters->FindEnemyBase_TimerRequest;
			FString CoreTypeList;
			for (const EBuildingExpansionType CoreType : ClusterRequest.CoreBuildingTypes)
			{
				CoreTypeList += FString::Printf(TEXT("%d,"), static_cast<int32>(CoreType));
			}
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_DIAG cluster_request core_types=[%s] core_type_count=%d dist=%.0f "
				       "min_neighbors=%d min_buildings=%d max_bases=%d min_score=%.0f"),
			       *CoreTypeList,
			       ClusterRequest.CoreBuildingTypes.Num(),
			       ClusterRequest.CoreClusterDistanceXY,
			       ClusterRequest.MinCoreNeighbors,
			       ClusterRequest.MinTotalBuildingsPerBase,
			       ClusterRequest.MaxBasesToReturn,
			       ClusterRequest.MinBaseScoreToReturn);
		}

		// Summarise every RTS-tracked actor by owner + type + subtype. The player HQ (a nomadic of HQ
		// subtype) and enemy base clustering both read this registered state, so this reveals whether the
		// map's HQ/enemy buildings are actually present and correctly typed for the AI to find.
		TMap<FString, int32> UnitStateHistogram;
		int32 TrackedActorCount = 0;
		for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
		{
			AActor* Actor = *ActorIterator;
			const URTSComponent* RTSComponent = IsValid(Actor)
				? Actor->FindComponentByClass<URTSComponent>()
				: nullptr;
			if (not IsValid(RTSComponent))
			{
				continue;
			}

			TrackedActorCount++;
			const FString Key = FString::Printf(
				TEXT("owner=%d type=%s subtype=%d"),
				RTSComponent->GetOwningPlayer(),
				*UEnum::GetValueAsString(RTSComponent->GetUnitType()),
				static_cast<int32>(RTSComponent->GetUnitSubType()));
			UnitStateHistogram.FindOrAdd(Key)++;
		}

		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_DIAG rts_tracked_actors=%d distinct_states=%d"),
		       TrackedActorCount,
		       UnitStateHistogram.Num());
		for (const TPair<FString, int32>& HistogramEntry : UnitStateHistogram)
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_DIAG unit_state %s count=%d"),
			       *HistogramEntry.Key,
			       HistogramEntry.Value);
		}
	}

	void FEnemyAIShippingTestRunner::ValidateRoadSplines(const FStrategicAIBlackboard& Blackboard)
	{
		TSet<const ARoadSplineActor*> UniqueSplines;
		int32 InvalidSplineCount = 0;
		for (const TWeakObjectPtr<ARoadSplineActor>& RoadSpline : Blackboard.RoadSplineActors)
		{
			if (RoadSpline.IsValid())
			{
				UniqueSplines.Add(RoadSpline.Get());
			}
			else
			{
				InvalidSplineCount++;
			}
		}

		RecordExpectation(
			Blackboard.RoadSplineActors.Num() == ExpectedRoadSplineCount
				&& UniqueSplines.Num() == ExpectedRoadSplineCount
				&& InvalidSplineCount == 0,
			TEXT("Blackboard.RoadSplines"),
			FString::Printf(
				TEXT("registered=%d unique=%d invalid=%d"),
				Blackboard.RoadSplineActors.Num(),
				UniqueSplines.Num(),
				InvalidSplineCount));
	}

	void FEnemyAIShippingTestRunner::ValidatePlayerUnitData(const FStrategicAIBlackboard& Blackboard)
	{
		const FResultPlayerUnitCounts& PlayerCounts = Blackboard.CurrentPlayerUnitCounts;
		const int32 PlayerTankCount = PlayerCounts.PlayerArmoredCars
			+ PlayerCounts.PlayerLightTanks
			+ PlayerCounts.PlayerMediumTanks
			+ PlayerCounts.PlayerHeavyTanks;
		RecordExpectation(
			GetIsUsableStrategicLocation(PlayerCounts.PlayerHQLocation),
			TEXT("Blackboard.PlayerHQ"),
			PlayerCounts.PlayerHQLocation.ToCompactString());
		RecordExpectation(
			not PlayerCounts.PlayerResourceBuildings.IsEmpty(),
			TEXT("Blackboard.PlayerResourceBuildings"),
			FString::Printf(TEXT("count=%d"), PlayerCounts.PlayerResourceBuildings.Num()));
		RecordExpectation(
			PlayerCounts.PlayerSquads > 0 && PlayerTankCount > 0,
			TEXT("Blackboard.PlayerCombatUnits"),
			FString::Printf(TEXT("squads=%d tanks=%d"), PlayerCounts.PlayerSquads, PlayerTankCount));

		const TArray<FPlayerUnitBulkLocation>& PlayerBulks =
			Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks;
		int32 InvalidBulkCount = 0;
		for (const FPlayerUnitBulkLocation& PlayerBulk : PlayerBulks)
		{
			if (not GetIsUsableStrategicLocation(PlayerBulk.BulkLocation)
				|| PlayerBulk.Score <= 0.f
				|| PlayerBulk.UnitsInBulk.IsEmpty())
			{
				InvalidBulkCount++;
			}
			for (const TWeakObjectPtr<AActor>& Unit : PlayerBulk.UnitsInBulk)
			{
				InvalidBulkCount += Unit.IsValid() ? 0 : 1;
			}
		}
		// The AI groups player units into bulks by proximity, so the count is dynamic (>= the authored
		// group count); match the async-readiness gate which also uses >=. What matters is each bulk is valid.
		RecordExpectation(
			PlayerBulks.Num() >= ExpectedPlayerUnitBulkCount && InvalidBulkCount == 0,
			TEXT("Blackboard.PlayerUnitBulks"),
			FString::Printf(TEXT("expected>=%d actual=%d invalid=%d"), ExpectedPlayerUnitBulkCount, PlayerBulks.Num(), InvalidBulkCount));
	}

	void FEnemyAIShippingTestRunner::ValidateEnemyBaseData(const FStrategicAIBlackboard& Blackboard)
	{
		int32 InvalidBaseCount = 0;
		for (const FEnemyBasePointCoreBuildings& BasePoint : Blackboard.EnemyBasePoints)
		{
			if (not GetIsUsableStrategicLocation(BasePoint.BaseLocation)
				|| BasePoint.CoreBuildingActors.IsEmpty()
				|| BasePoint.CoreBuildingLocations.IsEmpty())
			{
				InvalidBaseCount++;
			}
			for (const TWeakObjectPtr<AActor>& CoreBuilding : BasePoint.CoreBuildingActors)
			{
				InvalidBaseCount += CoreBuilding.IsValid() ? 0 : 1;
			}
		}

		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TEXT("Blackboard.EnemyBaseClusters"), TEXT("Strategic AI component is invalid."));
			return;
		}

		const int32 MaxBases = StrategicAIComponent->FindEnemyBase_TimerRequest.MaxBasesToReturn;
		const bool bWithinConfiguredMax = MaxBases <= 0 || Blackboard.EnemyBasePoints.Num() <= MaxBases;
		RecordExpectation(
			not Blackboard.EnemyBasePoints.IsEmpty() && bWithinConfiguredMax && InvalidBaseCount == 0,
			TEXT("Blackboard.EnemyBaseClusters"),
			FString::Printf(
				TEXT("clusters=%d max=%d invalid=%d"),
				Blackboard.EnemyBasePoints.Num(),
				MaxBases,
				InvalidBaseCount));
	}

	void FEnemyAIShippingTestRunner::ValidateGeneratedStrategicLocations(const FStrategicAIBlackboard& Blackboard)
	{
		const auto CountInvalidLocations = [](const TArray<FVector>& Locations)
		{
			int32 InvalidCount = 0;
			for (const FVector& Location : Locations)
			{
				InvalidCount += GetIsUsableStrategicLocation(Location) ? 0 : 1;
			}
			return InvalidCount;
		};

		TArray<FVector> DefenseLocations;
		for (const FDefensePositions& DefensePosition : Blackboard.CurrentBaseDefensePositions)
		{
			DefenseLocations.Add(DefensePosition.Location);
		}
		const TArray<FVector>& ConstructionLocations = Blackboard.CurrentConstructionLocations.ConstructionLocations;
		const TArray<FVector>& RoadMineLocations = Blackboard.CurrentMineLocations.RoadMineLocations;
		const TArray<FVector>& DefenseArcMineLocations = Blackboard.CurrentMineLocations.DefenseArcMineLocations;
		RecordExpectation(
			not DefenseLocations.IsEmpty() && CountInvalidLocations(DefenseLocations) == 0,
			TEXT("Blackboard.BaseDefenseLocations"),
			FString::Printf(TEXT("count=%d invalid=%d"), DefenseLocations.Num(), CountInvalidLocations(DefenseLocations)));
		RecordExpectation(
			not ConstructionLocations.IsEmpty() && CountInvalidLocations(ConstructionLocations) == 0,
			TEXT("Blackboard.ConstructionLocations"),
			FString::Printf(TEXT("count=%d invalid=%d"), ConstructionLocations.Num(), CountInvalidLocations(ConstructionLocations)));
		RecordExpectation(
			not RoadMineLocations.IsEmpty() && CountInvalidLocations(RoadMineLocations) == 0,
			TEXT("Blackboard.RoadMineLocations"),
			FString::Printf(TEXT("count=%d invalid=%d"), RoadMineLocations.Num(), CountInvalidLocations(RoadMineLocations)));
		RecordExpectation(
			not DefenseArcMineLocations.IsEmpty() && CountInvalidLocations(DefenseArcMineLocations) == 0,
			TEXT("Blackboard.DefenseArcMineLocations"),
			FString::Printf(TEXT("count=%d invalid=%d"), DefenseArcMineLocations.Num(), CountInvalidLocations(DefenseArcMineLocations)));
	}

	void FEnemyAIShippingTestRunner::ValidateTrainingPressureDeltas()
	{
		UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		AEnemyController* EnemyController = M_EnemyController.Get();
		if (not IsValid(StrategicAIComponent) || not IsValid(EnemyController))
		{
			RecordFailure(TEXT("Training.RuntimeValid"), TEXT("Required AI runtime objects are invalid."));
			return;
		}

		UStochasticDecisionTree* DecisionTree = EnemyController->GetStrategicDecisionTree();
		if (not IsValid(DecisionTree))
		{
			RecordFailure(TEXT("Training.TreeValid"), TEXT("Decision tree is invalid."));
			return;
		}

		TMap<EAITrainingFocus, float> ExpectedFocus;
		TMap<EAITrainingFocusSpecialty, float> ExpectedSpecialty;
		const float GameTimeSeconds = static_cast<float>(GetWorldSeconds());
		const FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetStrategicAIBlackboard();
		ValidateIndependentTrainingActionRouting(*DecisionTree, Blackboard);
		ValidateTrainingPointIncome();
		const int32 ContributionCount = CollectExpectedTrainingPressure(
			*DecisionTree,
			Blackboard,
			GameTimeSeconds,
			ExpectedFocus,
			ExpectedSpecialty);
		RecordExpectation(
			ContributionCount > 0,
			TEXT("Training.RegisteredActionsEmitPressure"),
			FString::Printf(TEXT("contributions=%d"), ContributionCount));
		FEnemyStrategicTrainingState& TrainingState = StrategicAIComponent->GetEditableEnemyTrainingState();
		const FEnemyStrategicTrainingState TrainingStateBefore = TrainingState;
		const TMap<EAITrainingFocus, float> FocusBefore = TrainingState.TrainingBuckets.FocusBuckets;
		const TMap<EAITrainingFocusSpecialty, float> SpecialtyBefore = TrainingState.TrainingBuckets.SpecialtyBuckets;
		StrategicAIComponent->ShippingTest_RunTrainingPressureThinkStep();
		CompareTrainingFocusDeltas(FocusBefore, TrainingState.TrainingBuckets.FocusBuckets, ExpectedFocus);
		CompareTrainingSpecialtyDeltas(SpecialtyBefore, TrainingState.TrainingBuckets.SpecialtyBuckets, ExpectedSpecialty);
		TrainingState = TrainingStateBefore;
	}

	void FEnemyAIShippingTestRunner::ValidateIndependentTrainingActionRouting(
		const UStochasticDecisionTree& DecisionTree,
		const FStrategicAIBlackboard& Blackboard)
	{
		AEnemyController* EnemyController = M_EnemyController.Get();
		if (not IsValid(EnemyController))
		{
			RecordFailure(TEXT("Training.ControlledRuntimeValid"), TEXT("Enemy controller is invalid."));
			return;
		}

		for (const FExpectedSubActionDefinition& ExpectedDefinition : GetExpectedSubActionDefinitions())
		{
			UStrategicAISubAction* SubAction = FindSubActionByType(
				DecisionTree.GetActionDefinitions(),
				ExpectedDefinition.SubtypeAction);
			if (not IsValid(SubAction))
			{
				continue;
			}

			ValidateIndependentTrainingActionRoutingCase(
				*SubAction,
				ExpectedDefinition,
				Blackboard,
				*EnemyController);
		}
	}

	void FEnemyAIShippingTestRunner::ValidateIndependentTrainingActionRoutingCase(
		UStrategicAISubAction& SubAction,
		const FExpectedSubActionDefinition& ExpectedDefinition,
		const FStrategicAIBlackboard& Blackboard,
		AEnemyController& SyntheticUnitActor)
	{
		FStrategicAIBlackboard ControlledBlackboard = Blackboard;
		ControlledBlackboard.IdleDirectControlUnits.Reset();
		float ControlledGameTimeSeconds = static_cast<float>(GetWorldSeconds());
		bool bHasUnitTrainingRequirement = false;
		bool bNonTrainingRequirementsMet = true;
		const auto ConfigureNonTrainingRequirements =
			[&ControlledBlackboard,
				&SyntheticUnitActor,
				&ControlledGameTimeSeconds,
				&bHasUnitTrainingRequirement,
				&bNonTrainingRequirementsMet](const auto& Requirements)
			{
				for (const UStrategicAIActionRequirement* Requirement : Requirements)
				{
					if (not IsValid(Requirement))
					{
						bNonTrainingRequirementsMet = false;
						continue;
					}
					if (Requirement->GetIsUnitTrainingPressureRequirement())
					{
						bHasUnitTrainingRequirement = true;
						continue;
					}

					ConfigureControlledTrainingRequirement(
						*Requirement,
						ControlledBlackboard,
						SyntheticUnitActor,
						ControlledGameTimeSeconds);
					bNonTrainingRequirementsMet = bNonTrainingRequirementsMet
						&& Requirement->GetIsRequirementMet(ControlledBlackboard, ControlledGameTimeSeconds);
				}
			};
		ConfigureNonTrainingRequirements(SubAction.GetNativeVisibleRequirements());
		ConfigureNonTrainingRequirements(SubAction.GetRequirements());

		const FString SubtypeName = UEnum::GetValueAsString(ExpectedDefinition.SubtypeAction);
		RecordExpectation(
			bNonTrainingRequirementsMet,
			FString(TEXT("Training.ControlledNonTrainingRequirementsMet.")) + SubtypeName,
			SubAction.GetDebugString());
		if (not bNonTrainingRequirementsMet)
		{
			return;
		}
		const bool bAreAllRequirementsMet =
			SubAction.GetAreRequirementsMet(ControlledBlackboard, ControlledGameTimeSeconds);
		RecordExpectation(
			bHasUnitTrainingRequirement ? not bAreAllRequirementsMet : bAreAllRequirementsMet,
			FString(TEXT("Training.ControlledRequirementState.")) + SubtypeName,
			FString::Printf(
				TEXT("has_unit_training_requirement=%d all_requirements_met=%d"),
				bHasUnitTrainingRequirement,
				bAreAllRequirementsMet));

		TArray<FEnemyStrategicTrainingPressureContribution> Contributions;
		SubAction.BuildTrainingPressureContributions(
			ControlledBlackboard,
			ControlledGameTimeSeconds,
			Contributions);
		RecordExpectation(
			Contributions.Num() == 1,
			FString(TEXT("Training.ControlledContributionCount.")) + SubtypeName,
			FString::Printf(TEXT("expected=1 actual=%d"), Contributions.Num()));
		for (const FEnemyStrategicTrainingPressureContribution& Contribution : Contributions)
		{
			ValidateTrainingContributionRouting(
				ExpectedDefinition.SubtypeAction,
				Contribution,
				FString(TEXT("Training.ControlledRouting.")) + SubtypeName);
			ValidateControlledTrainingBucketApplication(ExpectedDefinition, Contribution);
		}
	}

	void FEnemyAIShippingTestRunner::ValidateTrainingPointIncome()
	{
		UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TEXT("Training.PointIncome.RuntimeValid"), TEXT("Strategic AI component is invalid."));
			return;
		}

		FEnemyStrategicTrainingState& TrainingState = StrategicAIComponent->GetEditableEnemyTrainingState();
		const FEnemyStrategicTrainingState TrainingStateBefore = TrainingState;
		constexpr int32 InitialTrainingPoints = 17;
		constexpr float InitialIncomeRemainder = 0.375f;
		const int32 AuthoredTrainingPointsPerMinute = TrainingState.EnemyLevelTraining.TrainingPointsPerMinute;
		TrainingState.TrainingPoints = InitialTrainingPoints;
		TrainingState.TrainingPointIncomeRemainder = InitialIncomeRemainder;
		const TArray<float> ElapsedTimeSlices = { 7.25f, 13.5f, 4.125f, 20.75f, 14.375f };
		for (const float ElapsedTimeSlice : ElapsedTimeSlices)
		{
			StrategicAIComponent->ShippingTest_AccumulateTrainingPointIncomeForElapsedSeconds(ElapsedTimeSlice);
		}

		const int32 ExpectedTrainingPoints = InitialTrainingPoints + AuthoredTrainingPointsPerMinute;
		RecordExpectation(
			AuthoredTrainingPointsPerMinute >= 0
				&& TrainingState.TrainingPoints == ExpectedTrainingPoints
				&& FMath::IsNearlyEqual(
					TrainingState.TrainingPointIncomeRemainder,
					InitialIncomeRemainder,
					KINDA_SMALL_NUMBER),
			TEXT("Training.PointIncome.AuthoredPerMinuteRate"),
			FString::Printf(
				TEXT("rate=%d expected_points=%d actual_points=%d expected_remainder=%.3f actual_remainder=%.3f"),
				AuthoredTrainingPointsPerMinute,
				ExpectedTrainingPoints,
				TrainingState.TrainingPoints,
				InitialIncomeRemainder,
				TrainingState.TrainingPointIncomeRemainder));
		TrainingState = TrainingStateBefore;
	}

	int32 FEnemyAIShippingTestRunner::CollectExpectedTrainingPressure(
		const UStochasticDecisionTree& DecisionTree,
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds,
		TMap<EAITrainingFocus, float>& OutExpectedFocus,
		TMap<EAITrainingFocusSpecialty, float>& OutExpectedSpecialty)
	{
		int32 ContributionCount = 0;
		for (const FStrategicAIAction& Action : DecisionTree.GetActionDefinitions())
		{
			for (UStrategicAISubAction* SubAction : Action.GetSubActions())
			{
				if (not IsValid(SubAction))
				{
					continue;
				}

				TArray<FEnemyStrategicTrainingPressureContribution> Contributions;
				SubAction->BuildTrainingPressureContributions(Blackboard, GameTimeSeconds, Contributions);
				for (const FEnemyStrategicTrainingPressureContribution& Contribution : Contributions)
				{
					ContributionCount++;
					RecordExpectation(
						FMath::IsFinite(Contribution.PressureAmount)
							&& Contribution.PressureAmount > 0.f
							&& not Contribution.SourceDebugName.IsEmpty(),
						FString(TEXT("Training.ValidContribution.")) + FString::FromInt(ContributionCount),
						Contribution.SourceDebugName);
					AddExpectedTrainingContribution(
						SubAction->SubtypeAction,
						Contribution,
						OutExpectedFocus,
						OutExpectedSpecialty);
				}
			}
		}

		return ContributionCount;
	}

	void FEnemyAIShippingTestRunner::AddExpectedTrainingContribution(
		const ESubtypeAction SubtypeAction,
		const FEnemyStrategicTrainingPressureContribution& Contribution,
		TMap<EAITrainingFocus, float>& OutExpectedFocus,
		TMap<EAITrainingFocusSpecialty, float>& OutExpectedSpecialty)
	{
		const FExpectedSubActionDefinition* ExpectedDefinition = FindExpectedSubActionDefinition(SubtypeAction);
		if (ExpectedDefinition == nullptr)
		{
			RecordFailure(
				TEXT("Training.UnexpectedActionContribution"),
				UEnum::GetValueAsString(SubtypeAction));
			return;
		}

		ValidateTrainingContributionRouting(
			SubtypeAction,
			Contribution,
			FString(TEXT("Training.ActionRouting.")) + UEnum::GetValueAsString(SubtypeAction));
		OutExpectedFocus.FindOrAdd(ExpectedDefinition->TrainingFocus) += Contribution.PressureAmount;
		OutExpectedSpecialty.FindOrAdd(ExpectedDefinition->TrainingSpecialty) += Contribution.PressureAmount;
	}

	void FEnemyAIShippingTestRunner::ValidateTrainingContributionRouting(
		const ESubtypeAction SubtypeAction,
		const FEnemyStrategicTrainingPressureContribution& Contribution,
		const FString& TestName)
	{
		const FExpectedSubActionDefinition* ExpectedDefinition = FindExpectedSubActionDefinition(SubtypeAction);
		if (ExpectedDefinition == nullptr)
		{
			RecordFailure(TestName, FString(TEXT("Unexpected action: ")) + UEnum::GetValueAsString(SubtypeAction));
			return;
		}

		const bool bMatchesExpectedRouting = FMath::IsFinite(Contribution.PressureAmount)
			&& Contribution.PressureAmount > 0.f
			&& not Contribution.SourceDebugName.IsEmpty()
			&& not Contribution.bSpreadFocusPressureAcrossAllBuckets
			&& Contribution.FocusPressure == ExpectedDefinition->TrainingFocus
			&& Contribution.SpecialtyPressure == ExpectedDefinition->TrainingSpecialty;
		RecordExpectation(
			bMatchesExpectedRouting,
			TestName,
			FString::Printf(
				TEXT("expected_focus=%s actual_focus=%s expected_specialty=%s actual_specialty=%s "
					"spread=%d amount=%.3f source=%s"),
				*UEnum::GetValueAsString(ExpectedDefinition->TrainingFocus),
				*UEnum::GetValueAsString(Contribution.FocusPressure),
				*UEnum::GetValueAsString(ExpectedDefinition->TrainingSpecialty),
				*UEnum::GetValueAsString(Contribution.SpecialtyPressure),
				Contribution.bSpreadFocusPressureAcrossAllBuckets,
				Contribution.PressureAmount,
				*Contribution.SourceDebugName));
	}

	void FEnemyAIShippingTestRunner::ValidateControlledTrainingBucketApplication(
		const FExpectedSubActionDefinition& ExpectedDefinition,
		const FEnemyStrategicTrainingPressureContribution& Contribution)
	{
		FEnemyStrategicTrainingState ControlledTrainingState;
		ControlledTrainingState.AddTrainingPressureContribution(Contribution);
		const FEnemyStrategicTrainingBuckets& Buckets = ControlledTrainingState.TrainingBuckets;
		const float FocusPressure = GetBucketValue(Buckets.FocusBuckets, ExpectedDefinition.TrainingFocus);
		const float SpecialtyPressure = GetBucketValue(
			Buckets.SpecialtyBuckets,
			ExpectedDefinition.TrainingSpecialty);
		// A sub-action whose specialty is NoTrainingPressure contributes focus pressure only, so it must
		// create exactly one focus bucket and zero specialty buckets.
		const bool bExpectsSpecialtyBucket =
			ExpectedDefinition.TrainingSpecialty != EAITrainingFocusSpecialty::NoTrainingPressure;
		const bool bSpecialtyAppliedAsExpected = bExpectsSpecialtyBucket
			? (Buckets.SpecialtyBuckets.Num() == 1
				&& FMath::IsNearlyEqual(SpecialtyPressure, Contribution.PressureAmount, TrainingPressureTolerance))
			: (Buckets.SpecialtyBuckets.Num() == 0);
		const bool bAppliedExactlyToExpectedBuckets = Buckets.FocusBuckets.Num() == 1
			&& FMath::IsNearlyEqual(FocusPressure, Contribution.PressureAmount, TrainingPressureTolerance)
			&& bSpecialtyAppliedAsExpected;
		RecordExpectation(
			bAppliedExactlyToExpectedBuckets,
			FString(TEXT("Training.ControlledBucketApplication."))
				+ UEnum::GetValueAsString(ExpectedDefinition.SubtypeAction),
			FString::Printf(
				TEXT("focus_entries=%d specialty_entries=%d expected=%.3f focus=%.3f specialty=%.3f"),
				Buckets.FocusBuckets.Num(),
				Buckets.SpecialtyBuckets.Num(),
				Contribution.PressureAmount,
				FocusPressure,
				SpecialtyPressure));
	}

	void FEnemyAIShippingTestRunner::CompareTrainingFocusDeltas(
		const TMap<EAITrainingFocus, float>& Before,
		const TMap<EAITrainingFocus, float>& After,
		const TMap<EAITrainingFocus, float>& Expected)
	{
		const TArray<EAITrainingFocus> FocusBuckets =
		{
			EAITrainingFocus::Infantry,
			EAITrainingFocus::LightTanks,
			EAITrainingFocus::MediumTanks,
			EAITrainingFocus::HeavyTanks,
		};
		for (const EAITrainingFocus Focus : FocusBuckets)
		{
			const float ActualDelta = GetBucketValue(After, Focus) - GetBucketValue(Before, Focus);
			const float ExpectedDelta = GetBucketValue(Expected, Focus);
			RecordExpectation(
				FMath::IsNearlyEqual(ActualDelta, ExpectedDelta, TrainingPressureTolerance),
				FString(TEXT("Training.FocusDelta.")) + UEnum::GetValueAsString(Focus),
				FString::Printf(TEXT("expected=%.3f actual=%.3f"), ExpectedDelta, ActualDelta));
		}
	}

	void FEnemyAIShippingTestRunner::CompareTrainingSpecialtyDeltas(
		const TMap<EAITrainingFocusSpecialty, float>& Before,
		const TMap<EAITrainingFocusSpecialty, float>& After,
		const TMap<EAITrainingFocusSpecialty, float>& Expected)
	{
		const TArray<EAITrainingFocusSpecialty> SpecialtyBuckets =
		{
			EAITrainingFocusSpecialty::GeneralPurpose,
			EAITrainingFocusSpecialty::AntiTank,
			EAITrainingFocusSpecialty::AntiInfantry,
			EAITrainingFocusSpecialty::SpecialWeaponCarrier,
		};
		for (const EAITrainingFocusSpecialty Specialty : SpecialtyBuckets)
		{
			const float ActualDelta = GetBucketValue(After, Specialty) - GetBucketValue(Before, Specialty);
			const float ExpectedDelta = GetBucketValue(Expected, Specialty);
			RecordExpectation(
				FMath::IsNearlyEqual(ActualDelta, ExpectedDelta, TrainingPressureTolerance),
				FString(TEXT("Training.SpecialtyDelta.")) + UEnum::GetValueAsString(Specialty),
				FString::Printf(TEXT("expected=%.3f actual=%.3f"), ExpectedDelta, ActualDelta));
		}
	}

	void FEnemyAIShippingTestRunner::ValidateMissionActivity()
	{
		const UEnemyWaveController* WaveController = M_WaveController.Get();
		if (not IsValid(WaveController))
		{
			RecordFailure(TEXT("Missions.WaveControllerValid"), TEXT("Wave controller is invalid."));
			return;
		}

		// Refresh from the persistent formation history at validation time: the mission-activity phase can
		// end the same tick the single patrol wave completes, before a later observation tick would run, so
		// the brief patrol formation must be picked up from the history here rather than only during ticks.
		CollectFormationObservations();

		const int32 AttackMoveCount = WaveController->ShippingTest_GetCompletedAttackMoveWaveCount();
		const int32 PatrolCount = WaveController->ShippingTest_GetCompletedPatrolWaveCount();
		const int32 FailedSpawnCount = WaveController->ShippingTest_GetFailedSpawnCount();
		RecordExpectation(
			AttackMoveCount >= MinimumCompletedAttackMoveWaves,
			TEXT("Missions.RepeatingAttackMoveWaves"),
			FString::Printf(TEXT("completed=%d minimum=%d"), AttackMoveCount, MinimumCompletedAttackMoveWaves));
		RecordExpectation(
			PatrolCount >= MinimumCompletedPatrolWaves,
			TEXT("Missions.RandomPatrolWave"),
			FString::Printf(TEXT("completed=%d minimum=%d"), PatrolCount, MinimumCompletedPatrolWaves));
		RecordExpectation(
			FailedSpawnCount == 0,
			TEXT("Missions.WaveSpawnsSucceeded"),
			FString::Printf(TEXT("failed_spawns=%d"), FailedSpawnCount));
		RecordExpectation(
			not M_SeenAttackMoveFormationIDs.IsEmpty(),
			TEXT("Missions.AttackMoveFormationObserved"),
			FString::Printf(TEXT("unique_formations=%d"), M_SeenAttackMoveFormationIDs.Num()));
		RecordExpectation(
			not M_SeenPatrolFormationIDs.IsEmpty(),
			TEXT("Missions.PatrolFormationObserved"),
			FString::Printf(TEXT("unique_formations=%d"), M_SeenPatrolFormationIDs.Num()));
	}

	void FEnemyAIShippingTestRunner::ValidateAfterGarbageCollection()
	{
		const bool bRuntimeObjectsSurvived = TryCacheRuntimeObjects();
		RecordExpectation(
			bRuntimeObjectsSurvived,
			TEXT("GC.CoreRuntimeSurvived"),
			FString::Printf(TEXT("cycles=%d"), M_CompletedGarbageCollectionCycles));
		if (not bRuntimeObjectsSurvived)
		{
			return;
		}

		UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		UGlobalAbilitiesManager* GlobalAbilitiesManager = M_GlobalAbilitiesManager.Get();
		if (not IsValid(StrategicAIComponent) || not IsValid(GlobalAbilitiesManager))
		{
			RecordFailure(TEXT("GC.CoreRuntimeDereference"), TEXT("Cached runtime object became invalid."));
			return;
		}

		ValidateDecisionTreeLayout(false);
		ValidateRoadSplines(StrategicAIComponent->GetStrategicAIBlackboard());
		ValidateIdleUnitBlackboard(TEXT("GC"), false);
		ValidateFormationSnapshots();
		RecordExpectation(
			IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_HeavyCarpet_PE8_500Gr))
				&& IsValid(GlobalAbilitiesManager->FindLoadedAbilityByType(EGlobalAbility::GA_SovietT34Drop)),
			TEXT("GC.GlobalAbilitiesSurvived"),
			TEXT("Both target-gated ability UObjects remain valid after forced GC."));
	}

	void FEnemyAIShippingTestRunner::ValidateNoUnexpectedTrainingSpawns()
	{
		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TEXT("Training.NoUnexpectedSpawns"), TEXT("Strategic AI component is invalid."));
			return;
		}

		const int32 SpawnRequestDelta = StrategicAIComponent->ShippingTest_GetTrainingSpawnRequestCount()
			- M_InitialTrainingSpawnRequestCount;
		const int32 SpawnSuccessDelta = StrategicAIComponent->ShippingTest_GetTrainingSpawnSuccessCount()
			- M_InitialTrainingSpawnSuccessCount;
		const int32 ActiveBatchCount = StrategicAIComponent->ShippingTest_GetActiveTrainingSpawnBatchCount();
		RecordExpectation(
			SpawnRequestDelta == 0 && SpawnSuccessDelta == 0 && ActiveBatchCount == 0,
			TEXT("Training.NoUnexpectedSpawnsWithoutTrainers"),
			FString::Printf(
				TEXT("request_delta=%d success_delta=%d active_batches=%d"),
				SpawnRequestDelta,
				SpawnSuccessDelta,
				ActiveBatchCount));
	}

	void FEnemyAIShippingTestRunner::ValidateFormationSnapshots()
	{
		UEnemyFormationController* FormationController = M_FormationController.Get();
		if (not IsValid(FormationController))
		{
			RecordFailure(TEXT("GC.FormationControllerValid"), TEXT("Formation controller is invalid."));
			return;
		}

		TArray<FFormationData> Formations;
		FormationController->GetActiveFormationData(Formations);
		int32 InvalidFormationCount = 0;
		for (const FFormationData& Formation : Formations)
		{
			if (Formation.FormationUnits.IsEmpty()
				|| not Formation.FormationWaypoints.IsValidIndex(Formation.CurrentWaypointIndex)
				|| Formation.FormationWaypointDirections.Num() != Formation.FormationWaypoints.Num())
			{
				InvalidFormationCount++;
			}
			for (const FFormationUnitData& FormationUnit : Formation.FormationUnits)
			{
				InvalidFormationCount += FormationUnit.IsValidFormationUnit() ? 0 : 1;
			}
		}

		RecordExpectation(
			InvalidFormationCount == 0,
			TEXT("GC.FormationStateValid"),
			FString::Printf(TEXT("active=%d invalid=%d"), Formations.Num(), InvalidFormationCount));
	}

	void FEnemyAIShippingTestRunner::ValidateStochasticExecution()
	{
		const UEnemyStrategicAIComponent* StrategicAIComponent = M_StrategicAIComponent.Get();
		if (not IsValid(StrategicAIComponent))
		{
			RecordFailure(TEXT("Tree.ExecutionRuntimeValid"), TEXT("Strategic AI component is invalid."));
			return;
		}

		const FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetStrategicAIBlackboard();
		ValidateNoUnexpectedTrainingSpawns();
		RecordExpectation(
			Blackboard.StrategicAIMissionSettings.bAllowDirectControlStochasticDecisionTree,
			TEXT("Tree.DirectControlEnabled"),
			TEXT("UT_EnemyAI must allow the decision tree to command blackboard idle units."));
		RecordExpectation(
			not M_ObservedExecutedActions.IsEmpty(),
			TEXT("Tree.ExecutedAtLeastOneAction"),
			FString::Printf(TEXT("executed_subtypes=%d"), M_ObservedExecutedActions.Num()));
		for (const ESubtypeAction ExecutedAction : M_ObservedExecutedActions)
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Display,
			       TEXT("RTS_ENEMY_AI_TREE_EXECUTED subtype=%s"),
			       *UEnum::GetValueAsString(ExecutedAction));
		}
	}

	void FEnemyAIShippingTestRunner::RecordPass(const FString& TestName, const FString& Details)
	{
		M_PassedTestCount++;
		const FString Line = FString::Printf(TEXT("PASS %s %s"), *TestName, *Details);
		M_ResultLines.Add(Line);
		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_TEST_PASS name=%s details=%s"),
		       *TestName,
		       *Details);
	}

	void FEnemyAIShippingTestRunner::RecordFailure(const FString& TestName, const FString& Details)
	{
		M_FailedTestCount++;
		const FString Line = FString::Printf(TEXT("FAIL %s %s"), *TestName, *Details);
		M_ResultLines.Add(Line);
		UE_LOG(LogRTSEnemyAIShippingTests, Error,
		       TEXT("RTS_ENEMY_AI_TEST_FAIL name=%s details=%s"),
		       *TestName,
		       *Details);
	}

	void FEnemyAIShippingTestRunner::RecordExpectation(
		const bool bCondition,
		const FString& TestName,
		const FString& Details)
	{
		if (bCondition)
		{
			RecordPass(TestName, Details);
			return;
		}

		RecordFailure(TestName, Details);
	}

	void FEnemyAIShippingTestRunner::FinishRun()
	{
		if (not bM_IsRunning)
		{
			return;
		}

		SetPhase(EEnemyAIShippingTestPhase::Complete);
		bM_IsRunning = false;
		WriteSummary();
		const bool bPassed = M_FailedTestCount == 0;
		UE_LOG(LogRTSEnemyAIShippingTests, Display,
		       TEXT("RTS_ENEMY_AI_TEST_RESULT %s passed=%d failed=%d elapsed=%.2f"),
		       bPassed ? TEXT("PASS") : TEXT("FAIL"),
		       M_PassedTestCount,
		       M_FailedTestCount,
		       GetRunElapsedSeconds());
		if (bM_ExitOnComplete)
		{
			FPlatformMisc::RequestExitWithStatus(
				false,
				bPassed ? 0 : 1,
				TEXT("RTS Enemy AI Shipping Tests"));
		}
	}

	void FEnemyAIShippingTestRunner::WriteSummary() const
	{
		const FString SummaryDirectory = FPaths::ProjectSavedDir() / TEXT("EnemyAIShippingTests");
		IFileManager::Get().MakeDirectory(*SummaryDirectory, true);
		const FString SummaryPath = SummaryDirectory / FString::Printf(
			TEXT("Run_%s.txt"),
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		FString Summary = FString::Printf(
			TEXT("RESULT %s\nPASSED %d\nFAILED %d\n"),
			M_FailedTestCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
			M_PassedTestCount,
			M_FailedTestCount);
		for (const FString& ResultLine : M_ResultLines)
		{
			Summary += ResultLine + LINE_TERMINATOR;
		}

		if (not FFileHelper::SaveStringToFile(Summary, *SummaryPath))
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Error,
			       TEXT("Failed to write Enemy AI Shipping test summary: %s"),
			       *SummaryPath);
		}
	}

	void FEnemyAIShippingTestRunner::ResetRunState()
	{
		M_EnemyController.Reset();
		M_StrategicAIComponent.Reset();
		M_FormationController.Reset();
		M_WaveController.Reset();
		M_DirectControlComponent.Reset();
		M_GlobalAbilitiesManager.Reset();
		M_InitialActionExecutionTimes.Reset();
		M_ObservedExecutedActions.Reset();
		M_SeenAttackMoveFormationIDs.Reset();
		M_SeenPatrolFormationIDs.Reset();
		M_ResultLines.Reset();
		M_RunReason.Reset();
		M_RunStartedSeconds = 0.0;
		M_PhaseStartedSeconds = 0.0;
		M_CooldownBaselineSeconds = 0.0;
		M_CooldownBaselineWorldSeconds = 0.0;
		M_InitialT34CooldownRemaining = INDEX_NONE;
		M_InitialCarpetCooldownRemaining = INDEX_NONE;
		M_InitialTrainingSpawnRequestCount = 0;
		M_InitialTrainingSpawnSuccessCount = 0;
		M_RequestedGarbageCollectionCycles = DefaultGarbageCollectionCycles;
		M_CompletedGarbageCollectionCycles = 0;
		M_PassedTestCount = 0;
		M_FailedTestCount = 0;
		M_Phase = EEnemyAIShippingTestPhase::WaitForRuntime;
		bM_IsRunning = false;
		bM_ExitOnComplete = false;
		bM_PressureOnlyTrainingEnabled = false;
	}

	void FEnemyAIShippingTestRunner::ResumeTestWorldIfPaused() const
	{
		UWorld* World = M_World.Get();
		if (not IsValid(World))
		{
			return;
		}

		// The RTS boots paused behind a "click to start" widget: UPlayerStartGameControl calls
		// ACPPController::PauseGame(ForcePause) at BeginPlay. Toggling UGameplayStatics::SetGamePaused alone
		// leaves the controller's own M_PauseGameState.bM_IsGamePaused set, and PauseGame(ForceUnpause)
		// early-returns while that flag is stale. If game-thread timers never run, units do not register
		// with the unit manager, so strategic blackboard requests (base clusters, player HQ, mines) find
		// nothing. Drive the real start-game path so the controller's pause state is cleared properly.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			UE_LOG(LogRTSEnemyAIShippingTests, Warning,
			       TEXT("Enemy AI test map is paused at the start-game gate; issuing start so AI systems can progress."));
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}

		// Backstop: ensure the world clock itself is running even if the controller was unavailable.
		if (UGameplayStatics::IsGamePaused(World))
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}

	double FEnemyAIShippingTestRunner::GetWorldSeconds() const
	{
		const UWorld* World = M_World.Get();
		return IsValid(World) ? World->GetTimeSeconds() : 0.0;
	}

	double FEnemyAIShippingTestRunner::GetRealTimeSeconds() const
	{
		return FPlatformTime::Seconds();
	}

	double FEnemyAIShippingTestRunner::GetRunElapsedSeconds() const
	{
		return GetRealTimeSeconds() - M_RunStartedSeconds;
	}

	double FEnemyAIShippingTestRunner::GetPhaseElapsedSeconds() const
	{
		return GetRealTimeSeconds() - M_PhaseStartedSeconds;
	}
}

#endif // defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
