// Copyright (C) Bas Blokzijl - All rights reserved.

// Unit roster test (-RTSRunMapTest=UnitRoster): every unit type the player's async spawner can train (both factions:
// tanks, harvesters, squads, nomadics, aircraft) and every variant in the unit variant data asset is spawned, moved,
// made to fight a copy of itself owned by the enemy, deployed and packed up (nomadics) and killed. Each scenario counts
// the error lines logged while it ran. Compiled only into the RTS_SurvivalShippingMapTests target; see
// Docs/Testing/RTS_SurvivalShippingMapTests.md.

#include "CoreMinimal.h"

#ifndef RTS_WITH_SHIPPING_MAP_TESTS
#define RTS_WITH_SHIPPING_MAP_TESTS 0
#endif

#if RTS_WITH_SHIPPING_MAP_TESTS

#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirector.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirectorUnitOverrideSettings.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirectorUnitOverridesDataAsset.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "Stats/Stats.h"
#include "Tickable.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitRosterTests, Log, All);

namespace RTS::UnitTests::UnitRoster
{
	constexpr uint8 PlayerIndex = 1;
	constexpr uint8 EnemyIndex = 2;
	constexpr float AutoStartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 30.f;
	constexpr float SettleSeconds = 2.f;
	constexpr float MoveDistance = 1500.f;
	constexpr float MinMovedDistance = 300.f;
	// Units spawn facing +X and are sent along +Y: heavy tanks turn in place first.
	constexpr float MoveTimeoutSeconds = 40.f;
	constexpr float DuelDistance = 2500.f;
	constexpr float DuelSeconds = 20.f;
	constexpr float NomadicFastForwardDelaySeconds = 3.f;
	constexpr float NomadicDeployTimeoutSeconds = 60.f;
	constexpr float NomadicPackTimeoutSeconds = 120.f;
	constexpr float DeathTimeoutSeconds = 12.f;
	constexpr float DeathLingerSeconds = 3.f;
	constexpr float KillDamage = 1.0e9f;
	constexpr float AircraftFlightHeight = 2200.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	constexpr float SpawnLift = 150.f;
	constexpr float NavProjectExtent = 2000.f;
	constexpr int32 MaxErrorSamples = 3;
	constexpr int32 MaxErrorSampleLength = 240;
	constexpr int32 MaxTestMapTravelAttempts = 3;
	constexpr int32 DefaultDifficultyPercentage = 50;
	const TCHAR* const DefaultTestMap = TEXT("/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics");
	const TCHAR* const WreckClassPrefix = TEXT("Env_Dest");
	// The stack dump that follows each reported error arrives as many error lines of this category.
	const FName StackDumpCategory(TEXT("LogOutputDevice"));

	bool GetWasRequestedOnCommandLine()
	{
		FString RequestedMapTest;
		return FParse::Param(FCommandLine::Get(), TEXT("RTSRunUnitRosterTests"))
			|| (FParse::Value(FCommandLine::Get(), TEXT("RTSRunMapTest="), RequestedMapTest)
				&& RequestedMapTest == TEXT("UnitRoster"));
	}

	FString GetTestMapPackageName()
	{
		FString MapPackageName = DefaultTestMap;
		(void)FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterMap="), MapPackageName);
		return MapPackageName;
	}

	bool GetIsCurrentMapSupported(const UWorld& World)
	{
		return World.GetMapName().Contains(FPackageName::GetShortName(GetTestMapPackageName()));
	}

	// Same read as the menu battles: the landed flag is private movement data on the class default.
	bool GetAircraftStartsLanded(const UClass& AircraftClass)
	{
		const AAircraftMaster* Defaults = Cast<AAircraftMaster>(AircraftClass.GetDefaultObject());
		const FStructProperty* MovementProperty = FindFProperty<FStructProperty>(
			AAircraftMaster::StaticClass(), TEXT("M_AircraftMovementSettings"));
		if (Defaults == nullptr || MovementProperty == nullptr)
		{
			return true;
		}
		const FBoolProperty* LandedProperty = FindFProperty<FBoolProperty>(
			MovementProperty->Struct, TEXT("bM_StartedAsLanded"));
		if (LandedProperty == nullptr)
		{
			return true;
		}
		return LandedProperty->GetPropertyValue_InContainer(MovementProperty->ContainerPtrToValuePtr<void>(Defaults));
	}

	FString GetVariantName(const EUnitVariant Variant)
	{
		switch (Variant)
		{
		case EUnitVariant::Base:
			return TEXT("Base");
		case EUnitVariant::VariantA:
			return TEXT("VariantA");
		case EUnitVariant::VariantB:
			return TEXT("VariantB");
		case EUnitVariant::VariantC:
			return TEXT("VariantC");
		default:
			return TEXT("Unknown");
		}
	}
}

/** Counts error lines from every thread while a roster scenario runs; registered with GLog. */
class FUnitRosterLogCounter final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, const ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type BaseVerbosity = static_cast<ELogVerbosity::Type>(
			Verbosity & ELogVerbosity::VerbosityMask);
		if (BaseVerbosity > ELogVerbosity::Error || Category == RTS::UnitTests::UnitRoster::StackDumpCategory)
		{
			return;
		}
		FScopeLock Lock(&M_Lock);
		++M_ErrorCount;
		if (Message == nullptr || not GetMentionsWatchedUnit(Message))
		{
			return;
		}
		// Only errors that name the unit under test (or what it spawned) count: the test map's own units keep
		// logging their errors in the background.
		++M_OwnErrorCount;
		++M_ErrorsPerCategory.FindOrAdd(Category);
		FString Sample = FString(Message).Left(RTS::UnitTests::UnitRoster::MaxErrorSampleLength);
		Sample.ReplaceInline(TEXT("\n"), TEXT(" "));
		Sample.ReplaceInline(TEXT("\r"), TEXT(" "));
		if (M_Samples.Num() < RTS::UnitTests::UnitRoster::MaxErrorSamples && not M_Samples.Contains(Sample))
		{
			M_Samples.Add(Sample);
		}
	}

	void AddWatchToken(const FString& Token)
	{
		FScopeLock Lock(&M_Lock);
		if (not Token.IsEmpty())
		{
			M_WatchTokens.AddUnique(Token);
		}
	}

	int32 GetOwnErrorCount()
	{
		FScopeLock Lock(&M_Lock);
		return M_OwnErrorCount;
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	void Reset()
	{
		FScopeLock Lock(&M_Lock);
		M_ErrorCount = 0;
		M_OwnErrorCount = 0;
		M_ErrorsPerCategory.Reset();
		M_Samples.Reset();
		M_WatchTokens.Reset();
	}

	int32 GetErrorCount()
	{
		FScopeLock Lock(&M_Lock);
		return M_ErrorCount;
	}

	FString DescribeErrors()
	{
		FScopeLock Lock(&M_Lock);
		FString Description;
		for (const TPair<FName, int32>& CategoryCount : M_ErrorsPerCategory)
		{
			Description += FString::Printf(TEXT("%s=%d "), *CategoryCount.Key.ToString(), CategoryCount.Value);
		}
		for (const FString& Sample : M_Samples)
		{
			Description += TEXT("| ") + Sample + TEXT(" ");
		}
		return Description.TrimEnd();
	}

private:
	bool GetMentionsWatchedUnit(const TCHAR* Message) const
	{
		for (const FString& Token : M_WatchTokens)
		{
			if (FCString::Stristr(Message, *Token) != nullptr)
			{
				return true;
			}
		}
		return false;
	}

	FCriticalSection M_Lock;
	int32 M_ErrorCount = 0;
	int32 M_OwnErrorCount = 0;
	TMap<FName, int32> M_ErrorsPerCategory;
	TArray<FString> M_Samples;
	// Class names (without _C) of the unit under test and everything it spawned.
	TArray<FString> M_WatchTokens;
};

enum class EUnitRosterKind : uint8
{
	Tank,
	Squad,
	Nomadic,
	Aircraft,
	Other,
};

enum class EUnitRosterStage : uint8
{
	RequestSpawn,
	WaitForSpawn,
	Settle,
	Move,
	Duel,
	DeployNomadic,
	PackNomadic,
	Kill,
	WaitForDeath,
	Linger,
	Done,
};

struct FUnitRosterEntry
{
	FTrainingOption Option;
	EUnitVariant Variant = EUnitVariant::Base;
	EUnitRosterKind Kind = EUnitRosterKind::Other;
	FString Name;
	// Spawned directly (director overrides, listed classes) instead of through the spawner.
	TSoftClassPtr<AActor> DirectClass;
	// Director overrides are the enemy's units and spawn as the enemy player.
	bool bSpawnAsEnemy = false;
};

// Shared with the async spawn callback, which may outlive the scenario.
struct FUnitRosterSpawnSlot
{
	TWeakObjectPtr<AActor> Actor;
	bool bHasReturned = false;
};

struct FUnitRosterScenario
{
	FUnitRosterEntry Entry;
	EUnitRosterStage Stage = EUnitRosterStage::RequestSpawn;
	double StageStartSeconds = 0.0;
	FVector Origin = FVector::ZeroVector;
	TSharedPtr<FUnitRosterSpawnSlot> SpawnSlot;
	TWeakObjectPtr<AActor> Unit;
	TWeakObjectPtr<AActor> Enemy;
	FVector LocationBeforeMove = FVector::ZeroVector;
	float MovedDistance = 0.f;
	float EnemyHealthLost = 0.f;
	float UnitHealthLost = 0.f;
	bool bDeployed = false;
	bool bPacked = false;
	bool bDied = false;
	bool bActionIssued = false;
	TArray<FString> Failures;
	TArray<FString> Warnings;
	TArray<TWeakObjectPtr<AActor>> SpawnedParts;
	FString UnitClassName = TEXT("-");
	// The owner the unit spawned with; its enemy copy gets the other player.
	uint8 UnitOwner = RTS::UnitTests::UnitRoster::PlayerIndex;

	uint8 GetEnemyOwner() const
	{
		return UnitOwner == RTS::UnitTests::UnitRoster::PlayerIndex
			       ? RTS::UnitTests::UnitRoster::EnemyIndex
			       : RTS::UnitTests::UnitRoster::PlayerIndex;
	}
};

/**
 * @brief Runs the unit roster scenarios one after another on the test map; started from the command line
 * (-RTSRunMapTest=UnitRoster) after the map loaded. Writes one result line per unit to the log and a run file.
 */
class FRTSUnitRosterShippingTestRunner final : public FTickableGameObject
{
public:
	explicit FRTSUnitRosterShippingTestRunner(UWorld& InWorld);
	virtual ~FRTSUnitRosterShippingTestRunner() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	void StartRun();
	bool GetIsForWorld(const UWorld* World) const { return M_World.Get() == World; }

private:
	void GatherEntries();
	void GatherVariantEntries(const UUnitVariantDataAsset& VariantDataAsset);
	void AddEntry(const FTrainingOption& Option, const EUnitVariant Variant);
	bool GetEntryPassesShardAndFilter(const FString& EntryName, const int32 EntryIndex) const;
	void StartNextScenario();
	void AdvanceScenario(FUnitRosterScenario& Scenario);
	void SetStage(FUnitRosterScenario& Scenario, const EUnitRosterStage NewStage) const;
	double GetStageSeconds(const FUnitRosterScenario& Scenario) const;

	void AdvanceRequestSpawn(FUnitRosterScenario& Scenario);
	void AdvanceWaitForSpawn(FUnitRosterScenario& Scenario);
	void AdvanceSettle(FUnitRosterScenario& Scenario);
	void AdvanceMove(FUnitRosterScenario& Scenario);
	void AdvanceDuel(FUnitRosterScenario& Scenario);
	void AdvanceDeployNomadic(FUnitRosterScenario& Scenario);
	void AdvancePackNomadic(FUnitRosterScenario& Scenario);
	void AdvanceKill(FUnitRosterScenario& Scenario);
	void AdvanceWaitForDeath(FUnitRosterScenario& Scenario);
	void AdvanceLinger(FUnitRosterScenario& Scenario);

	bool RequestSpawnThroughSpawner(FUnitRosterScenario& Scenario);
	bool SpawnDirectly(FUnitRosterScenario& Scenario);
	void GatherDirectorEntries();
	// -RTSUnitRosterClasses=<class path;...>: units that are no training option (global-ability aircraft).
	void GatherListedClassEntries(const FString& ClassList);
	void AddDirectorEntry(const FString& DirectorName, const FTrainingOption& Option,
	                      const TSoftClassPtr<AActor>& UnitClass);
	void CheckSpawnedClass(FUnitRosterScenario& Scenario) const;
	void RestoreBaseVariant(const FUnitRosterScenario& Scenario) const;
	void SpawnEnemyCopy(FUnitRosterScenario& Scenario);
	void IssueDuelAttacks(const FUnitRosterScenario& Scenario) const;
	void FinishScenario(FUnitRosterScenario& Scenario);
	void CleanupScenario(FUnitRosterScenario& Scenario) const;
	void FinishRun();
	void WriteRunFile() const;

	FVector GetGroundLocation(const FVector& Location, const bool bProjectOntoNavMesh) const;
	FVector ChooseOrigin() const;
	void ResumeStartGameFlowIfPaused() const;
	void HandleActorSpawned(AActor* SpawnedActor);
	// Errors that name this actor's class count for the running scenario.
	void WatchActorClass(const AActor* Actor) const;

	static TArray<AActor*> GetDamageableParts(AActor* Unit);
	static AActor* GetAttackTarget(AActor* Unit);
	static FVector GetUnitLocation(AActor* Unit);
	static float GetHealthFraction(AActor* Unit);
	static bool GetIsAnyPartAlive(AActor* Unit);
	static void KillUnit(AActor* Unit);

	TWeakObjectPtr<UWorld> M_World;
	TSharedPtr<FUnitRosterLogCounter> M_LogCounter;
	FDelegateHandle M_ActorSpawnedHandle;
	TArray<FUnitRosterEntry> M_Entries;
	TArray<FString> M_ResultLines;
	FUnitRosterScenario M_Scenario;
	TWeakObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;
	FVector M_Origin = FVector::ZeroVector;
	FString M_NameFilter;
	int32 M_ShardIndex = 0;
	int32 M_ShardCount = 1;
	int32 M_CurrentEntryIndex = INDEX_NONE;
	int32 M_PassedCount = 0;
	int32 M_WarnedCount = 0;
	int32 M_FailedCount = 0;
	int32 M_TotalErrorLines = 0;
	float M_StartDelaySeconds = 0.f;
	bool bM_IsRunning = false;
	bool bM_HasGathered = false;
	bool bM_ExitOnComplete = false;
	bool bM_SkipDuels = false;
};

FRTSUnitRosterShippingTestRunner::FRTSUnitRosterShippingTestRunner(UWorld& InWorld)
	: M_World(&InWorld)
{
	M_LogCounter = MakeShared<FUnitRosterLogCounter>();
	GLog->AddOutputDevice(M_LogCounter.Get());
	M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateRaw(this, &FRTSUnitRosterShippingTestRunner::HandleActorSpawned));
}

FRTSUnitRosterShippingTestRunner::~FRTSUnitRosterShippingTestRunner()
{
	if (GLog != nullptr)
	{
		GLog->RemoveOutputDevice(M_LogCounter.Get());
	}
	if (UWorld* World = M_World.Get())
	{
		World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
	}
}

TStatId FRTSUnitRosterShippingTestRunner::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FRTSUnitRosterShippingTestRunner, STATGROUP_Tickables);
}

bool FRTSUnitRosterShippingTestRunner::IsTickable() const
{
	return bM_IsRunning && M_World.IsValid();
}

void FRTSUnitRosterShippingTestRunner::StartRun()
{
	using namespace RTS::UnitTests::UnitRoster;
	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterFilter="), M_NameFilter);
	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterShard="), M_ShardIndex);
	(void)FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterShardCount="), M_ShardCount);
	M_ShardCount = FMath::Max(1, M_ShardCount);
	M_ShardIndex = FMath::Clamp(M_ShardIndex, 0, M_ShardCount - 1);
	bM_ExitOnComplete = FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete"));
	bM_SkipDuels = FParse::Param(FCommandLine::Get(), TEXT("RTSUnitRosterNoDuels"));
	M_StartDelaySeconds = AutoStartDelaySeconds;
	bM_IsRunning = true;
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_BEGIN map=%s shard=%d/%d filter=%s duels=%d"),
	       *M_World->GetMapName(), M_ShardIndex, M_ShardCount, *M_NameFilter, bM_SkipDuels ? 0 : 1);
}

void FRTSUnitRosterShippingTestRunner::Tick(const float DeltaTime)
{
	if (M_StartDelaySeconds > 0.f)
	{
		M_StartDelaySeconds -= DeltaTime;
		return;
	}
	if (not bM_HasGathered)
	{
		bM_HasGathered = true;
		ResumeStartGameFlowIfPaused();
		GatherEntries();
		M_Origin = ChooseOrigin();
		StartNextScenario();
		return;
	}
	if (not M_Entries.IsValidIndex(M_CurrentEntryIndex))
	{
		FinishRun();
		return;
	}
	AdvanceScenario(M_Scenario);
}

void FRTSUnitRosterShippingTestRunner::GatherEntries()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSUnitRosterDirectors")))
	{
		GatherDirectorEntries();
		return;
	}
	FString ClassList;
	if (FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterClasses="), ClassList, false))
	{
		GatherListedClassEntries(ClassList);
		return;
	}
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(M_World.Get());
	if (not IsValid(Spawner))
	{
		UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_RESULT FAIL reason=AsyncSpawnerInvalid"));
		++M_FailedCount;
		return;
	}
	TArray<FTrainingOption> Options = Spawner->ShippingTest_GetAllTrainingOptions();
	Options.Sort([](const FTrainingOption& A, const FTrainingOption& B)
	{
		return A.UnitType != B.UnitType
			       ? static_cast<uint8>(A.UnitType) < static_cast<uint8>(B.UnitType)
			       : A.SubtypeValue < B.SubtypeValue;
	});
	for (const FTrainingOption& Option : Options)
	{
		AddEntry(Option, EUnitVariant::Base);
	}
	const UUnitVariantDataAsset* VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
	if (IsValid(VariantDataAsset))
	{
		M_VariantDataAsset = VariantDataAsset;
		// The player profile's variant choices are applied at mission start; Base entries must spawn the base class.
		for (const FTrainingOption& Unit : VariantDataAsset->GetUnitsWithChoice())
		{
			if (Unit.UnitType != EAllUnitType::UNType_BuildingExpansion)
			{
				(void)Spawner->SetPlayerUnitVariant(*VariantDataAsset, Unit, EUnitVariant::Base);
			}
		}
		GatherVariantEntries(*VariantDataAsset);
	}
	else
	{
		UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_NOTE variant data asset not loaded; variants skipped"));
	}
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_ENTRIES total=%d training_options=%d"),
	       M_Entries.Num(), Options.Num());
}

void FRTSUnitRosterShippingTestRunner::GatherDirectorEntries()
{
	const UEnemyDirectorUnitOverrideSettings* OverrideSettings = GetDefault<UEnemyDirectorUnitOverrideSettings>();
	const UEnemyDirectorUnitOverridesDataAsset* OverrideDataAsset =
		IsValid(OverrideSettings) ? OverrideSettings->GetUnitOverridesDataAsset() : nullptr;
	if (not IsValid(OverrideDataAsset))
	{
		UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_RESULT FAIL reason=DirectorOverridesAssetInvalid"));
		++M_FailedCount;
		return;
	}
	const TArray<TPair<EEnemyDirector, FString>> Directors = {
		{EEnemyDirector::DirectorOfShield, TEXT("Shield")},
		{EEnemyDirector::DirectorOfForge, TEXT("Forge")},
		{EEnemyDirector::DirectorOfHarvest, TEXT("Harvest")},
	};
	for (const TPair<EEnemyDirector, FString>& Director : Directors)
	{
		const FEnemyDirectorUnitOverrides* Overrides = OverrideDataAsset->FindOverrides(Director.Key);
		if (Overrides == nullptr)
		{
			UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_NOTE director %s has no overrides"),
			       *Director.Value);
			continue;
		}
		for (const FTankTrainingOptionSetup& Setup : Overrides->M_TankOverrides)
		{
			AddDirectorEntry(Director.Value, FTrainingOption(EAllUnitType::UNType_Tank,
			                                                 static_cast<uint8>(Setup.TankSubtype)), Setup.UnitClass);
		}
		for (const FSquadTrainingOptionSetup& Setup : Overrides->M_SquadOverrides)
		{
			AddDirectorEntry(Director.Value, FTrainingOption(EAllUnitType::UNType_Squad,
			                                                 static_cast<uint8>(Setup.SquadSubtype)), Setup.UnitClass);
		}
		for (const FNomadicTrainingOptionSetup& Setup : Overrides->M_NomadicOverrides)
		{
			AddDirectorEntry(Director.Value, FTrainingOption(EAllUnitType::UNType_Nomadic,
			                                                 static_cast<uint8>(Setup.NomadicSubtype)), Setup.UnitClass);
		}
		for (const FAircraftTrainingOptionSetup& Setup : Overrides->M_AircraftOverrides)
		{
			AddDirectorEntry(Director.Value, FTrainingOption(EAllUnitType::UNType_Aircraft,
			                                                 static_cast<uint8>(Setup.AircraftSubtype)), Setup.UnitClass);
		}
	}
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_ENTRIES total=%d director_overrides"), M_Entries.Num());
}

void FRTSUnitRosterShippingTestRunner::GatherListedClassEntries(const FString& ClassList)
{
	TArray<FString> ClassPaths;
	ClassList.ParseIntoArray(ClassPaths, TEXT(";"), true);
	for (const FString& ClassPath : ClassPaths)
	{
		const TSoftClassPtr<AActor> UnitClass{FSoftObjectPath(ClassPath)};
		const UClass* LoadedClass = UnitClass.LoadSynchronous();
		FUnitRosterEntry Entry;
		Entry.DirectClass = UnitClass;
		Entry.Kind = IsValid(LoadedClass) && LoadedClass->IsChildOf(AAircraftMaster::StaticClass())
			             ? EUnitRosterKind::Aircraft
			             : EUnitRosterKind::Tank;
		Entry.Name = TEXT("Class.") + FPackageName::ObjectPathToObjectName(ClassPath);
		M_Entries.Add(Entry);
	}
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_ENTRIES total=%d listed_classes"), M_Entries.Num());
}

void FRTSUnitRosterShippingTestRunner::AddDirectorEntry(const FString& DirectorName, const FTrainingOption& Option,
                                                        const TSoftClassPtr<AActor>& UnitClass)
{
	if (UnitClass.IsNull())
	{
		return;
	}
	const int32 EntriesBefore = M_Entries.Num();
	AddEntry(Option, EUnitVariant::Base);
	if (M_Entries.Num() == EntriesBefore)
	{
		return;
	}
	FUnitRosterEntry& Entry = M_Entries.Last();
	Entry.DirectClass = UnitClass;
	Entry.bSpawnAsEnemy = true;
	Entry.Name = TEXT("Director") + DirectorName + TEXT(".") + Entry.Name;
}

void FRTSUnitRosterShippingTestRunner::GatherVariantEntries(const UUnitVariantDataAsset& VariantDataAsset)
{
	const TArray<EUnitVariant> NonBaseVariants = {EUnitVariant::VariantA, EUnitVariant::VariantB, EUnitVariant::VariantC};
	for (const FTrainingOption& Unit : VariantDataAsset.GetUnitsWithChoice())
	{
		if (Unit.UnitType == EAllUnitType::UNType_BuildingExpansion)
		{
			// Building expansion variants need an owner slot; the Nomadic/BXP runner places expansions.
			continue;
		}
		for (const EUnitVariant Variant : NonBaseVariants)
		{
			if (VariantDataAsset.GetIsVariantImplemented(Unit, Variant))
			{
				AddEntry(Unit, Variant);
			}
		}
	}
}

void FRTSUnitRosterShippingTestRunner::AddEntry(const FTrainingOption& Option, const EUnitVariant Variant)
{
	FUnitRosterEntry Entry;
	Entry.Option = Option;
	Entry.Variant = Variant;
	switch (Option.UnitType)
	{
	case EAllUnitType::UNType_Squad:
		Entry.Kind = EUnitRosterKind::Squad;
		break;
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Harvester:
		Entry.Kind = EUnitRosterKind::Tank;
		break;
	case EAllUnitType::UNType_Nomadic:
		Entry.Kind = EUnitRosterKind::Nomadic;
		break;
	case EAllUnitType::UNType_Aircraft:
		Entry.Kind = EUnitRosterKind::Aircraft;
		break;
	default:
		Entry.Kind = EUnitRosterKind::Other;
		break;
	}
	Entry.Name = Option.GetTrainingName() + TEXT(".") + RTS::UnitTests::UnitRoster::GetVariantName(Variant);
	Entry.Name.ReplaceInline(TEXT(" "), TEXT("_"));
	const int32 CandidateIndex = M_Entries.Num() + M_ResultLines.Num();
	if (GetEntryPassesShardAndFilter(Entry.Name, CandidateIndex))
	{
		M_Entries.Add(Entry);
	}
	else
	{
		// Keeps the shard assignment stable: every candidate takes an index, also the skipped ones.
		M_ResultLines.Add(FString());
	}
}

bool FRTSUnitRosterShippingTestRunner::GetEntryPassesShardAndFilter(const FString& EntryName,
                                                                    const int32 EntryIndex) const
{
	if (not M_NameFilter.IsEmpty())
	{
		// Semicolon-separated: an entry passes when its name contains any of the filters.
		TArray<FString> Filters;
		M_NameFilter.ParseIntoArray(Filters, TEXT(";"), true);
		const bool bMatchesAnyFilter = Filters.ContainsByPredicate([&EntryName](const FString& Filter)
		{
			return EntryName.Contains(Filter);
		});
		if (not bMatchesAnyFilter)
		{
			return false;
		}
	}
	return EntryIndex % M_ShardCount == M_ShardIndex;
}

void FRTSUnitRosterShippingTestRunner::StartNextScenario()
{
	if (M_CurrentEntryIndex == INDEX_NONE)
	{
		// The placeholders only kept the shard indices stable.
		M_ResultLines.Reset();
	}
	++M_CurrentEntryIndex;
	if (not M_Entries.IsValidIndex(M_CurrentEntryIndex))
	{
		return;
	}
	M_Scenario = FUnitRosterScenario();
	M_Scenario.Entry = M_Entries[M_CurrentEntryIndex];
	M_Scenario.Origin = M_Origin;
	M_LogCounter->Reset();
	SetStage(M_Scenario, EUnitRosterStage::RequestSpawn);
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_SCENARIO_BEGIN index=%d/%d name=%s"),
	       M_CurrentEntryIndex + 1, M_Entries.Num(), *M_Scenario.Entry.Name);
}

void FRTSUnitRosterShippingTestRunner::SetStage(FUnitRosterScenario& Scenario, const EUnitRosterStage NewStage) const
{
	Scenario.Stage = NewStage;
	Scenario.StageStartSeconds = M_World.IsValid() ? M_World->GetTimeSeconds() : 0.0;
	Scenario.bActionIssued = false;
}

double FRTSUnitRosterShippingTestRunner::GetStageSeconds(const FUnitRosterScenario& Scenario) const
{
	return M_World.IsValid() ? M_World->GetTimeSeconds() - Scenario.StageStartSeconds : 0.0;
}

void FRTSUnitRosterShippingTestRunner::AdvanceScenario(FUnitRosterScenario& Scenario)
{
	switch (Scenario.Stage)
	{
	case EUnitRosterStage::RequestSpawn:
		AdvanceRequestSpawn(Scenario);
		break;
	case EUnitRosterStage::WaitForSpawn:
		AdvanceWaitForSpawn(Scenario);
		break;
	case EUnitRosterStage::Settle:
		AdvanceSettle(Scenario);
		break;
	case EUnitRosterStage::Move:
		AdvanceMove(Scenario);
		break;
	case EUnitRosterStage::Duel:
		AdvanceDuel(Scenario);
		break;
	case EUnitRosterStage::DeployNomadic:
		AdvanceDeployNomadic(Scenario);
		break;
	case EUnitRosterStage::PackNomadic:
		AdvancePackNomadic(Scenario);
		break;
	case EUnitRosterStage::Kill:
		AdvanceKill(Scenario);
		break;
	case EUnitRosterStage::WaitForDeath:
		AdvanceWaitForDeath(Scenario);
		break;
	case EUnitRosterStage::Linger:
		AdvanceLinger(Scenario);
		break;
	case EUnitRosterStage::Done:
		FinishScenario(Scenario);
		break;
	}
}

void FRTSUnitRosterShippingTestRunner::AdvanceRequestSpawn(FUnitRosterScenario& Scenario)
{
	const bool bSpawnsDirectly = Scenario.Entry.Kind == EUnitRosterKind::Aircraft
		|| not Scenario.Entry.DirectClass.IsNull();
	const bool bRequested = bSpawnsDirectly ? SpawnDirectly(Scenario) : RequestSpawnThroughSpawner(Scenario);
	if (not bRequested)
	{
		RestoreBaseVariant(Scenario);
		SetStage(Scenario, EUnitRosterStage::Done);
		return;
	}
	SetStage(Scenario, EUnitRosterStage::WaitForSpawn);
}

bool FRTSUnitRosterShippingTestRunner::RequestSpawnThroughSpawner(FUnitRosterScenario& Scenario)
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(M_World.Get());
	if (not IsValid(Spawner))
	{
		Scenario.Failures.Add(TEXT("spawner_invalid"));
		return false;
	}
	if (Scenario.Entry.Variant != EUnitVariant::Base)
	{
		const UUnitVariantDataAsset* VariantDataAsset = M_VariantDataAsset.Get();
		if (not IsValid(VariantDataAsset)
			|| not Spawner->SetPlayerUnitVariant(*VariantDataAsset, Scenario.Entry.Option, Scenario.Entry.Variant))
		{
			Scenario.Failures.Add(TEXT("variant_not_applied"));
			return false;
		}
	}
	if (Spawner->GetTrainingOptionClass(Scenario.Entry.Option).IsNull())
	{
		Scenario.Failures.Add(TEXT("no_class_mapped"));
		return false;
	}
	Scenario.SpawnSlot = MakeShared<FUnitRosterSpawnSlot>();
	TWeakPtr<FUnitRosterSpawnSlot> WeakSlot = Scenario.SpawnSlot;
	const FVector SpawnLocation = GetGroundLocation(Scenario.Origin, true)
		+ FVector(0.f, 0.f, RTS::UnitTests::UnitRoster::SpawnLift);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		Scenario.Entry.Option, SpawnLocation, M_World.Get(), M_CurrentEntryIndex,
		[WeakSlot](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			const TSharedPtr<FUnitRosterSpawnSlot> Slot = WeakSlot.Pin();
			if (Slot.IsValid())
			{
				Slot->Actor = SpawnedActor;
				Slot->bHasReturned = true;
			}
		}, FRotator::ZeroRotator);
	if (not bRequested)
	{
		Scenario.Failures.Add(TEXT("spawn_request_rejected"));
	}
	return bRequested;
}

bool FRTSUnitRosterShippingTestRunner::SpawnDirectly(FUnitRosterScenario& Scenario)
{
	using namespace RTS::UnitTests::UnitRoster;
	const bool bIsDirectorClass = not Scenario.Entry.DirectClass.IsNull();
	const ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(M_World.Get());
	UClass* UnitClass = bIsDirectorClass
		                    ? Scenario.Entry.DirectClass.LoadSynchronous()
		                    : IsValid(Spawner)
		                    ? Spawner->GetTrainingOptionClass(Scenario.Entry.Option).LoadSynchronous()
		                    : nullptr;
	if (not IsValid(UnitClass))
	{
		Scenario.Failures.Add(TEXT("class_not_loaded"));
		return false;
	}
	const bool bIsAircraft = UnitClass->IsChildOf(AAircraftMaster::StaticClass());
	FVector SpawnLocation = GetGroundLocation(Scenario.Origin, not bIsAircraft) + FVector(0.f, 0.f, SpawnLift);
	if (bIsAircraft && not GetAircraftStartsLanded(*UnitClass))
	{
		SpawnLocation.Z += AircraftFlightHeight;
	}
	// Director overrides are the enemy's units; the owner check then expects the enemy player.
	Scenario.UnitOwner = Scenario.Entry.bSpawnAsEnemy ? EnemyIndex : PlayerIndex;
	Scenario.SpawnSlot = MakeShared<FUnitRosterSpawnSlot>();
	Scenario.SpawnSlot->Actor = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*M_World.Get(), UnitClass, FTransform(SpawnLocation), Scenario.UnitOwner);
	Scenario.SpawnSlot->bHasReturned = true;
	return true;
}

void FRTSUnitRosterShippingTestRunner::AdvanceWaitForSpawn(FUnitRosterScenario& Scenario)
{
	const bool bReturned = Scenario.SpawnSlot.IsValid() && Scenario.SpawnSlot->bHasReturned;
	if (not bReturned)
	{
		if (GetStageSeconds(Scenario) > RTS::UnitTests::UnitRoster::SpawnTimeoutSeconds)
		{
			Scenario.Failures.Add(TEXT("spawn_timeout"));
			RestoreBaseVariant(Scenario);
			SetStage(Scenario, EUnitRosterStage::Done);
		}
		return;
	}
	RestoreBaseVariant(Scenario);
	Scenario.Unit = Scenario.SpawnSlot->Actor;
	if (not Scenario.Unit.IsValid())
	{
		Scenario.Failures.Add(TEXT("spawn_returned_null"));
		SetStage(Scenario, EUnitRosterStage::Done);
		return;
	}
	Scenario.UnitClassName = Scenario.Unit->GetClass()->GetName();
	WatchActorClass(Scenario.Unit.Get());
	CheckSpawnedClass(Scenario);
	SetStage(Scenario, EUnitRosterStage::Settle);
}

void FRTSUnitRosterShippingTestRunner::CheckSpawnedClass(FUnitRosterScenario& Scenario) const
{
	const UUnitVariantDataAsset* VariantDataAsset = M_VariantDataAsset.Get();
	if (Scenario.Entry.Variant == EUnitVariant::Base || not IsValid(VariantDataAsset))
	{
		return;
	}
	const UClass* VariantClass = VariantDataAsset->FindVariantClass(Scenario.Entry.Option, Scenario.Entry.Variant).Get();
	if (Scenario.Unit->GetClass() != VariantClass)
	{
		Scenario.Failures.Add(FString::Printf(TEXT("wrong_class=%s"), *Scenario.Unit->GetClass()->GetName()));
	}
}

void FRTSUnitRosterShippingTestRunner::RestoreBaseVariant(const FUnitRosterScenario& Scenario) const
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(M_World.Get());
	const UUnitVariantDataAsset* VariantDataAsset = M_VariantDataAsset.Get();
	if (Scenario.Entry.Variant == EUnitVariant::Base || not IsValid(Spawner) || not IsValid(VariantDataAsset))
	{
		return;
	}
	(void)Spawner->SetPlayerUnitVariant(*VariantDataAsset, Scenario.Entry.Option, EUnitVariant::Base);
}

void FRTSUnitRosterShippingTestRunner::AdvanceSettle(FUnitRosterScenario& Scenario)
{
	if (GetStageSeconds(Scenario) < RTS::UnitTests::UnitRoster::SettleSeconds)
	{
		return;
	}
	AActor* Unit = Scenario.Unit.Get();
	if (not GetIsAnyPartAlive(Unit))
	{
		Scenario.Failures.Add(TEXT("dead_or_missing_after_spawn"));
		SetStage(Scenario, EUnitRosterStage::Done);
		return;
	}
	const URTSComponent* RTSComponent = Unit->FindComponentByClass<URTSComponent>();
	if (RTSComponent == nullptr)
	{
		Scenario.Warnings.Add(TEXT("no_rts_component"));
	}
	else if (RTSComponent->GetOwningPlayer() != Scenario.UnitOwner)
	{
		// Soviet Blueprints default to the enemy player; the player's spawner never trains them in a real game.
		Scenario.UnitOwner = RTSComponent->GetOwningPlayer();
	}
	SetStage(Scenario, EUnitRosterStage::Move);
}

void FRTSUnitRosterShippingTestRunner::AdvanceMove(FUnitRosterScenario& Scenario)
{
	using namespace RTS::UnitTests::UnitRoster;
	AActor* Unit = Scenario.Unit.Get();
	if (not GetIsAnyPartAlive(Unit))
	{
		Scenario.Failures.Add(TEXT("died_during_move"));
		SetStage(Scenario, EUnitRosterStage::Done);
		return;
	}
	if (not Scenario.bActionIssued)
	{
		Scenario.bActionIssued = true;
		Scenario.LocationBeforeMove = GetUnitLocation(Unit);
		ICommands* Commands = Cast<ICommands>(Unit);
		if (Commands == nullptr)
		{
			Scenario.Warnings.Add(TEXT("no_commands_interface"));
			SetStage(Scenario, EUnitRosterStage::Duel);
			return;
		}
		const FVector Destination = GetGroundLocation(Scenario.Origin + FVector(0.f, MoveDistance, 0.f), true);
		const ECommandQueueError Error = Commands->MoveToLocation(Destination, true, FRotator::ZeroRotator);
		if (Error != ECommandQueueError::NoError)
		{
			Scenario.Warnings.Add(FString::Printf(TEXT("move_rejected=%d"), static_cast<int32>(Error)));
		}
		return;
	}
	Scenario.MovedDistance = FVector::Dist2D(Scenario.LocationBeforeMove, GetUnitLocation(Unit));
	if (Scenario.MovedDistance >= MinMovedDistance)
	{
		SetStage(Scenario, EUnitRosterStage::Duel);
		return;
	}
	if (GetStageSeconds(Scenario) > MoveTimeoutSeconds)
	{
		Scenario.Warnings.Add(FString::Printf(TEXT("did_not_move=%.0f"), Scenario.MovedDistance));
		SetStage(Scenario, EUnitRosterStage::Duel);
	}
}

void FRTSUnitRosterShippingTestRunner::AdvanceDuel(FUnitRosterScenario& Scenario)
{
	const bool bDuels = not bM_SkipDuels && (Scenario.Entry.Kind == EUnitRosterKind::Tank
		|| Scenario.Entry.Kind == EUnitRosterKind::Squad || Scenario.Entry.Kind == EUnitRosterKind::Aircraft);
	if (not bDuels)
	{
		SetStage(Scenario, Scenario.Entry.Kind == EUnitRosterKind::Nomadic
			                   ? EUnitRosterStage::DeployNomadic
			                   : EUnitRosterStage::Kill);
		return;
	}
	if (not Scenario.bActionIssued)
	{
		Scenario.bActionIssued = true;
		SpawnEnemyCopy(Scenario);
		return;
	}
	// Soldiers a squad spawns later keep the owner of their own Blueprint until matched to the squad.
	if (ASquadController* EnemySquad = Cast<ASquadController>(Scenario.Enemy.Get()))
	{
		for (ASquadUnit* Soldier : EnemySquad->GetSquadUnitsChecked())
		{
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, Scenario.GetEnemyOwner());
		}
	}
	const double DuelSeconds = GetStageSeconds(Scenario);
	constexpr double AttackOrderDelaySeconds = 2.0;
	if (DuelSeconds >= AttackOrderDelaySeconds && Scenario.EnemyHealthLost == 0.f && Scenario.UnitHealthLost == 0.f)
	{
		// Re-issued until someone is hurt: squads spawn their soldiers after the first frames.
		IssueDuelAttacks(Scenario);
	}
	Scenario.EnemyHealthLost = 1.f - GetHealthFraction(Scenario.Enemy.Get());
	Scenario.UnitHealthLost = 1.f - GetHealthFraction(Scenario.Unit.Get());
	if (DuelSeconds < RTS::UnitTests::UnitRoster::DuelSeconds)
	{
		return;
	}
	if (Scenario.EnemyHealthLost <= 0.f && Scenario.UnitHealthLost <= 0.f)
	{
		Scenario.Warnings.Add(TEXT("no_damage_in_duel"));
	}
	SetStage(Scenario, EUnitRosterStage::Kill);
}

void FRTSUnitRosterShippingTestRunner::SpawnEnemyCopy(FUnitRosterScenario& Scenario)
{
	AActor* Unit = Scenario.Unit.Get();
	if (not IsValid(Unit))
	{
		return;
	}
	using namespace RTS::UnitTests::UnitRoster;
	const FVector UnitLocation = GetUnitLocation(Unit);
	const bool bAirborneCopy = Unit->IsA<AAircraftMaster>() && not GetAircraftStartsLanded(*Unit->GetClass());
	FVector EnemyLocation = GetGroundLocation(UnitLocation + FVector(DuelDistance, 0.f, 0.f), not bAirborneCopy)
		+ FVector(0.f, 0.f, SpawnLift);
	if (bAirborneCopy)
	{
		// Air-to-air duel: the copy cruises at the height the tested aircraft spawned at.
		EnemyLocation.Z += AircraftFlightHeight;
	}
	const FRotator FacingUnit = (UnitLocation - EnemyLocation).GetSafeNormal2D().Rotation();
	Scenario.Enemy = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*M_World.Get(), Unit->GetClass(), FTransform(FacingUnit, EnemyLocation),
		Scenario.GetEnemyOwner());
	if (not Scenario.Enemy.IsValid())
	{
		Scenario.Warnings.Add(TEXT("enemy_copy_not_spawned"));
	}
}

void FRTSUnitRosterShippingTestRunner::IssueDuelAttacks(const FUnitRosterScenario& Scenario) const
{
	AActor* Unit = Scenario.Unit.Get();
	AActor* Enemy = Scenario.Enemy.Get();
	AActor* UnitTarget = GetAttackTarget(Enemy);
	AActor* EnemyTarget = GetAttackTarget(Unit);
	if (ICommands* UnitCommands = Cast<ICommands>(Unit); UnitCommands != nullptr && IsValid(UnitTarget))
	{
		(void)UnitCommands->AttackActor(UnitTarget, true);
	}
	if (ICommands* EnemyCommands = Cast<ICommands>(Enemy); EnemyCommands != nullptr && IsValid(EnemyTarget))
	{
		(void)EnemyCommands->AttackActor(EnemyTarget, true);
	}
}

void FRTSUnitRosterShippingTestRunner::AdvanceDeployNomadic(FUnitRosterScenario& Scenario)
{
	using namespace RTS::UnitTests::UnitRoster;
	ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Scenario.Unit.Get());
	if (not IsValid(Nomadic))
	{
		Scenario.Failures.Add(TEXT("nomadic_missing_before_deploy"));
		SetStage(Scenario, EUnitRosterStage::Kill);
		return;
	}
	if (not Scenario.bActionIssued)
	{
		Scenario.bActionIssued = true;
		const ECommandQueueError Error = Nomadic->CreateBuildingAtLocation(
			Nomadic->GetActorLocation(), Nomadic->GetActorRotation(), true);
		if (Error != ECommandQueueError::NoError)
		{
			Scenario.Failures.Add(FString::Printf(TEXT("deploy_rejected=%d"), static_cast<int32>(Error)));
			SetStage(Scenario, EUnitRosterStage::Kill);
		}
		return;
	}
	if (Nomadic->GetNomadicStatus() == ENomadStatus::Building)
	{
		Scenario.bDeployed = true;
		SetStage(Scenario, EUnitRosterStage::PackNomadic);
		return;
	}
	const double Waited = GetStageSeconds(Scenario);
	if (Waited >= NomadicFastForwardDelaySeconds)
	{
		(void)Nomadic->ShippingTest_ForceFinishConversionToBuilding();
	}
	if (Waited > NomadicDeployTimeoutSeconds)
	{
		Scenario.Failures.Add(FString::Printf(TEXT("deploy_timeout_status=%d"),
		                                      static_cast<int32>(Nomadic->GetNomadicStatus())));
		SetStage(Scenario, EUnitRosterStage::Kill);
	}
}

void FRTSUnitRosterShippingTestRunner::AdvancePackNomadic(FUnitRosterScenario& Scenario)
{
	ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Scenario.Unit.Get());
	if (not IsValid(Nomadic))
	{
		Scenario.Failures.Add(TEXT("nomadic_missing_before_pack"));
		SetStage(Scenario, EUnitRosterStage::Kill);
		return;
	}
	if (not Scenario.bActionIssued)
	{
		Scenario.bActionIssued = true;
		const ECommandQueueError Error = Nomadic->ConvertToVehicle(true);
		if (Error != ECommandQueueError::NoError)
		{
			Scenario.Failures.Add(FString::Printf(TEXT("pack_rejected=%d"), static_cast<int32>(Error)));
			SetStage(Scenario, EUnitRosterStage::Kill);
		}
		return;
	}
	if (Nomadic->GetNomadicStatus() == ENomadStatus::Truck)
	{
		Scenario.bPacked = true;
		SetStage(Scenario, EUnitRosterStage::Kill);
		return;
	}
	if (GetStageSeconds(Scenario) > RTS::UnitTests::UnitRoster::NomadicPackTimeoutSeconds)
	{
		Scenario.Failures.Add(FString::Printf(TEXT("pack_timeout_status=%d"),
		                                      static_cast<int32>(Nomadic->GetNomadicStatus())));
		SetStage(Scenario, EUnitRosterStage::Kill);
	}
}

void FRTSUnitRosterShippingTestRunner::AdvanceKill(FUnitRosterScenario& Scenario)
{
	KillUnit(Scenario.Unit.Get());
	KillUnit(Scenario.Enemy.Get());
	SetStage(Scenario, EUnitRosterStage::WaitForDeath);
}

void FRTSUnitRosterShippingTestRunner::AdvanceWaitForDeath(FUnitRosterScenario& Scenario)
{
	const bool bUnitAlive = GetIsAnyPartAlive(Scenario.Unit.Get());
	const bool bEnemyAlive = GetIsAnyPartAlive(Scenario.Enemy.Get());
	if (not bUnitAlive && not bEnemyAlive)
	{
		Scenario.bDied = true;
		SetStage(Scenario, EUnitRosterStage::Linger);
		return;
	}
	if (GetStageSeconds(Scenario) > RTS::UnitTests::UnitRoster::DeathTimeoutSeconds)
	{
		Scenario.Failures.Add(bUnitAlive ? TEXT("survived_lethal_damage") : TEXT("enemy_copy_survived_lethal_damage"));
		SetStage(Scenario, EUnitRosterStage::Linger);
	}
}

void FRTSUnitRosterShippingTestRunner::AdvanceLinger(FUnitRosterScenario& Scenario)
{
	// Death effects, wrecks and crashing aircraft run their course while their errors still count.
	if (GetStageSeconds(Scenario) >= RTS::UnitTests::UnitRoster::DeathLingerSeconds)
	{
		SetStage(Scenario, EUnitRosterStage::Done);
	}
}

void FRTSUnitRosterShippingTestRunner::FinishScenario(FUnitRosterScenario& Scenario)
{
	const int32 ErrorLines = M_LogCounter->GetOwnErrorCount();
	const int32 BackgroundErrorLines = M_LogCounter->GetErrorCount() - ErrorLines;
	M_TotalErrorLines += ErrorLines;
	const TCHAR* Verdict = TEXT("PASS");
	if (not Scenario.Failures.IsEmpty())
	{
		Verdict = TEXT("FAIL");
		++M_FailedCount;
	}
	else if (not Scenario.Warnings.IsEmpty() || ErrorLines > 0)
	{
		Verdict = TEXT("WARN");
		++M_WarnedCount;
	}
	else
	{
		++M_PassedCount;
	}
	const FString& UnitClassName = Scenario.UnitClassName;
	const FString Line = FString::Printf(
		TEXT("%s %s class=%s owner=%d moved=%.0f dealt=%.2f taken=%.2f deployed=%d packed=%d died=%d errors=%d background_errors=%d fails=[%s] warns=[%s] %s"),
		Verdict, *Scenario.Entry.Name, *UnitClassName, Scenario.UnitOwner, Scenario.MovedDistance, Scenario.EnemyHealthLost,
		Scenario.UnitHealthLost, Scenario.bDeployed ? 1 : 0, Scenario.bPacked ? 1 : 0, Scenario.bDied ? 1 : 0,
		ErrorLines, BackgroundErrorLines, *FString::Join(Scenario.Failures, TEXT(",")), *FString::Join(Scenario.Warnings, TEXT(",")),
		*M_LogCounter->DescribeErrors());
	M_ResultLines.Add(Line);
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_SCENARIO %s"), *Line);
	CleanupScenario(Scenario);
	StartNextScenario();
}

void FRTSUnitRosterShippingTestRunner::CleanupScenario(FUnitRosterScenario& Scenario) const
{
	TArray<AActor*> ToDestroy = {Scenario.Unit.Get(), Scenario.Enemy.Get()};
	for (const TWeakObjectPtr<AActor>& Part : Scenario.SpawnedParts)
	{
		ToDestroy.Add(Part.Get());
	}
	for (AActor* Actor : ToDestroy)
	{
		if (IsValid(Actor) && not Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	Scenario.SpawnedParts.Reset();
}

void FRTSUnitRosterShippingTestRunner::FinishRun()
{
	bM_IsRunning = false;
	const bool bPassed = M_FailedCount == 0 && M_PassedCount + M_WarnedCount > 0;
	UE_LOG(LogRTSUnitRosterTests, Display,
	       TEXT("RTS_UNIT_ROSTER_RESULT %s passed=%d warned=%d failed=%d total=%d error_lines=%d shard=%d/%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCount, M_WarnedCount, M_FailedCount, M_Entries.Num(),
	       M_TotalErrorLines, M_ShardIndex, M_ShardCount);
	WriteRunFile();
	if (bM_ExitOnComplete)
	{
		FPlatformMisc::RequestExit(false);
	}
}

void FRTSUnitRosterShippingTestRunner::WriteRunFile() const
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("UnitRosterTests");
	const FString FileName = FString::Printf(TEXT("Run_%s_shard%d.txt"),
	                                         *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), M_ShardIndex);
	FString Contents = FString::Printf(TEXT("RESULT %s passed=%d warned=%d failed=%d total=%d error_lines=%d\n"),
	                                   M_FailedCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_PassedCount,
	                                   M_WarnedCount, M_FailedCount, M_Entries.Num(), M_TotalErrorLines);
	Contents += FString::Join(M_ResultLines, TEXT("\n")) + TEXT("\n");
	(void)FFileHelper::SaveStringToFile(Contents, *(Directory / FileName), FFileHelper::EEncodingOptions::ForceUTF8);
}

FVector FRTSUnitRosterShippingTestRunner::GetGroundLocation(const FVector& Location, const bool bProjectOntoNavMesh) const
{
	using namespace RTS::UnitTests::UnitRoster;
	UWorld* World = M_World.Get();
	FVector Grounded = Location;
	if (not IsValid(World))
	{
		return Grounded;
	}
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UnitRosterGround), false);
	const FVector TraceOffset = FVector::UpVector * GroundTraceHalfHeight;
	if (World->LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset, ECC_Visibility,
	                                    QueryParams))
	{
		Grounded = GroundHit.ImpactPoint;
	}
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation NavLocation;
	if (bProjectOntoNavMesh && IsValid(NavigationSystem)
		&& NavigationSystem->ProjectPointToNavigation(Grounded, NavLocation, FVector(NavProjectExtent)))
	{
		Grounded = NavLocation.Location;
	}
	return Grounded;
}

FVector FRTSUnitRosterShippingTestRunner::ChooseOrigin() const
{
	FString OriginText;
	FVector Origin = FVector::ZeroVector;
	if (FParse::Value(FCommandLine::Get(), TEXT("RTSUnitRosterOrigin="), OriginText))
	{
		TArray<FString> Parts;
		OriginText.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() >= 2)
		{
			Origin = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), 0.0);
		}
	}
	else if (const ACPPController* Controller = FRTS_Statics::GetRTSController(M_World.Get()))
	{
		if (const APawn* CameraPawn = Controller->GetPawn())
		{
			Origin = CameraPawn->GetActorLocation();
		}
	}
	const FVector Grounded = GetGroundLocation(Origin, true);
	UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_ORIGIN %s"), *Grounded.ToCompactString());
	return Grounded;
}

void FRTSUnitRosterShippingTestRunner::ResumeStartGameFlowIfPaused() const
{
	UWorld* World = M_World.Get();
	if (not IsValid(World) || not UGameplayStatics::IsGamePaused(World))
	{
		return;
	}
	if (ACPPController* Controller = FRTS_Statics::GetRTSController(World))
	{
		Controller->ShippingTest_ResumeStartGameFlow();
	}
	if (UGameplayStatics::IsGamePaused(World))
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
}

void FRTSUnitRosterShippingTestRunner::WatchActorClass(const AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return;
	}
	FString ClassName = Actor->GetClass()->GetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	M_LogCounter->AddWatchToken(ClassName);
}

void FRTSUnitRosterShippingTestRunner::HandleActorSpawned(AActor* SpawnedActor)
{
	if (not bM_IsRunning || not IsValid(SpawnedActor))
	{
		return;
	}
	const bool bIsWreck = SpawnedActor->GetClass()->GetName().StartsWith(RTS::UnitTests::UnitRoster::WreckClassPrefix);
	if (bIsWreck || SpawnedActor->IsA<ATeamWeapon>() || SpawnedActor->IsA<ASquadUnit>())
	{
		M_Scenario.SpawnedParts.Add(SpawnedActor);
		WatchActorClass(SpawnedActor);
	}
}

TArray<AActor*> FRTSUnitRosterShippingTestRunner::GetDamageableParts(AActor* Unit)
{
	TArray<AActor*> Parts;
	if (not IsValid(Unit) || Unit->IsActorBeingDestroyed())
	{
		return Parts;
	}
	if (ASquadController* Squad = Cast<ASquadController>(Unit))
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && not Soldier->IsActorBeingDestroyed())
			{
				Parts.Add(Soldier);
			}
		}
		return Parts;
	}
	Parts.Add(Unit);
	return Parts;
}

AActor* FRTSUnitRosterShippingTestRunner::GetAttackTarget(AActor* Unit)
{
	for (AActor* Part : GetDamageableParts(Unit))
	{
		const UHealthComponent* Health = Part->FindComponentByClass<UHealthComponent>();
		if (Health != nullptr && Health->GetCurrentHealth() > 0.f)
		{
			return Part;
		}
	}
	return nullptr;
}

FVector FRTSUnitRosterShippingTestRunner::GetUnitLocation(AActor* Unit)
{
	const TArray<AActor*> Parts = GetDamageableParts(Unit);
	if (not Parts.IsEmpty())
	{
		return Parts[0]->GetActorLocation();
	}
	return IsValid(Unit) ? Unit->GetActorLocation() : FVector::ZeroVector;
}

float FRTSUnitRosterShippingTestRunner::GetHealthFraction(AActor* Unit)
{
	const TArray<AActor*> Parts = GetDamageableParts(Unit);
	float HealthSum = 0.f;
	int32 PartsWithHealth = 0;
	for (const AActor* Part : Parts)
	{
		const UHealthComponent* Health = Part->FindComponentByClass<UHealthComponent>();
		if (Health == nullptr)
		{
			continue;
		}
		HealthSum += FMath::Clamp(Health->GetHealthPercentage(), 0.f, 1.f);
		++PartsWithHealth;
	}
	// A unit whose parts are all gone has lost all its health; one without health components never loses any.
	if (Parts.IsEmpty())
	{
		return 0.f;
	}
	return PartsWithHealth > 0 ? HealthSum / PartsWithHealth : 1.f;
}

bool FRTSUnitRosterShippingTestRunner::GetIsAnyPartAlive(AActor* Unit)
{
	for (const AActor* Part : GetDamageableParts(Unit))
	{
		const UHealthComponent* Health = Part->FindComponentByClass<UHealthComponent>();
		if (Health == nullptr || Health->GetCurrentHealth() > 0.f)
		{
			return true;
		}
	}
	return false;
}

void FRTSUnitRosterShippingTestRunner::KillUnit(AActor* Unit)
{
	for (AActor* Part : GetDamageableParts(Unit))
	{
		UGameplayStatics::ApplyDamage(Part, RTS::UnitTests::UnitRoster::KillDamage, nullptr, nullptr, nullptr);
	}
}

namespace RTS::UnitTests::UnitRoster
{
	TArray<TUniquePtr<FRTSUnitRosterShippingTestRunner>> GActiveRunners;
	int32 GTestMapTravelAttempts = 0;

	void ApplyDefaultRuntimeState(UWorld& World)
	{
		URTSGameInstance* const GameInstance = World.GetGameInstance<URTSGameInstance>();
		if (not IsValid(GameInstance))
		{
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
		const FString MapPackageName = GetTestMapPackageName();
		if (GTestMapTravelAttempts >= MaxTestMapTravelAttempts)
		{
			UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_RESULT FAIL could not travel to %s"),
			       *MapPackageName);
			if (FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete")))
			{
				FPlatformMisc::RequestExit(false);
			}
			return;
		}
		++GTestMapTravelAttempts;
		ApplyDefaultRuntimeState(World);
		TWeakObjectPtr<UWorld> WeakWorld = &World;
		World.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, MapPackageName]()
		{
			if (UWorld* TravelWorld = WeakWorld.Get())
			{
				UE_LOG(LogRTSUnitRosterTests, Display, TEXT("RTS_UNIT_ROSTER_TRAVEL map=%s attempt=%d"),
				       *MapPackageName, GTestMapTravelAttempts);
				UGameplayStatics::OpenLevel(TravelWorld, FName(*MapPackageName));
			}
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
		ApplyDefaultRuntimeState(*World);
		TUniquePtr<FRTSUnitRosterShippingTestRunner> Runner = MakeUnique<FRTSUnitRosterShippingTestRunner>(*World);
		Runner->StartRun();
		GActiveRunners.Add(MoveTemp(Runner));
	}

	void HandleWorldCleanup(UWorld* World, bool, bool)
	{
		GActiveRunners.RemoveAll([World](const TUniquePtr<FRTSUnitRosterShippingTestRunner>& Runner)
		{
			return not Runner.IsValid() || Runner->GetIsForWorld(World);
		});
	}

	struct FUnitRosterShippingTestRegistration
	{
		FDelegateHandle PostWorldInitializationHandle;
		FDelegateHandle WorldCleanupHandle;

		FUnitRosterShippingTestRegistration()
		{
			PostWorldInitializationHandle =
				FWorldDelegates::OnPostWorldInitialization.AddStatic(&HandlePostWorldInitialization);
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
		}

		~FUnitRosterShippingTestRegistration()
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}
	};

	FUnitRosterShippingTestRegistration GRegistration;
}

#endif
