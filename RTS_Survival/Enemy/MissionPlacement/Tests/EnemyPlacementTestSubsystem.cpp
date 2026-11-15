// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyPlacementTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "NavigationSystem.h"
#include "Engine/DamageEvents.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyBaseTrenchComponent/EnemyBaseTrenchComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyPlacementTest, Log, All);

namespace EnemyPlacementTestPrivate
{
	const TCHAR* const Switch = TEXT("EnemyPlacementValidate");
	const TCHAR* const ConvoyDefinitionPath = TEXT("/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_Fuel.DA_Convoy_Fuel");
	constexpr double StartDelaySeconds = 8.0;
	constexpr int32 TestWaveSupply = 500;

	// Scripted run: each step waits for its event (assets load slowly on a cold start), then these seconds.
	constexpr double CheckGarrisonAfterSpawnSeconds = 5.0;
	constexpr double CheckReleaseAfterKillSeconds = 2.0;
	constexpr double EndFirstConvoyAfterReleaseSeconds = 2.0;
	constexpr double CheckClearedAfterEndSeconds = 1.0;
	// After the second convoy started; long enough for its column to spawn.
	constexpr double SecondConvoyLifeSeconds = 6.0;
	constexpr double CheckRoutesAfterSecondEndSeconds = 3.0;
	constexpr double GiveUpSeconds = 120.0;

	constexpr float KillDamage = 1000000.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	const FVector NavExtent(2000.f, 2000.f, 5000.f);

	const FVector StrongpointOffset(0.f, -3000.f, 0.f);
	const FVector ReinforcementOffset(-3500.f, -6000.f, 0.f);
	const FVector ConvoyRouteOffset(-4000.f, 3000.f, 0.f);
	const FVector DeadRouteOffset(-4000.f, 9000.f, 0.f);
	constexpr float RouteProbeSpacing = 600.f;
	const FVector ConvoyExitPoint(8000.f, 0.f, 0.f);
	const FVector VehiclePostLocation(600.f, 0.f, 0.f);
	constexpr float VehiclePostYaw = 90.f;
	const FVector SquadPostLocation(-600.f, 300.f, 0.f);
	const FVector HardPostLocation(0.f, 900.f, 0.f);
	const FVector ThreatPoint(5000.f, 0.f, 0.f);
	constexpr float TrenchRadius = 3000.f;

	constexpr float PostTolerance = 300.f;
	constexpr float FacingToleranceDegrees = 25.f;
	constexpr float TrenchCentreTolerance = 10.f;
	constexpr float ThreatDirectionMinDot = 0.99f;

	constexpr int32 VehiclePostIndex = 0;
	constexpr int32 SquadPostIndex = 1;
	constexpr int32 HardPostIndex = 2;

	constexpr int32 ConvoyCount = 2;
	constexpr float ConvoyIntervalSeconds = 3.f;
	constexpr int32 TierStepPerConvoy = 1;
	constexpr int32 TierEasy = 0;
	constexpr int32 TierHard = 1;

	FEnemyPlacementUnit GetTankOption()
	{
		return FEnemyPlacementUnit::MakeVehicle(ETankSubtype::Tank_SU_85);
	}

	FEnemyPlacementUnit GetSquadOption()
	{
		return FEnemyPlacementUnit::MakeSquad(ESquadSubtype::Squad_Rus_Mosin);
	}
}

bool UEnemyPlacementTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), EnemyPlacementTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UEnemyPlacementTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyPlacementTestSubsystem, STATGROUP_Tickables);
}

void UEnemyPlacementTestSubsystem::Tick(const float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay() || M_Step == EStep::Finished)
	{
		return;
	}
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}
	if (M_TestStartSeconds < 0.0)
	{
		M_TestStartSeconds = World->GetTimeSeconds() + EnemyPlacementTestPrivate::StartDelaySeconds;
		return;
	}
	if (M_Step == EStep::WaitingForStart)
	{
		if (World->GetTimeSeconds() >= M_TestStartSeconds)
		{
			StartTest();
		}
		return;
	}
	if (GetTestSeconds() > EnemyPlacementTestPrivate::GiveUpSeconds)
	{
		Check(false, TEXT("finished_in_time"), FString::Printf(TEXT("stuck at step %d"), static_cast<int32>(M_Step)));
		Evaluate();
		return;
	}
	TickSteps();
#endif
}

double UEnemyPlacementTestSubsystem::GetTestSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_TestStartSeconds;
}

#if RTS_WITH_MAP_TESTS

bool UEnemyPlacementTestSubsystem::GetIsStepDue(const double DueSeconds)
{
	return GetTestSeconds() >= DueSeconds;
}

void UEnemyPlacementTestSubsystem::TickSteps()
{
	using namespace EnemyPlacementTestPrivate;
	switch (M_Step)
	{
	case EStep::CheckGarrison:
		if (M_GarrisonSpawnedSeconds >= 0.0 && GetIsStepDue(M_GarrisonSpawnedSeconds + CheckGarrisonAfterSpawnSeconds))
		{
			CheckGarrison();
			AdvanceStep(EStep::KillConvoyTruck);
		}
		break;
	case EStep::KillConvoyTruck:
		if (GetHasLivingTruckOrFinishedFirstConvoy())
		{
			KillFirstConvoyTruck();
			AdvanceStep(EStep::CheckReleaseAndReinforcements);
		}
		break;
	case EStep::CheckReleaseAndReinforcements:
		if (GetIsStepDue(M_LastStepSeconds + CheckReleaseAfterKillSeconds))
		{
			CheckReleaseAndReinforcements();
			AdvanceStep(EStep::EndFirstConvoy);
		}
		break;
	case EStep::EndFirstConvoy:
		if (GetIsStepDue(M_LastStepSeconds + EndFirstConvoyAfterReleaseSeconds))
		{
			EndNewestConvoy();
			AdvanceStep(EStep::CheckCleared);
		}
		break;
	case EStep::CheckCleared:
		if (GetIsStepDue(M_LastStepSeconds + CheckClearedAfterEndSeconds))
		{
			CheckCleared();
			AdvanceStep(EStep::EndSecondConvoy);
		}
		break;
	case EStep::EndSecondConvoy:
		if (M_SecondConvoyStartSeconds >= 0.0 && GetIsStepDue(M_SecondConvoyStartSeconds + SecondConvoyLifeSeconds))
		{
			EndNewestConvoy();
			AdvanceStep(EStep::CheckRoutes);
		}
		break;
	case EStep::CheckRoutes:
		if (GetIsStepDue(M_LastStepSeconds + CheckRoutesAfterSecondEndSeconds))
		{
			CheckRoutes();
			Evaluate();
		}
		break;
	default:
		break;
	}
}

void UEnemyPlacementTestSubsystem::AdvanceStep(const EStep NextStep)
{
	M_Step = NextStep;
	M_LastStepSeconds = GetTestSeconds();
}

bool UEnemyPlacementTestSubsystem::GetHasLivingTruckOrFinishedFirstConvoy() const
{
	if (not M_ConvoyOutcomes.IsEmpty())
	{
		return true;
	}
	if (M_StartedConvoys.IsEmpty() || not M_StartedConvoys[0].IsValid())
	{
		return false;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	M_StartedConvoys[0]->GetMemberSnapshots(Snapshots);
	return Snapshots.ContainsByPredicate([](const FConvoyMemberSnapshot& Snapshot)
	{
		return Snapshot.Role == EConvoyUnitRole::Truck && Snapshot.Actor.IsValid();
	});
}

void UEnemyPlacementTestSubsystem::StartTest()
{
	using namespace EnemyPlacementTestPrivate;
	AEnemyController* const EnemyController = FRTS_Statics::GetEnemyController(this);
	const bool bHasCenter = FindNavigableCenter(M_Center);
	const bool bHasDifficulty = EnemyPlacement::TryGetGameDifficulty(this, M_Difficulty);
	M_ConvoyDefinition = LoadObject<UConvoyDefinition>(nullptr, ConvoyDefinitionPath);
	Check(IsValid(EnemyController) && bHasCenter && bHasDifficulty && IsValid(M_ConvoyDefinition), TEXT("setup"),
	      FString::Printf(TEXT("enemy_controller=%d nav_center=%d difficulty=%d convoy_definition=%d"),
	                      IsValid(EnemyController), bHasCenter, bHasDifficulty, IsValid(M_ConvoyDefinition)));
	if (not IsValid(EnemyController) || not bHasCenter || not IsValid(M_ConvoyDefinition))
	{
		Evaluate();
		return;
	}
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST difficulty %s"),
	       *UEnum::GetValueAsString(M_Difficulty));
	M_EnemyController = EnemyController;
	EnemyController->SetEnemyWaveSupply(TestWaveSupply);
	SpawnReinforcementSpawner();
	SpawnStrongpoint();
	SpawnConvoyRoutes();
	M_Step = EStep::CheckGarrison;
}

// ------------------------------------------------------------------
// Strongpoint
// ------------------------------------------------------------------

void UEnemyPlacementTestSubsystem::SpawnReinforcementSpawner()
{
	using namespace EnemyPlacementTestPrivate;
	const FVector Location = M_Center + ReinforcementOffset;
	AEnemyWaveSpawner* const Spawner = GetWorld()->SpawnActorDeferred<AEnemyWaveSpawner>(
		AEnemyWaveSpawner::StaticClass(), FTransform(Location));
	if (not IsValid(Spawner))
	{
		return;
	}
	Spawner->SpawnerId = TEXT("Test_Reinforcements");
	Spawner->Timing.bStartOnBeginPlay = false;
	Spawner->Timing.FirstWaveDelay = 0.f;
	Spawner->SpawnPoints = {FVector::ZeroVector};
	Spawner->Waypoints = {M_Center + StrongpointOffset - Location};
	FEnemyWaveSpawnerUnitEntry Entry;
	Entry.UnitOptions = {GetTankOption()};
	Spawner->Units.BaseUnits = {Entry};
	Spawner->FinishSpawning(FTransform(Location));
	M_ReinforcementSpawner = Spawner;
}

void UEnemyPlacementTestSubsystem::SpawnStrongpoint()
{
	using namespace EnemyPlacementTestPrivate;
	const FVector Location = M_Center + StrongpointOffset;
	AEnemyStrongpoint* const Strongpoint = GetWorld()->SpawnActorDeferred<AEnemyStrongpoint>(
		AEnemyStrongpoint::StaticClass(), FTransform(Location));
	if (not IsValid(Strongpoint))
	{
		Check(false, TEXT("spawn_strongpoint"), TEXT("could not spawn"));
		return;
	}
	ConfigureStrongpoint(*Strongpoint);
	Strongpoint->FinishSpawning(FTransform(Location));
	Strongpoint->OnGarrisonSpawned.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnGarrisonSpawned);
	Strongpoint->OnGarrisonReleased.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnGarrisonReleased);
	Strongpoint->OnReinforcementsCalled.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnReinforcementsCalled);
	Strongpoint->OnStrongpointCleared.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnStrongpointCleared);
	M_Strongpoint = Strongpoint;
	Strongpoint->SpawnGarrison();
	// A second call must not spawn a second garrison or add a second trench base.
	Strongpoint->SpawnGarrison();
	Check(AEnemyStrongpoint::FindEnemyStrongpoint(this, TEXT("Test_Strongpoint")) == Strongpoint,
	      TEXT("find_strongpoint_by_id"), TEXT("Test_Strongpoint"));
}

void UEnemyPlacementTestSubsystem::ConfigureStrongpoint(AEnemyStrongpoint& Strongpoint) const
{
	using namespace EnemyPlacementTestPrivate;
	Strongpoint.StrongpointId = TEXT("Test_Strongpoint");
	Strongpoint.Timing.bSpawnOnBeginPlay = false;
	Strongpoint.Timing.SpawnDelay = 0.f;
	Strongpoint.Timing.DigInDelay = 1.f;

	FEnemyStrongpointPost VehiclePost;
	VehiclePost.Post = FTransform(FRotator(0.f, VehiclePostYaw, 0.f), VehiclePostLocation);
	VehiclePost.UnitOptions = {GetTankOption()};
	VehiclePost.bDigIn = true;

	FEnemyStrongpointPost SquadPost;
	SquadPost.Post.SetLocation(SquadPostLocation);
	SquadPost.UnitOptions = {GetSquadOption()};
	SquadPost.bDigIn = false;
	// Held exactly on the post; with cover on it may settle beside the dug-in vehicle instead.
	SquadPost.bTakeNearbyCover = false;

	// Manned only from Ironman up; on the test's own difficulty it must stay empty unless that is Ironman.
	FEnemyStrongpointPost HardPost;
	HardPost.Post.SetLocation(HardPostLocation);
	HardPost.UnitOptions = {GetTankOption()};
	HardPost.FromDifficulty = ERTSGameDifficulty::Ironman;
	HardPost.bDigIn = true;

	Strongpoint.Posts = {VehiclePost, SquadPost, HardPost};
	Strongpoint.Release.Release = EEnemyStrongpointRelease::OnLosses;
	Strongpoint.Release.LossFraction = 0.5f;
	Strongpoint.Release.bReleaseDugInDefenders = false;
	Strongpoint.Reinforcements.Spawners = {M_ReinforcementSpawner};
	Strongpoint.Reinforcements.LossFraction = 0.25f;
	Strongpoint.Trenches.bDigTrenches = true;
	Strongpoint.Trenches.PerimeterRadius = TrenchRadius;
	Strongpoint.Trenches.bFaceThreatPoint = true;
	Strongpoint.Trenches.ThreatPoint = ThreatPoint;
	Strongpoint.Minimap.bShowOnMinimap = true;
}

void UEnemyPlacementTestSubsystem::OnGarrisonSpawned(AEnemyStrongpoint* Strongpoint, const TArray<AActor*>& Units)
{
	M_GarrisonSpawnedBroadcasts++;
	M_GarrisonSpawnedSeconds = GetTestSeconds();
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST garrison spawned %d units"), Units.Num());
}

void UEnemyPlacementTestSubsystem::OnGarrisonReleased(AEnemyStrongpoint* Strongpoint, const TArray<AActor*>& Units)
{
	M_ReleasedUnitCount += Units.Num();
	const AEnemyController* const EnemyController = M_EnemyController.Get();
	const UEnemyDirectControlComponent* const DirectControl = IsValid(EnemyController)
		                                                          ? EnemyController->GetEnemyDirectControlComponent()
		                                                          : nullptr;
	if (not IsValid(DirectControl))
	{
		return;
	}
	// Checked now: the strategic AI may pick the released units for an action (out of the idle list) right after.
	const TArray<AActor*> Registered = DirectControl->GetRegisteredDirectControlUnits();
	for (AActor* const Unit : Units)
	{
		M_ReleasedUnitsRegistered += Registered.Contains(Unit) ? 1 : 0;
		bM_ReleasedSquad |= Unit != nullptr && Unit->IsA<ASquadController>();
	}
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST garrison released %d units"), Units.Num());
}

void UEnemyPlacementTestSubsystem::OnReinforcementsCalled(AEnemyStrongpoint* Strongpoint)
{
	M_ReinforcementsBroadcasts++;
}

void UEnemyPlacementTestSubsystem::OnStrongpointCleared(AEnemyStrongpoint* Strongpoint)
{
	M_ClearedBroadcasts++;
}

void UEnemyPlacementTestSubsystem::CheckGarrison()
{
	using namespace EnemyPlacementTestPrivate;
	if (not IsValid(M_Strongpoint))
	{
		Check(false, TEXT("strongpoint_alive"), TEXT("gone"));
		return;
	}
	M_ExpectedDefenders = M_Difficulty == ERTSGameDifficulty::Ironman ? 3 : 2;
	Check(M_GarrisonSpawnedBroadcasts == 1 && M_Strongpoint->GetDefenderCount() == M_ExpectedDefenders,
	      TEXT("garrison_per_difficulty"),
	      FString::Printf(TEXT("broadcasts=%d defenders=%d expected=%d"), M_GarrisonSpawnedBroadcasts,
	                      M_Strongpoint->GetDefenderCount(), M_ExpectedDefenders));
	for (const FEnemyStrongpointDefender& Defender : M_Strongpoint->M_Defenders)
	{
		CheckDefenderOnPost(Defender.Unit.Get(), Defender.PostIndex);
	}
	CheckDigIn();
	CheckTrenchBase();
	Check(not M_Strongpoint->M_MinimapIconId.IsNone(), TEXT("strongpoint_minimap_icon"),
	      M_Strongpoint->M_MinimapIconId.ToString());
	Check(M_ReinforcementsBroadcasts == 0 && M_ReleasedUnitCount == 0, TEXT("nothing_released_before_losses"),
	      FString::Printf(TEXT("reinforcements=%d released=%d"), M_ReinforcementsBroadcasts, M_ReleasedUnitCount));
	CheckNothingVisibleInGame(M_Strongpoint, TEXT("strongpoint"));
	CheckNothingVisibleInGame(M_ConvoyRoute, TEXT("convoy_route"));
	// Dug-in defenders die first: losses reach the reinforcement and release fractions, the squad is released.
	KillDefenders(true);
}

void UEnemyPlacementTestSubsystem::CheckDefenderOnPost(const AActor* Defender, const int32 PostIndex)
{
	using namespace EnemyPlacementTestPrivate;
	if (not IsValid(Defender) || not M_Strongpoint->Posts.IsValidIndex(PostIndex))
	{
		Check(false, FString::Printf(TEXT("defender_post_%d"), PostIndex), TEXT("no defender"));
		return;
	}
	const FEnemyStrongpointPost& StrongpointPost = M_Strongpoint->Posts[PostIndex];
	const FVector PostLocation = M_Strongpoint->GetPostWorldLocation(StrongpointPost);
	FVector DefenderLocation = Defender->GetActorLocation();
	FString Diagnostics;
	if (ASquadController* const Squad = Cast<ASquadController>(const_cast<AActor*>(Defender)))
	{
		// A squad stands where its soldiers are; the controller actor itself may stay at the spawn point.
		DefenderLocation = Squad->GetSquadLocation();
		const AEnemyController* const EnemyController = M_EnemyController.Get();
		const UEnemyBaseTrenchComponent* const BaseTrenches = IsValid(EnemyController)
			                                                      ? EnemyController->GetEnemyBaseTrenchComponent()
			                                                      : nullptr;
		Diagnostics = FString::Printf(TEXT(" controller %.0f cm away, command %s, offset from post %s, trenches %d"),
		                              FVector::Dist2D(PostLocation, Defender->GetActorLocation()),
		                              *UEnum::GetValueAsString(Squad->GetActiveCommandID()),
		                              *(DefenderLocation - PostLocation).ToCompactString(),
		                              IsValid(BaseTrenches) ? BaseTrenches->GetDugTrenches().Num() : -1);
	}
	const float Distance = FVector::Dist2D(PostLocation, DefenderLocation);
	Check(Distance <= PostTolerance, FString::Printf(TEXT("defender_on_post_%d"), PostIndex),
	      FString::Printf(TEXT("%.0f cm from the post%s"), Distance, *Diagnostics));
	if (not Defender->IsA<ATankMaster>())
	{
		return;
	}
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Defender->GetActorRotation().Yaw,
	                                                               M_Strongpoint->GetPostWorldYaw(StrongpointPost)));
	Check(YawError <= FacingToleranceDegrees, FString::Printf(TEXT("defender_faces_post_%d"), PostIndex),
	      FString::Printf(TEXT("%.1f deg off"), YawError));
}

void UEnemyPlacementTestSubsystem::CheckDigIn()
{
	for (const FEnemyStrongpointDefender& Defender : M_Strongpoint->M_Defenders)
	{
		const ATrackedTankMaster* const Tank = Cast<ATrackedTankMaster>(Defender.Unit.Get());
		if (Defender.PostIndex != EnemyPlacementTestPrivate::VehiclePostIndex || Tank == nullptr)
		{
			continue;
		}
		const UDigInComponent* const DigIn = Tank->GetDigInComponent();
		Check(IsValid(DigIn) && DigIn->GetIsDiggingInOrDugIn(), TEXT("vehicle_post_digs_in"),
		      IsValid(DigIn) ? TEXT("") : TEXT("no dig-in component"));
		return;
	}
	Check(false, TEXT("vehicle_post_digs_in"), TEXT("no tracked tank on the vehicle post"));
}

void UEnemyPlacementTestSubsystem::CheckTrenchBase()
{
	using namespace EnemyPlacementTestPrivate;
	const AEnemyController* const EnemyController = M_EnemyController.Get();
	const UEnemyBaseTrenchComponent* const BaseTrenches = IsValid(EnemyController)
		                                                      ? EnemyController->GetEnemyBaseTrenchComponent()
		                                                      : nullptr;
	if (not IsValid(BaseTrenches))
	{
		Check(false, TEXT("trench_base_added_once"), TEXT("no base trench component"));
		return;
	}
	int32 MatchingBases = 0;
	float BestDot = -1.f;
	for (const FEnemyTrenchBase& Base : BaseTrenches->GetBases())
	{
		if (not Base.bIsAddedByDesigner
			|| FVector::Dist2D(Base.Centre, M_Strongpoint->GetActorLocation()) > TrenchCentreTolerance)
		{
			continue;
		}
		MatchingBases++;
		BestDot = FMath::Max(BestDot, static_cast<float>(FVector2D::DotProduct(Base.ThreatDirection,
		                                                                        FVector2D(1.f, 0.f))));
	}
	Check(MatchingBases == 1 && BestDot >= ThreatDirectionMinDot && BaseTrenches->GetIsDigging(),
	      TEXT("trench_base_added_once"),
	      FString::Printf(TEXT("bases=%d facing_dot=%.3f digging=%d"), MatchingBases, BestDot,
	                      BaseTrenches->GetIsDigging()));
}

void UEnemyPlacementTestSubsystem::KillUnit(AActor* Unit) const
{
	if (not RTSFunctionLibrary::RTSIsValid(Unit))
	{
		return;
	}
	const FDamageEvent DamageEvent;
	if (ASquadController* const Squad = Cast<ASquadController>(Unit))
	{
		for (ASquadUnit* const Soldier : Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier))
			{
				Soldier->TakeDamage(EnemyPlacementTestPrivate::KillDamage, DamageEvent, nullptr, nullptr);
			}
		}
		return;
	}
	Unit->TakeDamage(EnemyPlacementTestPrivate::KillDamage, DamageEvent, nullptr, nullptr);
}

void UEnemyPlacementTestSubsystem::KillDefenders(const bool bOnlyDugIn)
{
	if (not IsValid(M_Strongpoint))
	{
		return;
	}
	for (const FEnemyStrongpointDefender& Defender : M_Strongpoint->M_Defenders)
	{
		if (not bOnlyDugIn || Defender.bDigsIn)
		{
			KillUnit(Defender.Unit.Get());
		}
	}
}

void UEnemyPlacementTestSubsystem::CheckReleaseAndReinforcements()
{
	if (not IsValid(M_Strongpoint))
	{
		return;
	}
	Check(M_ReinforcementsBroadcasts == 1 && IsValid(M_ReinforcementSpawner)
	      && (M_ReinforcementSpawner->GetIsSpawnerRunning() || M_ReinforcementSpawner->GetSpawnedWaveCount() > 0),
	      TEXT("reinforcements_called_on_losses"),
	      FString::Printf(TEXT("broadcasts=%d spawner_running=%d"), M_ReinforcementsBroadcasts,
	                      IsValid(M_ReinforcementSpawner) && M_ReinforcementSpawner->GetIsSpawnerRunning()));
	Check(M_ReleasedUnitCount == 1 && M_ReleasedUnitsRegistered == 1 && bM_ReleasedSquad,
	      TEXT("survivors_released_to_direct_control"),
	      FString::Printf(TEXT("released=%d registered_at_release=%d squad=%d"), M_ReleasedUnitCount,
	                      M_ReleasedUnitsRegistered, bM_ReleasedSquad));
	KillDefenders(false);
}

void UEnemyPlacementTestSubsystem::CheckCleared()
{
	if (not IsValid(M_Strongpoint))
	{
		return;
	}
	Check(M_ClearedBroadcasts == 1 && M_Strongpoint->GetIsCleared() && M_Strongpoint->M_MinimapIconId.IsNone(),
	      TEXT("strongpoint_cleared_once"),
	      FString::Printf(TEXT("broadcasts=%d alive=%d icon=%s"), M_ClearedBroadcasts,
	                      M_Strongpoint->GetAliveDefenderCount(), *M_Strongpoint->M_MinimapIconId.ToString()));
}

// ------------------------------------------------------------------
// Convoy routes
// ------------------------------------------------------------------

void UEnemyPlacementTestSubsystem::SpawnConvoyRoutes()
{
	using namespace EnemyPlacementTestPrivate;
	FVector RouteStart = M_Center + ConvoyRouteOffset;
	const bool bHasDrivableRoute = FindDrivableRouteStart(RouteStart);
	Check(bHasDrivableRoute, TEXT("test_map_has_drivable_route"), RouteStart.ToCompactString());
	M_ConvoyRoute = SpawnConfiguredRoute(RouteStart, TEXT("Test_Convoy"));
	M_DeadBuildingRoute = SpawnConfiguredRoute(M_Center + DeadRouteOffset, TEXT("Test_DeadBuildingConvoy"));
	if (not IsValid(M_ConvoyRoute) || not IsValid(M_DeadBuildingRoute))
	{
		Check(false, TEXT("spawn_convoy_routes"), TEXT("could not spawn"));
		return;
	}
	AActor* const DeadBuilding = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(M_Center));
	M_DeadBuildingRoute->Ownership.bHasOwningBuilding = true;
	M_DeadBuildingRoute->Ownership.OwningBuilding = DeadBuilding;
	if (IsValid(DeadBuilding))
	{
		DeadBuilding->Destroy();
	}
	M_ConvoyRoute->OnConvoyStarted.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnConvoyStarted);
	M_ConvoyRoute->OnConvoyFinished.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnConvoyFinished);
	M_ConvoyRoute->OnRouteFinished.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnRouteFinished);
	M_DeadBuildingRoute->OnRouteFinished.AddDynamic(this, &UEnemyPlacementTestSubsystem::OnDeadRouteFinished);
	M_ConvoyRoute->StartRoute();
	M_DeadBuildingRoute->StartRoute();
	Check(AEnemyConvoyRoute::FindEnemyConvoyRoute(this, TEXT("Test_Convoy")) == M_ConvoyRoute,
	      TEXT("find_convoy_route_by_id"), TEXT("Test_Convoy"));
}

AEnemyConvoyRoute* UEnemyPlacementTestSubsystem::SpawnConfiguredRoute(const FVector& Location, const FName RouteId)
{
	using namespace EnemyPlacementTestPrivate;
	AEnemyConvoyRoute* const Route = GetWorld()->SpawnActorDeferred<AEnemyConvoyRoute>(
		AEnemyConvoyRoute::StaticClass(), FTransform(Location));
	if (not IsValid(Route))
	{
		return nullptr;
	}
	Route->RouteId = RouteId;
	Route->Definition = M_ConvoyDefinition;
	Route->RoutePoints = {FVector::ZeroVector, ConvoyExitPoint};
	Route->Timing.bStartOnBeginPlay = false;
	Route->Timing.FirstConvoyDelay = 0.f;
	Route->Timing.Repeat = EEnemyWaveSpawnerRepeat::FixedCount;
	Route->Timing.ConvoyCount = ConvoyCount;
	Route->Timing.bWaitForPreviousConvoy = true;
	Route->Timing.ConvoyInterval = ConvoyIntervalSeconds;
	Route->Escort.GuardTierNewToRTS = TierEasy;
	Route->Escort.GuardTierNormal = TierEasy;
	Route->Escort.GuardTierHard = TierHard;
	Route->Escort.GuardTierBrutal = TierHard;
	Route->Escort.GuardTierStepPerConvoy = TierStepPerConvoy;
	Route->FinishSpawning(FTransform(Location));
	return Route;
}

bool UEnemyPlacementTestSubsystem::FindDrivableRouteStart(FVector& InOutStart) const
{
	using namespace EnemyPlacementTestPrivate;
	// The map's navmesh decides which stretches a heavy vehicle can drive end to end; try a few around the centre.
	const FVector Candidates[] = {
		ConvoyRouteOffset, FVector(-4000.f, 6000.f, 0.f), FVector(-4000.f, -6000.f, 0.f), FVector(-8000.f, 0.f, 0.f)
	};
	for (const FVector& Offset : Candidates)
	{
		const FVector Start = M_Center + Offset;
		bool bReachedEnd = false;
		const FConvoyRoute Route = UConvoyRouteLibrary::BuildNavigableRoute(
			GetWorld(), {Start, Start + ConvoyExitPoint}, RouteProbeSpacing, NAME_None, bReachedEnd);
		if (bReachedEnd && Route.Points.Num() >= 2)
		{
			InOutStart = Start;
			return true;
		}
	}
	return false;
}

int32 UEnemyPlacementTestSubsystem::GetExpectedGuardTier(const int32 ConvoyIndex) const
{
	using namespace EnemyPlacementTestPrivate;
	// Below the definition's last tier on every difficulty, so the second convoy proves the step per convoy.
	const int32 BaseTier = EnemyPlacement::GetIsDifficultyIncluded(M_Difficulty, ERTSGameDifficulty::Hard)
		                       ? TierHard
		                       : TierEasy;
	const int32 LastTier = FMath::Max(M_ConvoyDefinition->GuardTiers.Num() - 1, 0);
	return FMath::Clamp(BaseTier + TierStepPerConvoy * ConvoyIndex, 0, LastTier);
}

void UEnemyPlacementTestSubsystem::OnConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy)
{
	M_StartedConvoys.Add(Convoy);
	M_StartedGuardTiers.Add(IsValid(Convoy) ? Convoy->GetStartParams().GuardTierIndex : INDEX_NONE);
	M_StartedTruckCounts.Add(IsValid(Convoy) ? M_ConvoyDefinition->GetTruckColumn(M_StartedGuardTiers.Last()).Num() : 0);
	if (M_StartedConvoys.Num() == 2)
	{
		M_SecondConvoyStartSeconds = GetTestSeconds();
	}
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST convoy %d started, tier %d"),
	       M_StartedConvoys.Num(), M_StartedGuardTiers.Last());
}

void UEnemyPlacementTestSubsystem::OnConvoyFinished(AEnemyConvoyRoute* Route, AConvoy* Convoy, EConvoyOutcome Outcome)
{
	M_ConvoyOutcomes.Add(Outcome);
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST convoy finished: %s"),
	       *ConvoyNames::GetOutcomeName(Outcome));
}

void UEnemyPlacementTestSubsystem::OnRouteFinished(AEnemyConvoyRoute* Route)
{
	M_RouteFinishedBroadcasts++;
}

void UEnemyPlacementTestSubsystem::OnDeadRouteFinished(AEnemyConvoyRoute* Route)
{
	M_DeadRouteFinishedBroadcasts++;
}

void UEnemyPlacementTestSubsystem::KillFirstConvoyTruck()
{
	const bool bHasConvoy = M_StartedConvoys.Num() == 1 && M_StartedConvoys[0].IsValid();
	Check(bHasConvoy && IsValid(M_ConvoyRoute) && M_ConvoyRoute->bM_HasBuiltRoute, TEXT("first_convoy_started"),
	      FString::Printf(TEXT("started=%d route_built=%d"), M_StartedConvoys.Num(),
	                      IsValid(M_ConvoyRoute) && M_ConvoyRoute->bM_HasBuiltRoute));
	if (not bHasConvoy)
	{
		return;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	M_StartedConvoys[0]->GetMemberSnapshots(Snapshots);
	for (const FConvoyMemberSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.Role == EConvoyUnitRole::Truck && Snapshot.Actor.IsValid())
		{
			KillUnit(Snapshot.Actor.Get());
			M_KilledTrucksInFirstConvoy++;
			return;
		}
	}
	Check(false, TEXT("convoy_truck_to_kill"), TEXT("no living truck"));
}

void UEnemyPlacementTestSubsystem::EndNewestConvoy()
{
	if (M_ConvoyOutcomes.Num() == M_StartedConvoys.Num() && not M_StartedConvoys.IsEmpty())
	{
		// The newest convoy already reached its exit; nothing to end.
		return;
	}
	if (M_StartedConvoys.IsEmpty() || not M_StartedConvoys.Last().IsValid())
	{
		Check(false, FString::Printf(TEXT("convoy_%d_to_end"), M_StartedConvoys.Num()), TEXT("no convoy"));
		return;
	}
	M_StartedConvoys.Last()->EndConvoy(true);
}

void UEnemyPlacementTestSubsystem::CheckRoutes()
{
	using namespace EnemyPlacementTestPrivate;
	if (not IsValid(M_ConvoyRoute))
	{
		Check(false, TEXT("convoy_route_alive"), TEXT("gone"));
		return;
	}
	Check(M_ConvoyRoute->GetStartedConvoyCount() == ConvoyCount && M_StartedGuardTiers.Num() == ConvoyCount,
	      TEXT("convoy_fixed_count_2_waits_for_previous"),
	      FString::Printf(TEXT("started=%d"), M_ConvoyRoute->GetStartedConvoyCount()));
	for (int32 ConvoyIndex = 0; ConvoyIndex < M_StartedGuardTiers.Num(); ++ConvoyIndex)
	{
		Check(M_StartedGuardTiers[ConvoyIndex] == GetExpectedGuardTier(ConvoyIndex),
		      FString::Printf(TEXT("convoy_%d_guard_tier"), ConvoyIndex + 1),
		      FString::Printf(TEXT("%d (expected %d of %d tiers)"), M_StartedGuardTiers[ConvoyIndex],
		                      GetExpectedGuardTier(ConvoyIndex), M_ConvoyDefinition->GuardTiers.Num()));
	}
	int32 ExpectedEscapedTrucks = 0;
	bool bOutcomesValid = M_ConvoyOutcomes.Num() == ConvoyCount && M_StartedTruckCounts.Num() == ConvoyCount;
	for (int32 ConvoyIndex = 0; bOutcomesValid && ConvoyIndex < ConvoyCount; ++ConvoyIndex)
	{
		const EConvoyOutcome Outcome = M_ConvoyOutcomes[ConvoyIndex];
		bOutcomesValid = Outcome == EConvoyOutcome::Aborted || Outcome == EConvoyOutcome::Escaped;
		const int32 KilledTrucks = ConvoyIndex == 0 ? M_KilledTrucksInFirstConvoy : 0;
		ExpectedEscapedTrucks += Outcome == EConvoyOutcome::Escaped ? M_StartedTruckCounts[ConvoyIndex] - KilledTrucks : 0;
	}
	Check(bOutcomesValid && M_RouteFinishedBroadcasts == 1 && not M_ConvoyRoute->GetIsRouteRunning(),
	      TEXT("convoy_route_finishes_once"),
	      FString::Printf(TEXT("outcomes=%d route_finished=%d running=%d"), M_ConvoyOutcomes.Num(),
	                      M_RouteFinishedBroadcasts, M_ConvoyRoute->GetIsRouteRunning()));
	Check(M_ConvoyRoute->GetDestroyedTruckCount() == M_KilledTrucksInFirstConvoy && M_KilledTrucksInFirstConvoy == 1
	      && M_ConvoyRoute->GetEscapedTruckCount() == ExpectedEscapedTrucks, TEXT("convoy_truck_tally"),
	      FString::Printf(TEXT("destroyed=%d escaped=%d (expected %d; first convoy %s)"),
	                      M_ConvoyRoute->GetDestroyedTruckCount(), M_ConvoyRoute->GetEscapedTruckCount(),
	                      ExpectedEscapedTrucks,
	                      M_ConvoyOutcomes.IsEmpty() ? TEXT("?") : *ConvoyNames::GetOutcomeName(M_ConvoyOutcomes[0])));
	Check(IsValid(M_DeadBuildingRoute) && M_DeadBuildingRoute->GetStartedConvoyCount() == 0
	      && M_DeadRouteFinishedBroadcasts == 1, TEXT("dead_owning_building_starts_no_convoy"),
	      IsValid(M_DeadBuildingRoute)
		      ? FString::Printf(TEXT("started=%d finished=%d"), M_DeadBuildingRoute->GetStartedConvoyCount(),
		                        M_DeadRouteFinishedBroadcasts)
		      : TEXT("route gone"));
}

void UEnemyPlacementTestSubsystem::CheckNothingVisibleInGame(const AActor* PlacedActor, const FString& Name)
{
	if (not IsValid(PlacedActor))
	{
		Check(false, TEXT("hidden_in_game_") + Name, TEXT("actor gone"));
		return;
	}
	TArray<UPrimitiveComponent*> Primitives;
	PlacedActor->GetComponents<UPrimitiveComponent>(Primitives);
	int32 VisibleInGame = 0;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		VisibleInGame += IsValid(Primitive) && Primitive->IsVisible() && not Primitive->bHiddenInGame ? 1 : 0;
	}
	Check(PlacedActor->IsHidden() && VisibleInGame == 0, TEXT("hidden_in_game_") + Name,
	      FString::Printf(TEXT("actor_hidden=%d visible_primitives=%d of %d"), PlacedActor->IsHidden(), VisibleInGame,
	                      Primitives.Num()));
}

bool UEnemyPlacementTestSubsystem::FindNavigableCenter(FVector& OutCenter) const
{
	using namespace EnemyPlacementTestPrivate;
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

bool UEnemyPlacementTestSubsystem::GetIsStepDue(const double) { return false; }
void UEnemyPlacementTestSubsystem::TickSteps() {}
void UEnemyPlacementTestSubsystem::AdvanceStep(const EStep) {}
bool UEnemyPlacementTestSubsystem::GetHasLivingTruckOrFinishedFirstConvoy() const { return false; }
void UEnemyPlacementTestSubsystem::StartTest() {}
void UEnemyPlacementTestSubsystem::SpawnReinforcementSpawner() {}
void UEnemyPlacementTestSubsystem::SpawnStrongpoint() {}
void UEnemyPlacementTestSubsystem::ConfigureStrongpoint(AEnemyStrongpoint&) const {}
void UEnemyPlacementTestSubsystem::OnGarrisonSpawned(AEnemyStrongpoint*, const TArray<AActor*>&) {}
void UEnemyPlacementTestSubsystem::OnGarrisonReleased(AEnemyStrongpoint*, const TArray<AActor*>&) {}
void UEnemyPlacementTestSubsystem::OnReinforcementsCalled(AEnemyStrongpoint*) {}
void UEnemyPlacementTestSubsystem::OnStrongpointCleared(AEnemyStrongpoint*) {}
void UEnemyPlacementTestSubsystem::CheckGarrison() {}
void UEnemyPlacementTestSubsystem::CheckDefenderOnPost(const AActor*, const int32) {}
void UEnemyPlacementTestSubsystem::CheckDigIn() {}
void UEnemyPlacementTestSubsystem::CheckTrenchBase() {}
void UEnemyPlacementTestSubsystem::KillUnit(AActor*) const {}
void UEnemyPlacementTestSubsystem::KillDefenders(const bool) {}
void UEnemyPlacementTestSubsystem::CheckReleaseAndReinforcements() {}
void UEnemyPlacementTestSubsystem::CheckCleared() {}
void UEnemyPlacementTestSubsystem::SpawnConvoyRoutes() {}
AEnemyConvoyRoute* UEnemyPlacementTestSubsystem::SpawnConfiguredRoute(const FVector&, const FName) { return nullptr; }
bool UEnemyPlacementTestSubsystem::FindDrivableRouteStart(FVector&) const { return false; }
int32 UEnemyPlacementTestSubsystem::GetExpectedGuardTier(const int32) const { return 0; }
void UEnemyPlacementTestSubsystem::OnConvoyStarted(AEnemyConvoyRoute*, AConvoy*) {}
void UEnemyPlacementTestSubsystem::OnConvoyFinished(AEnemyConvoyRoute*, AConvoy*, EConvoyOutcome) {}
void UEnemyPlacementTestSubsystem::OnRouteFinished(AEnemyConvoyRoute*) {}
void UEnemyPlacementTestSubsystem::OnDeadRouteFinished(AEnemyConvoyRoute*) {}
void UEnemyPlacementTestSubsystem::KillFirstConvoyTruck() {}
void UEnemyPlacementTestSubsystem::EndNewestConvoy() {}
void UEnemyPlacementTestSubsystem::CheckRoutes() {}
void UEnemyPlacementTestSubsystem::CheckNothingVisibleInGame(const AActor*, const FString&) {}
bool UEnemyPlacementTestSubsystem::FindNavigableCenter(FVector&) const { return false; }

#endif

void UEnemyPlacementTestSubsystem::Evaluate()
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("EnemyPlacementTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	M_Step = EStep::Finished;
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEnemyPlacementTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSEnemyPlacementTest, Display, TEXT("RTS_PLACEMENT_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}
