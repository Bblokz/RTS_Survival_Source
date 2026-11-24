// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyWaveSpawnerTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "NavigationSystem.h"
#include "Engine/DamageEvents.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyWaveSpawnerTest, Log, All);

namespace EnemyWaveSpawnerTestPrivate
{
	const TCHAR* const Switch = TEXT("EnemyWaveSpawnerValidate");
	constexpr double StartDelaySeconds = 8.0;
	constexpr int32 TestWaveSupply = 500;
	constexpr int32 AttackWaveCount = 3;
	constexpr int32 AttackUnitsPerWave = 2;
	constexpr float WaveIntervalSeconds = 6.f;
	// Long enough for the 3 attack waves, the patrol building death and a would-be 4th attack wave.
	constexpr double RunWavesSeconds = 45.0;
	constexpr double PatrolBuildingDeathAfterFirstWaveSeconds = 2.0;
	constexpr double KillTimeoutSeconds = 25.0;
	constexpr float KillDamage = 1000000.f;
	constexpr float SpawnerOffset = 2500.f;
	constexpr float PathLength = 1200.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	const FVector NavExtent(2000.f, 2000.f, 5000.f);
	// Base 2 + Normal 1 + Hard 1 + Brutal 1, for NewToRTS, Normal, Hard, Brutal, Ironman.
	const int32 ExpectedUnitsPerDifficulty[] = {2, 3, 4, 5, 5};
	constexpr float DistinctSpawnTolerance = 50.f;
	constexpr float SecondSpawnPointOffset = 600.f;

	enum ESpawnerVariant : int32
	{
		Attack,
		Patrol,
		DeadBuilding,
		DifficultyLists
	};

	FEnemyPlacementUnit GetTankOption()
	{
		return FEnemyPlacementUnit::MakeVehicle(ETankSubtype::Tank_SU_85);
	}

	FEnemyPlacementUnit GetSquadOption()
	{
		return FEnemyPlacementUnit::MakeSquad(ESquadSubtype::Squad_Rus_Mosin);
	}

	FEnemyWaveSpawnerUnitEntry MakeEntry(const FEnemyPlacementUnit& Option, const int32 Count)
	{
		FEnemyWaveSpawnerUnitEntry Entry;
		Entry.UnitOptions = {Option};
		Entry.Count = Count;
		return Entry;
	}
}

bool UEnemyWaveSpawnerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), EnemyWaveSpawnerTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UEnemyWaveSpawnerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyWaveSpawnerTestSubsystem, STATGROUP_Tickables);
}

void UEnemyWaveSpawnerTestSubsystem::Tick(const float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}
	if (M_PhaseStartSeconds < 0.0)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		return;
	}
	switch (M_Phase)
	{
	case EPhase::WaitingForStart:
		if (GetPhaseSeconds() >= EnemyWaveSpawnerTestPrivate::StartDelaySeconds)
		{
			StartTest();
		}
		break;
	case EPhase::RunningWaves:
		TickRunningWaves();
		break;
	case EPhase::KillingAttackUnits:
		TickKillingAttackUnits();
		break;
	case EPhase::Finished:
		break;
	}
#endif
}

void UEnemyWaveSpawnerTestSubsystem::SetPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetWorld()->GetTimeSeconds();
}

double UEnemyWaveSpawnerTestSubsystem::GetPhaseSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds;
}

#if RTS_WITH_MAP_TESTS

void UEnemyWaveSpawnerTestSubsystem::StartTest()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	AEnemyController* const EnemyController = FRTS_Statics::GetEnemyController(this);
	const bool bHasCenter = FindNavigableCenter(M_Center);
	Check(IsValid(EnemyController) && bHasCenter, TEXT("setup"),
	      FString::Printf(TEXT("enemy_controller=%d nav_center=%d"), IsValid(EnemyController), bHasCenter));
	if (not IsValid(EnemyController) || not bHasCenter)
	{
		Evaluate();
		return;
	}
	M_EnemyController = EnemyController;
	EnemyController->SetEnemyWaveSupply(TestWaveSupply);
	CheckDifficultyUnitLists();
	SpawnTestSpawners();
	SetPhase(EPhase::RunningWaves);
}

void UEnemyWaveSpawnerTestSubsystem::CheckDifficultyUnitLists()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	AEnemyWaveSpawner* const Spawner = SpawnConfiguredSpawner(M_Center, DifficultyLists);
	if (not IsValid(Spawner))
	{
		Check(false, TEXT("difficulty_lists_spawner"), TEXT("could not spawn"));
		return;
	}
	const TArray<FVector> WorldSpawnPoints = Spawner->GetWorldPoints(Spawner->SpawnPoints);
	const ERTSGameDifficulty Difficulties[] = {
		ERTSGameDifficulty::NewToRTS, ERTSGameDifficulty::Normal, ERTSGameDifficulty::Hard,
		ERTSGameDifficulty::Brutal, ERTSGameDifficulty::Ironman
	};
	for (int32 DifficultyIndex = 0; DifficultyIndex < static_cast<int32>(UE_ARRAY_COUNT(Difficulties)); ++DifficultyIndex)
	{
		const TArray<FAttackWaveElement> Elements = Spawner->BuildWaveElements(
			Difficulties[DifficultyIndex], WorldSpawnPoints);
		Check(Elements.Num() == ExpectedUnitsPerDifficulty[DifficultyIndex],
		      FString::Printf(TEXT("difficulty_units_%s"), *UEnum::GetValueAsString(Difficulties[DifficultyIndex])),
		      FString::Printf(TEXT("%d (expected %d)"), Elements.Num(), ExpectedUnitsPerDifficulty[DifficultyIndex]));
		const int32 Overlaps = CountOverlappingSpawnLocations(Elements);
		Check(Overlaps == 0, FString::Printf(TEXT("spawn_locations_distinct_%d"), DifficultyIndex),
		      FString::Printf(TEXT("%d overlapping pairs"), Overlaps));
	}
	CheckNothingVisibleInGame(Spawner, TEXT("difficulty_lists"));
	Spawner->Destroy();
}

void UEnemyWaveSpawnerTestSubsystem::SpawnTestSpawners()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	M_PatrolBuilding = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(M_Center));
	AActor* const DeadBuildingActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(M_Center));

	M_AttackSpawner = SpawnConfiguredSpawner(M_Center + FVector(-SpawnerOffset, 0.f, 0.f), Attack);
	M_PatrolSpawner = SpawnConfiguredSpawner(M_Center + FVector(0.f, SpawnerOffset, 0.f), Patrol);
	M_DeadBuildingSpawner = SpawnConfiguredSpawner(M_Center + FVector(0.f, -SpawnerOffset, 0.f), DeadBuilding);
	if (not IsValid(M_AttackSpawner) || not IsValid(M_PatrolSpawner) || not IsValid(M_DeadBuildingSpawner))
	{
		Check(false, TEXT("spawn_spawners"), TEXT("could not spawn the test spawners"));
		Evaluate();
		return;
	}
	M_PatrolSpawner->Ownership.OwningBuilding = M_PatrolBuilding;
	M_DeadBuildingSpawner->Ownership.OwningBuilding = DeadBuildingActor;
	if (IsValid(DeadBuildingActor))
	{
		DeadBuildingActor->Destroy();
	}

	M_AttackSpawner->OnWaveUnitsSpawned.AddDynamic(this, &UEnemyWaveSpawnerTestSubsystem::OnAttackUnitsSpawned);
	M_AttackSpawner->OnSpawnerFinished.AddDynamic(this, &UEnemyWaveSpawnerTestSubsystem::OnAttackFinished);
	M_PatrolSpawner->OnWaveUnitsSpawned.AddDynamic(this, &UEnemyWaveSpawnerTestSubsystem::OnPatrolUnitsSpawned);
	M_PatrolSpawner->OnSpawnerFinished.AddDynamic(this, &UEnemyWaveSpawnerTestSubsystem::OnPatrolFinished);

	M_AttackSpawner->StartSpawner();
	M_PatrolSpawner->StartSpawner();
	M_DeadBuildingSpawner->StartSpawner();
	Check(AEnemyWaveSpawner::FindEnemyWaveSpawner(this, TEXT("Test_Attack")) == M_AttackSpawner,
	      TEXT("find_spawner_by_id"), TEXT("Test_Attack"));
}

AEnemyWaveSpawner* UEnemyWaveSpawnerTestSubsystem::SpawnConfiguredSpawner(const FVector& Location, const int32 Variant)
{
	using namespace EnemyWaveSpawnerTestPrivate;
	AEnemyWaveSpawner* const Spawner = GetWorld()->SpawnActorDeferred<AEnemyWaveSpawner>(
		AEnemyWaveSpawner::StaticClass(), FTransform(Location));
	if (not IsValid(Spawner))
	{
		return nullptr;
	}
	Spawner->Timing.bStartOnBeginPlay = false;
	Spawner->Timing.FirstWaveDelay = 0.f;
	Spawner->Timing.WaveInterval = WaveIntervalSeconds;
	Spawner->Timing.IntervalVarianceFraction = 0.f;
	Spawner->SpawnPoints = {FVector::ZeroVector};
	Spawner->Waypoints = {FVector(PathLength, 0.f, 0.f)};
	switch (Variant)
	{
	case Attack:
		ConfigureAttackSpawner(*Spawner);
		break;
	case Patrol:
		ConfigurePatrolSpawner(*Spawner);
		break;
	case DeadBuilding:
		ConfigureDeadBuildingSpawner(*Spawner);
		break;
	default:
		ConfigureDifficultyListsSpawner(*Spawner);
		break;
	}
	Spawner->FinishSpawning(FTransform(Location));
	return Spawner;
}

void UEnemyWaveSpawnerTestSubsystem::ConfigureAttackSpawner(AEnemyWaveSpawner& Spawner) const
{
	using namespace EnemyWaveSpawnerTestPrivate;
	Spawner.SpawnerId = TEXT("Test_Attack");
	Spawner.Behaviour = EEnemyWaveSpawnerBehaviour::AttackWave;
	Spawner.Timing.Repeat = EEnemyWaveSpawnerRepeat::FixedCount;
	Spawner.Timing.WaveCount = AttackWaveCount;
	Spawner.Units.BaseUnits = {MakeEntry(GetTankOption(), AttackUnitsPerWave)};
	Spawner.Minimap.bShowOnMinimap = true;
}

void UEnemyWaveSpawnerTestSubsystem::ConfigurePatrolSpawner(AEnemyWaveSpawner& Spawner) const
{
	using namespace EnemyWaveSpawnerTestPrivate;
	Spawner.SpawnerId = TEXT("Test_Patrol");
	Spawner.Behaviour = EEnemyWaveSpawnerBehaviour::PatrolWave;
	Spawner.Timing.Repeat = EEnemyWaveSpawnerRepeat::Infinite;
	Spawner.Units.BaseUnits = {MakeEntry(GetSquadOption(), 1)};
	Spawner.Waypoints = {FVector(PathLength, 0.f, 0.f), FVector(PathLength, PathLength, 0.f)};
	Spawner.Ownership.WaveType = EEnemyWaveType::Wave_OwningBuilding;
}

void UEnemyWaveSpawnerTestSubsystem::ConfigureDeadBuildingSpawner(AEnemyWaveSpawner& Spawner) const
{
	using namespace EnemyWaveSpawnerTestPrivate;
	Spawner.SpawnerId = TEXT("Test_DeadBuilding");
	Spawner.Timing.Repeat = EEnemyWaveSpawnerRepeat::Once;
	Spawner.Units.BaseUnits = {MakeEntry(GetTankOption(), 1)};
	Spawner.Ownership.WaveType = EEnemyWaveType::Wave_OwningBuilding;
}

void UEnemyWaveSpawnerTestSubsystem::ConfigureDifficultyListsSpawner(AEnemyWaveSpawner& Spawner) const
{
	using namespace EnemyWaveSpawnerTestPrivate;
	Spawner.SpawnerId = TEXT("Test_DifficultyLists");
	Spawner.SpawnPoints = {FVector::ZeroVector, FVector(0.f, SecondSpawnPointOffset, 0.f)};
	Spawner.Units.BaseUnits = {MakeEntry(GetTankOption(), 2)};
	Spawner.Units.ExtraUnitsNormal = {MakeEntry(GetTankOption(), 1)};
	Spawner.Units.ExtraUnitsHard = {MakeEntry(GetSquadOption(), 1)};
	Spawner.Units.ExtraUnitsBrutal = {MakeEntry(GetTankOption(), 1)};
}

int32 UEnemyWaveSpawnerTestSubsystem::CountOverlappingSpawnLocations(const TArray<FAttackWaveElement>& Elements) const
{
	int32 Overlaps = 0;
	for (int32 First = 0; First < Elements.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Elements.Num(); ++Second)
		{
			const float Distance = FVector::Dist2D(Elements[First].SpawnLocation, Elements[Second].SpawnLocation);
			Overlaps += Distance < EnemyWaveSpawnerTestPrivate::DistinctSpawnTolerance ? 1 : 0;
		}
	}
	return Overlaps;
}

void UEnemyWaveSpawnerTestSubsystem::OnAttackUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units)
{
	M_AttackWaveUnitCounts.Add(Units.Num());
	for (AActor* Unit : Units)
	{
		M_AttackUnits.Add(Unit);
	}
	UE_LOG(LogRTSEnemyWaveSpawnerTest, Display, TEXT("RTS_WAVE_SPAWNER_TEST attack wave %d spawned %d units"),
	       M_AttackWaveUnitCounts.Num(), Units.Num());
}

void UEnemyWaveSpawnerTestSubsystem::OnPatrolUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units)
{
	for (AActor* Unit : Units)
	{
		M_PatrolUnits.Add(Unit);
	}
	UE_LOG(LogRTSEnemyWaveSpawnerTest, Display, TEXT("RTS_WAVE_SPAWNER_TEST patrol wave spawned %d units"),
	       Units.Num());
}

void UEnemyWaveSpawnerTestSubsystem::OnAttackFinished(AEnemyWaveSpawner* Spawner)
{
	M_AttackFinishedBroadcasts++;
}

void UEnemyWaveSpawnerTestSubsystem::OnPatrolFinished(AEnemyWaveSpawner* Spawner)
{
	M_PatrolFinishedBroadcasts++;
}

void UEnemyWaveSpawnerTestSubsystem::TickRunningWaves()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	if (not IsValid(M_AttackSpawner) || not IsValid(M_PatrolSpawner))
	{
		return;
	}
	WatchFormationsAndDirectControl();
	M_PeakAttackMarkers = FMath::Max(M_PeakAttackMarkers, M_AttackSpawner->M_MinimapMarkers.Num());
	if (not bM_HasDestroyedPatrolBuilding && M_PatrolSpawner->GetSpawnedWaveCount() >= 1
		&& GetPhaseSeconds() >= PatrolBuildingDeathAfterFirstWaveSeconds)
	{
		bM_HasDestroyedPatrolBuilding = true;
		M_PatrolBuilding->Destroy();
		UE_LOG(LogRTSEnemyWaveSpawnerTest, Display, TEXT("RTS_WAVE_SPAWNER_TEST destroyed the patrol's owning building"));
	}
	if (GetPhaseSeconds() < RunWavesSeconds)
	{
		return;
	}
	CheckRunResults();
	KillAttackUnits();
	SetPhase(EPhase::KillingAttackUnits);
}

void UEnemyWaveSpawnerTestSubsystem::CheckRunResults()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	Check(M_AttackSpawner->GetSpawnedWaveCount() == AttackWaveCount && M_AttackWaveUnitCounts.Num() == AttackWaveCount,
	      TEXT("attack_fixed_count_3"), FString::Printf(TEXT("%d waves"), M_AttackWaveUnitCounts.Num()));
	bool bAllWavesFull = not M_AttackWaveUnitCounts.IsEmpty();
	for (const int32 Count : M_AttackWaveUnitCounts)
	{
		bAllWavesFull = bAllWavesFull && Count == AttackUnitsPerWave;
	}
	Check(bAllWavesFull, TEXT("attack_units_per_wave"), FString::Printf(TEXT("expected %d each"), AttackUnitsPerWave));
	Check(not M_AttackSpawner->GetIsSpawnerRunning() && M_AttackFinishedBroadcasts == 1, TEXT("attack_finished_once"),
	      FString::Printf(TEXT("running=%d broadcasts=%d"), M_AttackSpawner->GetIsSpawnerRunning(),
	                      M_AttackFinishedBroadcasts));
	Check(bM_HasDestroyedPatrolBuilding && not M_PatrolSpawner->GetIsSpawnerRunning()
	      && M_PatrolFinishedBroadcasts == 1, TEXT("patrol_stops_when_owning_building_dies"),
	      FString::Printf(TEXT("destroyed=%d running=%d waves=%d"), bM_HasDestroyedPatrolBuilding,
	                      M_PatrolSpawner->GetIsSpawnerRunning(), M_PatrolSpawner->GetSpawnedWaveCount()));
	Check(bM_SawPatrolFormation, TEXT("patrol_units_in_patrol_formation"), TEXT(""));
	Check(IsValid(M_DeadBuildingSpawner) && not M_DeadBuildingSpawner->GetIsSpawnerRunning()
	      && M_DeadBuildingSpawner->GetSpawnedWaveCount() == 0, TEXT("dead_building_never_spawns"),
	      IsValid(M_DeadBuildingSpawner)
		      ? FString::Printf(TEXT("waves=%d"), M_DeadBuildingSpawner->GetSpawnedWaveCount())
		      : TEXT("spawner gone"));
	Check(M_AttackSpawner->M_NextMarkerIndex == AttackWaveCount && M_PeakAttackMarkers >= 1,
	      TEXT("minimap_marker_per_wave"),
	      FString::Printf(TEXT("created %d, peak alive %d"), M_AttackSpawner->M_NextMarkerIndex, M_PeakAttackMarkers));
	Check(bM_SawAttackUnitInDirectControl, TEXT("attack_units_reach_direct_control"), TEXT(""));
	CheckNothingVisibleInGame(M_AttackSpawner, TEXT("attack"));
	CheckNothingVisibleInGame(M_PatrolSpawner, TEXT("patrol"));
}

void UEnemyWaveSpawnerTestSubsystem::KillAttackUnits()
{
	int32 AliveUnits = 0;
	for (const TWeakObjectPtr<AActor>& Unit : M_AttackUnits)
	{
		AliveUnits += RTSFunctionLibrary::RTSIsValid(Unit.Get()) ? 1 : 0;
	}
	const int32 MarkersBeforeKill = M_AttackSpawner->M_MinimapMarkers.Num();
	// Without living markers here the removal check below would pass without proving anything.
	Check(MarkersBeforeKill > 0 && AliveUnits > 0, TEXT("minimap_markers_alive_before_kill"),
	      FString::Printf(TEXT("%d markers, %d living units"), MarkersBeforeKill, AliveUnits));
	const FDamageEvent DamageEvent;
	for (const TWeakObjectPtr<AActor>& Unit : M_AttackUnits)
	{
		if (Unit.IsValid())
		{
			Unit->TakeDamage(EnemyWaveSpawnerTestPrivate::KillDamage, DamageEvent, nullptr, nullptr);
		}
	}
}

void UEnemyWaveSpawnerTestSubsystem::TickKillingAttackUnits()
{
	using namespace EnemyWaveSpawnerTestPrivate;
	const int32 MarkersLeft = IsValid(M_AttackSpawner) ? M_AttackSpawner->M_MinimapMarkers.Num() : 0;
	if (MarkersLeft > 0 && GetPhaseSeconds() < KillTimeoutSeconds)
	{
		return;
	}
	Check(MarkersLeft == 0, TEXT("minimap_markers_removed_on_last_death"),
	      FString::Printf(TEXT("%d left after %.2f s"), MarkersLeft, GetPhaseSeconds()));
	Evaluate();
}

void UEnemyWaveSpawnerTestSubsystem::WatchFormationsAndDirectControl()
{
	AEnemyController* const EnemyController = M_EnemyController.Get();
	if (not IsValid(EnemyController))
	{
		return;
	}
	if (not bM_SawPatrolFormation && IsValid(EnemyController->GetEnemyFormationController()))
	{
		TArray<FFormationData> Formations;
		EnemyController->GetEnemyFormationController()->GetActiveFormationData(Formations);
		for (const FFormationData& Formation : Formations)
		{
			bM_SawPatrolFormation |= GetIsPatrolFormationOfTestUnits(Formation);
		}
	}
	UEnemyDirectControlComponent* const DirectControl = EnemyController->GetEnemyDirectControlComponent();
	if (bM_SawAttackUnitInDirectControl || not IsValid(DirectControl))
	{
		return;
	}
	for (AActor* const Registered : DirectControl->GetRegisteredDirectControlUnits())
	{
		bM_SawAttackUnitInDirectControl |= Registered != nullptr && M_AttackUnits.Contains(Registered);
	}
}

bool UEnemyWaveSpawnerTestSubsystem::GetIsPatrolFormationOfTestUnits(const FFormationData& Formation) const
{
	if (not Formation.bIsRandomPatrolWithAttackMoveFormation)
	{
		return false;
	}
	for (const FFormationUnitData& FormationUnit : Formation.FormationUnits)
	{
		const AActor* const UnitActor = FormationUnit.IsValidFormationUnit() ? FormationUnit.Unit->GetOwnerActor() : nullptr;
		if (UnitActor != nullptr && M_PatrolUnits.Contains(UnitActor))
		{
			return true;
		}
	}
	return false;
}

void UEnemyWaveSpawnerTestSubsystem::CheckNothingVisibleInGame(const AEnemyWaveSpawner* Spawner, const FString& Name)
{
	if (not IsValid(Spawner))
	{
		return;
	}
	TArray<UPrimitiveComponent*> Primitives;
	Spawner->GetComponents<UPrimitiveComponent>(Primitives);
	int32 VisibleInGame = 0;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		VisibleInGame += IsValid(Primitive) && Primitive->IsVisible() && not Primitive->bHiddenInGame ? 1 : 0;
	}
	Check(Spawner->IsHidden() && VisibleInGame == 0, TEXT("hidden_in_game_") + Name,
	      FString::Printf(TEXT("actor_hidden=%d visible_primitives=%d of %d"), Spawner->IsHidden(), VisibleInGame,
	                      Primitives.Num()));
}

bool UEnemyWaveSpawnerTestSubsystem::FindNavigableCenter(FVector& OutCenter) const
{
	using namespace EnemyWaveSpawnerTestPrivate;
	const ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	const FVector CameraLocation = IsValid(Controller) && IsValid(Controller->PlayerCameraManager)
		                               ? Controller->PlayerCameraManager->GetCameraLocation()
		                               : FVector::ZeroVector;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	FHitResult Hit;
	const FVector Top(CameraLocation.X, CameraLocation.Y, GroundTraceHalfHeight);
	const FVector Bottom(CameraLocation.X, CameraLocation.Y, -GroundTraceHalfHeight);
	const FVector Ground = GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, ObjectQuery)
		                       ? Hit.ImpactPoint
		                       : CameraLocation;
	UNavigationSystemV1* const NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (not IsValid(NavigationSystem) || not NavigationSystem->ProjectPointToNavigation(Ground, Projected, NavExtent))
	{
		return false;
	}
	OutCenter = Projected.Location;
	return true;
}

#else

void UEnemyWaveSpawnerTestSubsystem::StartTest() {}
void UEnemyWaveSpawnerTestSubsystem::CheckDifficultyUnitLists() {}
void UEnemyWaveSpawnerTestSubsystem::SpawnTestSpawners() {}
AEnemyWaveSpawner* UEnemyWaveSpawnerTestSubsystem::SpawnConfiguredSpawner(const FVector&, const int32) { return nullptr; }
void UEnemyWaveSpawnerTestSubsystem::OnAttackUnitsSpawned(AEnemyWaveSpawner*, const TArray<AActor*>&) {}
void UEnemyWaveSpawnerTestSubsystem::OnPatrolUnitsSpawned(AEnemyWaveSpawner*, const TArray<AActor*>&) {}
void UEnemyWaveSpawnerTestSubsystem::OnAttackFinished(AEnemyWaveSpawner*) {}
void UEnemyWaveSpawnerTestSubsystem::OnPatrolFinished(AEnemyWaveSpawner*) {}
void UEnemyWaveSpawnerTestSubsystem::ConfigureAttackSpawner(AEnemyWaveSpawner&) const {}
void UEnemyWaveSpawnerTestSubsystem::ConfigurePatrolSpawner(AEnemyWaveSpawner&) const {}
void UEnemyWaveSpawnerTestSubsystem::ConfigureDeadBuildingSpawner(AEnemyWaveSpawner&) const {}
void UEnemyWaveSpawnerTestSubsystem::ConfigureDifficultyListsSpawner(AEnemyWaveSpawner&) const {}
int32 UEnemyWaveSpawnerTestSubsystem::CountOverlappingSpawnLocations(const TArray<FAttackWaveElement>&) const { return 0; }
void UEnemyWaveSpawnerTestSubsystem::CheckRunResults() {}
void UEnemyWaveSpawnerTestSubsystem::KillAttackUnits() {}
bool UEnemyWaveSpawnerTestSubsystem::GetIsPatrolFormationOfTestUnits(const FFormationData&) const { return false; }
void UEnemyWaveSpawnerTestSubsystem::TickRunningWaves() {}
void UEnemyWaveSpawnerTestSubsystem::TickKillingAttackUnits() {}
void UEnemyWaveSpawnerTestSubsystem::WatchFormationsAndDirectControl() {}
void UEnemyWaveSpawnerTestSubsystem::CheckNothingVisibleInGame(const AEnemyWaveSpawner*, const FString&) {}
bool UEnemyWaveSpawnerTestSubsystem::FindNavigableCenter(FVector&) const { return false; }

#endif

void UEnemyWaveSpawnerTestSubsystem::Evaluate()
{
	if (M_Phase == EPhase::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSEnemyWaveSpawnerTest, Display, TEXT("RTS_WAVE_SPAWNER_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("EnemyWaveSpawnerTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	SetPhase(EPhase::Finished);
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEnemyWaveSpawnerTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSEnemyWaveSpawnerTest, Display, TEXT("RTS_WAVE_SPAWNER_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}
