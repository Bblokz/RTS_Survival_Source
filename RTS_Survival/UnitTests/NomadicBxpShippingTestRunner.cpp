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
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
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
	FString M_RunReason;
	FString M_NameFilter;
	TArray<FString> M_ExcludeNameFilters;
	int32 M_CurrentScenarioIndex = INDEX_NONE;
	int32 M_PassedScenarioCount = 0;
	int32 M_FailedScenarioCount = 0;
	int32 M_MaxScenarios = 0;
	float M_StartDelaySeconds = 0.0f;
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

	if (M_Scenarios.IsEmpty())
	{
		GatherScenarios();
		StartNextScenario();
		return;
	}

	if (not M_Scenarios.IsValidIndex(M_CurrentScenarioIndex))
	{
		FinishRun();
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
	M_Scenarios.Empty();
	bM_IsRunning = true;

	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_TEST_BEGIN reason=%s max_scenarios=%d name_filter=%s exclude_filters=%s"),
	       *M_RunReason,
	       M_MaxScenarios,
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
	const bool bPassed = M_FailedScenarioCount == 0 && M_PassedScenarioCount > 0;
	UE_LOG(LogRTSNomadicBxpShippingTests, Display,
	       TEXT("RTS_NOMADIC_BXP_TEST_RESULT %s passed=%d failed=%d total=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"),
	       M_PassedScenarioCount,
	       M_FailedScenarioCount,
	       M_PassedScenarioCount + M_FailedScenarioCount);

	bM_IsRunning = false;

	if (bM_ExitOnComplete)
	{
		FPlatformMisc::RequestExit(false);
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
		FinishRun();
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
