// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ForgeStationTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "UnrealClient.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/Missions/Berlin/BerlinMissionForcesData.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/BerlinForgeStation.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/Mission_BerlinForgeStation.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerAirDropSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSForgeStationTest, Log, All);

namespace ForgeStationTestPrivate
{
	const TCHAR* Switch = TEXT("ForgeStationValidate");
	constexpr double StartDelaySeconds = 3.0;
	constexpr double GiveUpSeconds = 240.0;
	// Test seconds at which each step runs.
	constexpr double CheckGarrisonAt = 15.0;
	constexpr double WatchWorkUntil = 55.0;
	constexpr double CheckPlateAt = 60.0;
	constexpr double KillTargetsAt = 62.0;
	constexpr double CheckWrecksAt = 66.0;
	// The Blueprint reward: 1 s to the intel, 8 s more to the drop, then complete (BerlinMissionScripting.md 5).
	constexpr double CheckCompleteAt = 78.0;
	constexpr double CheckReinforcementsAt = 80.0;
	// The player unit is moved this far from the station pivot, inside the 100 m ring, on the player's side.
	constexpr float PlayerUnitDistance = 8500.f;
	constexpr int32 LocalPlayer = 1;
	const TCHAR* WorkMontagePath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/AimPosMontages/Scavenge_infMontage");
	const TCHAR* ForcesFolder = TEXT("/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/");
	const TCHAR* ShotsSwitch = TEXT("ForgeStationShots");
	// Shot times: the vent of the first sedation cycle is about 22-28 s of game time (test seconds + 3).
	// Shots come after the player unit was moved near (55 s), so the fog of war shows the enemy buildings.
	constexpr double FirstShotAt = 57.5;
	constexpr double SecondShotAt = 60.5;
	constexpr double WreckShotAt = 70.0;
	// Shot runs move the unit closer, inside the guards' reach, so it sees the targets.
	constexpr float ShotPlayerUnitDistance = 4000.f;
}

bool UForgeStationTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), ForgeStationTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UForgeStationTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForgeStationTestSubsystem, STATGROUP_Tickables);
}

void UForgeStationTestSubsystem::Tick(const float DeltaTime)
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
		M_TestStartSeconds = World->GetTimeSeconds() + ForgeStationTestPrivate::StartDelaySeconds;
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
	if (GetTestSeconds() > ForgeStationTestPrivate::GiveUpSeconds)
	{
		Check(false, TEXT("finished_in_time"), FString::Printf(TEXT("stuck at step %d"), static_cast<int32>(M_Step)));
		Evaluate();
		return;
	}
	TickSteps();
#endif
}

double UForgeStationTestSubsystem::GetTestSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_TestStartSeconds;
}

void UForgeStationTestSubsystem::StartTest()
{
	UWorld* World = GetWorld();
	M_Station = ABerlinForgeStation::FindForgeStation(World);
	for (TActorIterator<AEnemyStrongpoint> It(World); It; ++It)
	{
		if (It->GetStrongpointId() == TEXT("ForgeStation"))
		{
			M_Strongpoint = *It;
		}
	}
	for (TObjectIterator<UMission_BerlinForgeStation> It; It; ++It)
	{
		if (It->GetWorld() == World && not It->IsTemplate())
		{
			M_Mission = *It;
		}
	}
	M_WorkMontage = LoadObject<UAnimMontage>(nullptr, ForgeStationTestPrivate::WorkMontagePath);
	bM_TakeShots = FParse::Param(FCommandLine::Get(), ForgeStationTestPrivate::ShotsSwitch);
	M_Step = EStep::CheckSetup;
}

void UForgeStationTestSubsystem::TickSteps()
{
	using namespace ForgeStationTestPrivate;
	const double Seconds = GetTestSeconds();
	TakeShotIfDue(Seconds);
	switch (M_Step)
	{
	case EStep::CheckSetup:
		CheckSetup();
		M_Step = EStep::CheckGarrison;
		break;
	case EStep::CheckGarrison:
		if (Seconds >= CheckGarrisonAt)
		{
			CheckGarrison();
			M_Step = EStep::WatchWork;
		}
		break;
	case EStep::WatchWork:
		WatchWork();
		if (Seconds >= WatchWorkUntil)
		{
			ReportWork();
			M_Step = EStep::MovePlayerUnit;
		}
		break;
	case EStep::MovePlayerUnit:
		MovePlayerUnit();
		M_Step = EStep::CheckPlate;
		break;
	case EStep::CheckPlate:
		if (Seconds >= CheckPlateAt)
		{
			const bool bShown = M_Mission.IsValid() && M_Mission->GetIsPlateShown();
			Check(not bM_PlateShownBeforeMove, TEXT("plate_hidden_before_player_near"), TEXT(""));
			Check(bShown || not bM_MovedPlayerUnit, TEXT("plate_shown_when_player_near"),
			      bM_MovedPlayerUnit ? TEXT("") : TEXT("(no player unit to move; skipped)"));
			M_Step = EStep::KillTargets;
		}
		break;
	case EStep::KillTargets:
		if (Seconds >= KillTargetsAt)
		{
			KillTargets();
			M_Step = EStep::CheckWrecks;
		}
		break;
	case EStep::CheckWrecks:
		if (Seconds >= CheckWrecksAt)
		{
			CheckWrecks();
			M_Step = EStep::CheckComplete;
		}
		break;
	case EStep::CheckComplete:
		if (Seconds >= CheckCompleteAt)
		{
			CheckComplete();
			M_Step = EStep::CheckReinforcements;
		}
		break;
	case EStep::CheckReinforcements:
		if (Seconds >= CheckReinforcementsAt)
		{
			CheckReinforcements();
			Evaluate();
		}
		break;
	default:
		break;
	}
}

void UForgeStationTestSubsystem::CheckSetup()
{
	Check(M_Station.IsValid(), TEXT("station_found"), TEXT(""));
	Check(M_Strongpoint.IsValid(), TEXT("strongpoint_found"), TEXT(""));
	Check(M_Mission.IsValid(), TEXT("mission_found"), TEXT(""));
	Check(IsValid(M_WorkMontage), TEXT("work_montage_loaded"), TEXT(""));
	if (not M_Station.IsValid())
	{
		return;
	}
	Check(M_Station->GetTargetCount() == 3 && M_Station->GetAliveTargetCount() == 3, TEXT("three_targets_alive"),
	      FString::Printf(TEXT("targets %d alive %d"), M_Station->GetTargetCount(), M_Station->GetAliveTargetCount()));
	Check(M_Station->GetRuntimeDressingCount() >= 15, TEXT("dressing_built"),
	      FString::Printf(TEXT("%d components"), M_Station->GetRuntimeDressingCount()));
}

int32 UForgeStationTestSubsystem::GetExpectedDefenders() const
{
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	EnemyPlacement::TryGetGameDifficulty(GetWorld(), Difficulty);
	FString Name = TEXT("Normal");
	switch (Difficulty)
	{
	case ERTSGameDifficulty::NewToRTS:
		Name = TEXT("NewToRTS");
		break;
	case ERTSGameDifficulty::Hard:
		Name = TEXT("Hard");
		break;
	case ERTSGameDifficulty::Brutal:
	case ERTSGameDifficulty::Ironman:
		Name = TEXT("Brutal");
		break;
	default:
		break;
	}
	const FString Path = FString(ForgeStationTestPrivate::ForcesFolder) + TEXT("DA_BerlinForces_") + Name;
	const UBerlinMissionForcesData* Forces = LoadObject<UBerlinMissionForcesData>(nullptr, *Path);
	return IsValid(Forces) ? Forces->ForgeStation.Garrison.Num() : INDEX_NONE;
}

void UForgeStationTestSubsystem::CheckGarrison()
{
	if (M_Strongpoint.IsValid())
	{
		const int32 Expected = GetExpectedDefenders();
		Check(M_Strongpoint->GetDefenderCount() == Expected, TEXT("garrison_from_data_asset"),
		      FString::Printf(TEXT("defenders %d expected %d"), M_Strongpoint->GetDefenderCount(), Expected));
		Check(M_Strongpoint->GetRunningWorkCircuitCount() >= 1, TEXT("work_circuit_running"),
		      FString::Printf(TEXT("%d running"), M_Strongpoint->GetRunningWorkCircuitCount()));
		Check(not M_Strongpoint->GetIsEngaged(), TEXT("garrison_not_engaged_at_start"), TEXT(""));
	}
	if (not M_Station.IsValid())
	{
		return;
	}
	FString MeshNames;
	bool bAllForgeMeshes = true;
	for (AActor* Target : M_Station->GetTargetActors())
	{
		// The building mesh component is the root of every building expansion.
		const UStaticMeshComponent* BuildingMesh = IsValid(Target)
			                                           ? Cast<UStaticMeshComponent>(Target->GetRootComponent())
			                                           : nullptr;
		const UStaticMesh* Mesh = IsValid(BuildingMesh) ? BuildingMesh->GetStaticMesh() : nullptr;
		const FString MeshName = IsValid(Mesh) ? Mesh->GetName() : TEXT("none");
		bAllForgeMeshes = bAllForgeMeshes && MeshName.StartsWith(TEXT("SM_ForgeStation_"));
		MeshNames += MeshName + TEXT(" ");
	}
	Check(bAllForgeMeshes, TEXT("targets_keep_station_meshes"), MeshNames);
	// Diagnostics: what of each target is drawn (the building mesh may be hidden by a construction or deploy step).
	bool bAllVisible = true;
	for (AActor* Target : M_Station->GetTargetActors())
	{
		const ABuildingExpansion* Building = Cast<ABuildingExpansion>(Target);
		const UStaticMeshComponent* Root = Cast<UStaticMeshComponent>(Target->GetRootComponent());
		// The actor itself is hidden by the fog of war until a player unit sees it; that is correct.
		const bool bBuiltAndDrawn = IsValid(Root) && Root->IsVisible() && IsValid(Building)
			&& Building->GetStatus() == EBuildingExpansionStatus::BXS_Built;
		bAllVisible = bAllVisible && bBuiltAndDrawn;
		FString Components;
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			Components += FString::Printf(TEXT("%s%s "), *Primitive->GetName(), Primitive->IsVisible() ? TEXT("+") : TEXT("-"));
		}
		const int32 Status = IsValid(Building) ? static_cast<int32>(Building->GetStatus()) : -1;
		M_Lines.Add(FString::Printf(TEXT("INFO target %s rootVisible=%d actorHidden=%d status=%d comps: %s"),
		                            *Target->GetName(), IsValid(Root) && Root->IsVisible(), Target->IsHidden(), Status,
		                            *Components));
		UE_LOG(LogRTSForgeStationTest, Display, TEXT("RTS_FORGE_TEST %s"), *M_Lines.Last());
	}
	Check(bAllVisible, TEXT("targets_built_and_mesh_visible"), TEXT(""));
	Check(M_Mission.IsValid() && not M_Mission->GetIsPlateShown(), TEXT("plate_hidden_at_start"), TEXT(""));
}

bool UForgeStationTestSubsystem::GetIsAnyWorkMontagePlaying() const
{
	if (not IsValid(M_WorkMontage))
	{
		return false;
	}
	for (TActorIterator<ASquadUnit> It(GetWorld()); It; ++It)
	{
		const USquadUnitAnimInstance* AnimInstance = It->GetAnimBP_SquadUnit();
		if (IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(M_WorkMontage))
		{
			return true;
		}
	}
	return false;
}

void UForgeStationTestSubsystem::WatchWork()
{
	if (M_Strongpoint.IsValid())
	{
		M_MaxWorkingCircuits = FMath::Max(M_MaxWorkingCircuits, M_Strongpoint->GetWorkingCircuitCount());
	}
	bM_SawWorkMontage = bM_SawWorkMontage || GetIsAnyWorkMontagePlaying();
	if (M_Station.IsValid())
	{
		M_SeenSedationPhases.Add(static_cast<uint8>(M_Station->GetSedationPhase()));
	}
	if (M_Mission.IsValid() && M_Mission->GetIsPlateShown())
	{
		bM_PlateShownBeforeMove = true;
	}
}

void UForgeStationTestSubsystem::ReportWork()
{
	Check(M_MaxWorkingCircuits >= 1, TEXT("crew_reached_a_stake_and_worked"),
	      FString::Printf(TEXT("max working %d"), M_MaxWorkingCircuits));
	Check(bM_SawWorkMontage, TEXT("work_montage_played"), TEXT(""));
	Check(M_SeenSedationPhases.Contains(static_cast<uint8>(EForgeSedationPhase::Vent))
	      && M_SeenSedationPhases.Contains(static_cast<uint8>(EForgeSedationPhase::Thaw)),
	      TEXT("sedation_cycle_ran"), FString::Printf(TEXT("phases seen %d"), M_SeenSedationPhases.Num()));
}

void UForgeStationTestSubsystem::MovePlayerUnit()
{
	if (not M_Station.IsValid())
	{
		return;
	}
	AActor* PlayerUnit = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		const bool bIsPlayerUnit = IsValid(RTSComponent)
			&& RTSComponent->GetOwningPlayer() == ForgeStationTestPrivate::LocalPlayer
			&& Cast<ASquadController>(*It) == nullptr && Cast<ICommands>(*It) != nullptr;
		if (bIsPlayerUnit && RTSFunctionLibrary::RTSIsValid(*It))
		{
			PlayerUnit = *It;
			break;
		}
	}
	if (not IsValid(PlayerUnit))
	{
		Check(true, TEXT("player_unit_for_ring"), TEXT("none on the map; ring not exercised"));
		return;
	}
	// The player's side of the station: its local -X (away from the crater).
	const FTransform& StationTransform = M_Station->GetActorTransform();
	const float Distance = bM_TakeShots ? ForgeStationTestPrivate::ShotPlayerUnitDistance
		                       : ForgeStationTestPrivate::PlayerUnitDistance;
	FVector Target = StationTransform.TransformPosition(FVector(-Distance, 0.f, 0.f));
	Target.Z = PlayerUnit->GetActorLocation().Z;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(0, 0, 5000), Target - FVector(0, 0, 5000),
	                                         ECC_Visibility))
	{
		Target.Z = Hit.ImpactPoint.Z + 150.f;
	}
	PlayerUnit->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	bM_MovedPlayerUnit = true;
	Check(true, TEXT("player_unit_moved_into_ring"), PlayerUnit->GetName());
}

void UForgeStationTestSubsystem::LookAtStation(const FVector& LocalCameraOffset)
{
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (not M_Station.IsValid() || not IsValid(Controller))
	{
		return;
	}
	const FTransform& StationTransform = M_Station->GetActorTransform();
	const FVector CameraLocation = StationTransform.TransformPosition(LocalCameraOffset);
	const FVector LookAt = StationTransform.TransformPosition(FVector(300.f, 0.f, 300.f));
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, (LookAt - CameraLocation).Rotation());
	if (IsValid(Camera))
	{
		Controller->SetViewTarget(Camera);
	}
}

void UForgeStationTestSubsystem::TakeShotIfDue(const double Seconds)
{
	using namespace ForgeStationTestPrivate;
	if (not bM_TakeShots)
	{
		return;
	}
	const double ShotTimes[] = {FirstShotAt, SecondShotAt, WreckShotAt};
	const FVector CameraOffsets[] = {
		FVector(-2600.f, 2200.f, 1900.f), FVector(3200.f, -2600.f, 1700.f), FVector(-2600.f, 2200.f, 1900.f)
	};
	const TCHAR* Names[] = {TEXT("ForgeStation_front"), TEXT("ForgeStation_crater_side"), TEXT("ForgeStation_wrecks")};
	if (M_ShotsTaken >= UE_ARRAY_COUNT(ShotTimes) || Seconds < ShotTimes[M_ShotsTaken] - 1.0)
	{
		return;
	}
	if (Seconds < ShotTimes[M_ShotsTaken])
	{
		if (M_CameraPlacedForShot != M_ShotsTaken)
		{
			M_CameraPlacedForShot = M_ShotsTaken;
			LookAtStation(CameraOffsets[M_ShotsTaken]);
		}
		return;
	}
	const FString File = FPaths::ProjectSavedDir() / TEXT("ForgeStationTest") / (FString(Names[M_ShotsTaken]) + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, false, false);
	Check(true, TEXT("shot_requested"), File);
	++M_ShotsTaken;
}

void UForgeStationTestSubsystem::KillTargets()
{
	if (not M_Station.IsValid())
	{
		return;
	}
	int32 Killed = 0;
	for (AActor* Target : M_Station->GetTargetActors())
	{
		if (AHPActorObjectsMaster* Building = Cast<AHPActorObjectsMaster>(Target))
		{
			Building->TriggerDestroyActor(ERTSDeathType::Kinetic);
			++Killed;
		}
	}
	Check(Killed == 3, TEXT("targets_killed"), FString::Printf(TEXT("%d"), Killed));
}

void UForgeStationTestSubsystem::CheckWrecks()
{
	if (not M_Station.IsValid())
	{
		return;
	}
	Check(M_Station->GetAliveTargetCount() == 0, TEXT("targets_dead"),
	      FString::Printf(TEXT("alive %d"), M_Station->GetAliveTargetCount()));
	Check(M_Station->GetWreckCount() == 3, TEXT("wrecks_spawned"),
	      FString::Printf(TEXT("%d"), M_Station->GetWreckCount()));
	Check(M_Station->GetSedationPhase() == EForgeSedationPhase::Idle, TEXT("sedation_stopped_with_rig"), TEXT(""));
}

void UForgeStationTestSubsystem::CheckComplete()
{
	Check(M_Mission.IsValid() && M_Mission->GetHasFiredAllTargetsDestroyed(), TEXT("all_targets_destroyed_event_fired"),
	      TEXT(""));
	Check(M_Mission.IsValid() && M_Mission->GetClass()->IsInBlueprint(), TEXT("mission_is_blueprint_stage"),
	      M_Mission.IsValid() ? M_Mission->GetClass()->GetName() : TEXT("none"));
	Check(M_Mission.IsValid() && M_Mission->GetIsPlateShown(), TEXT("plate_shown_by_target_death"), TEXT(""));
	Check(M_Mission.IsValid() && M_Mission->GetIsBonusComplete(), TEXT("bonus_completed"), TEXT(""));
	Check(M_Mission.IsValid() && M_Mission->GetHasAddedArchiveEntry(), TEXT("archive_entry_added"), TEXT(""));
}

void UForgeStationTestSubsystem::CheckReinforcements()
{
	Check(M_Mission.IsValid() && M_Mission->GetHasLaunchedReinforcements(), TEXT("reinforcements_launched"),
	      TEXT(""));
	const UPlayerAirDropSubsystem* AirDrop = UPlayerAirDropSubsystem::Get(GetWorld());
	Check(IsValid(AirDrop) && AirDrop->GetRunningAbilityCount() > 0, TEXT("drop_aircraft_flying"),
	      FString::Printf(TEXT("%d abilities"), IsValid(AirDrop) ? AirDrop->GetRunningAbilityCount() : -1));
}

void UForgeStationTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSForgeStationTest, Display, TEXT("RTS_FORGE_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}

void UForgeStationTestSubsystem::Evaluate()
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSForgeStationTest, Display, TEXT("RTS_FORGE_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("ForgeStationTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	M_Step = EStep::Finished;
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}
