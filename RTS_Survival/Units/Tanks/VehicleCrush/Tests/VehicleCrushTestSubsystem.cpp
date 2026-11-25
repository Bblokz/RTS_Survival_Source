// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleCrushTestSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/VehicleCrush/RTSVehicleCrushSettings.h"
#include "RTS_Survival/Units/Tanks/VehicleCrush/RTSVehicleCrushSubsystem.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSVehicleCrushTest, Log, All);

namespace VehicleCrushTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("VehicleCrushTest");
	const TCHAR* const LocationSwitch = TEXT("VehicleCrushTestAt=");
	const TCHAR* const TankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
			"Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const SquadClassPath = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	// The flat field the vehicle duel bench and the wall gate test use on RTS_ToCopySmall.
	const FVector DefaultArenaLocation(16796.f, -15808.f, 0.f);

	constexpr double HudBuildSeconds = 5.0;
	constexpr float SquadLoadTimeoutSeconds = 40.f;
	constexpr float DriveTimeoutSeconds = 90.f;
	constexpr float SpawnLiftCm = 150.f;
	// Squads spawn beside the lane and are put into it once their soldiers exist.
	constexpr float SquadSpawnSideOffsetCm = 2500.f;
	constexpr float SoldierSpacingAcrossCm = 55.f;
	constexpr float SoldierSpacingAlongCm = 90.f;
	constexpr float GoalDistanceCm = 8500.f;
	constexpr float GoalReachedCm = 600.f;
	// Past the allied squad: from here on every hostile soldier reacts.
	constexpr float EnableDodgeAtCm = 5000.f;
	constexpr float GroundTraceUpCm = 5000.f;
	constexpr float GroundTraceDownCm = 20000.f;
}

bool UVehicleCrushTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld() &&
		FParse::Param(FCommandLine::Get(), VehicleCrushTestPrivate::TestSwitch);
}

TStatId UVehicleCrushTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleCrushTestSubsystem, STATGROUP_Tickables);
}

void UVehicleCrushTestSubsystem::Tick(const float DeltaTime)
{
	using namespace VehicleCrushTestPrivate;
	UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay() || M_Step == EVehicleCrushTestStep::Done)
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds();
	if (M_StartRealSeconds < 0.0)
	{
		M_StartRealSeconds = RealSeconds;
	}
	if (M_Step != EVehicleCrushTestStep::StartGame)
	{
		KeepGameRunning();
		HoldTankFire();
	}
	switch (M_Step)
	{
	case EVehicleCrushTestStep::StartGame:
		if (RealSeconds - M_StartRealSeconds >= HudBuildSeconds && TryStartGame())
		{
			KeepGameRunning();
			SpawnUnits();
		}
		return;
	case EVehicleCrushTestStep::WaitSquads:
		{
			bool bAllLoaded = true;
			for (const FLaneSquad& Lane : M_Squads)
			{
				bAllLoaded &= Lane.Squad.IsValid() && Lane.Squad->GetIsSquadFullyLoadedAndInitialized();
			}
			if (bAllLoaded && M_Tank.IsValid() && GetGameSeconds() - M_StepStartSeconds > 3.f)
			{
				PlaceSoldiersInLane();
				return;
			}
			if (GetGameSeconds() - M_StepStartSeconds > SquadLoadTimeoutSeconds)
			{
				Check(false, TEXT("squads_loaded"), TEXT("timeout"));
				Finish(TEXT("squads_not_loaded"));
			}
			return;
		}
	case EVehicleCrushTestStep::Drive:
		TickDrive();
		return;
	case EVehicleCrushTestStep::Done:
		return;
	}
}

bool UVehicleCrushTestSubsystem::TryStartGame() const
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
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

void UVehicleCrushTestSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		if (UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(VehicleCrushTestPrivate::DifficultyButtonName)))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UVehicleCrushTestSubsystem::SpawnUnits()
{
	using namespace VehicleCrushTestPrivate;
	M_Arena = DefaultArenaLocation;
	FString At;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, At, false))
	{
		FString X, Y;
		if (At.Split(TEXT(","), &X, &Y))
		{
			M_Arena = FVector(FCString::Atof(*X), FCString::Atof(*Y), 0.f);
		}
	}
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_Arena, GroundZ))
	{
		Finish(TEXT("no_ground"));
		return;
	}
	M_Arena.Z = GroundZ;
	M_Goal = M_Arena + FVector(GoalDistanceCm, 0.f, 0.f);

	// Player 3 joins player 1's team: an ally that is not the vehicle's own owner.
	FRTSPlayers::ResetToFreeForAll();
	FRTSPlayers::SetPlayerTeam(3, RTSPlayers::HumanPlayer);
	URTSVehicleCrushSettings* const Settings = GetMutableDefault<URTSVehicleCrushSettings>();
	Settings->DodgeChance = 0.f;

	UClass* const TankClass = LoadClass<AActor>(nullptr, TankClassPath);
	UClass* const SquadClass = LoadClass<AActor>(nullptr, SquadClassPath);
	if (not IsValid(TankClass) || not IsValid(SquadClass))
	{
		Finish(TEXT("classes_not_found"));
		return;
	}
	M_Tank = Cast<ATankMaster>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), TankClass, FTransform(FRotator::ZeroRotator, M_Arena + FVector(0.f, 0.f, SpawnLiftCm)),
		RTSPlayers::HumanPlayer));

	M_Squads = {
		{TEXT("hostile_no_dodge"), 2, 1600.f},
		{TEXT("own"), 1, 3000.f},
		{TEXT("allied"), 3, 4200.f},
		{TEXT("hostile_dodge"), 2, 6600.f},
	};
	for (FLaneSquad& Lane : M_Squads)
	{
		const FVector Beside = M_Arena + FVector(Lane.LaneX, SquadSpawnSideOffsetCm, SpawnLiftCm);
		Lane.Squad = Cast<ASquadController>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*GetWorld(), SquadClass, FTransform(Beside), Lane.Player));
	}
	UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH spawned tank=%d arena=%s"),
	       M_Tank.IsValid() ? 1 : 0, *M_Arena.ToCompactString());
	M_Step = EVehicleCrushTestStep::WaitSquads;
	M_StepStartSeconds = GetGameSeconds();
}

void UVehicleCrushTestSubsystem::PlaceSoldiersInLane()
{
	using namespace VehicleCrushTestPrivate;
	for (FLaneSquad& Lane : M_Squads)
	{
		TArray<ASquadUnit*> Soldiers = Lane.Squad->GetSquadUnitsChecked();
		int32 Index = 0;
		for (ASquadUnit* const Soldier : Soldiers)
		{
			if (not IsValid(Soldier))
			{
				continue;
			}
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, Lane.Player);
			// Two ranks across the lane centre, all inside the hull width.
			const float Across = ((Index % 3) - 1) * SoldierSpacingAcrossCm;
			const float Along = (Index / 3) * SoldierSpacingAlongCm;
			FVector Spot = M_Arena + FVector(Lane.LaneX + Along, Across, 0.f);
			float GroundZ = Spot.Z;
			(void)TryGetGroundZ(Spot, GroundZ);
			Spot.Z = GroundZ + Soldier->GetSimpleCollisionHalfHeight() + 2.f;
			Soldier->Force_TerminateMovement();
			Soldier->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
			++Index;
		}
		Lane.SoldiersPlaced = Index;
		UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH lane %s player=%d soldiers=%d x=%.0f"),
		       *Lane.Label, Lane.Player, Index, Lane.LaneX);
	}
	ICommands* const TankCommands = Cast<ICommands>(M_Tank.Get());
	const bool bOrdered = TankCommands != nullptr &&
		TankCommands->MoveToLocation(M_Goal, true, FRotator::ZeroRotator) == ECommandQueueError::NoError;
	Check(bOrdered, TEXT("tank_move_ordered"), M_Goal.ToCompactString());
	M_Step = EVehicleCrushTestStep::Drive;
	M_StepStartSeconds = GetGameSeconds();
}

void UVehicleCrushTestSubsystem::HoldTankFire() const
{
	const ATankMaster* const Tank = M_Tank.Get();
	if (not IsValid(Tank))
	{
		return;
	}
	for (ACPPTurretsMaster* const Turret : Tank->GetTurrets())
	{
		if (IsValid(Turret))
		{
			Turret->DisableTurret();
		}
	}
	for (UHullWeaponComponent* const HullWeapon : Tank->GetHullWeapons())
	{
		if (IsValid(HullWeapon))
		{
			HullWeapon->DisableHullWeapon();
		}
	}
}

void UVehicleCrushTestSubsystem::TickDrive()
{
	using namespace VehicleCrushTestPrivate;
	ATankMaster* const Tank = M_Tank.Get();
	if (not IsValid(Tank))
	{
		Finish(TEXT("tank_lost"));
		return;
	}
	const FVector TankLocation = Tank->GetActorLocation();
	if (not bM_DodgeEnabled && TankLocation.X - M_Arena.X > EnableDodgeAtCm)
	{
		bM_DodgeEnabled = true;
		URTSVehicleCrushSettings* const Settings = GetMutableDefault<URTSVehicleCrushSettings>();
		Settings->DodgeChance = 1.f;
		Settings->WrongSideChance = 0.f;
		Settings->DodgeReactionSeconds = FVector2D(0.05f, 0.15f);
		UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH dodge on at x=%.0f"),
		       TankLocation.X - M_Arena.X);
	}
	const bool bArrived = FVector::Dist2D(TankLocation, M_Goal) < GoalReachedCm;
	if (not bArrived && GetGameSeconds() - M_StepStartSeconds < DriveTimeoutSeconds)
	{
		return;
	}
	Check(bArrived, TEXT("tank_reached_goal"),
	      FString::Printf(TEXT("dist=%.0f after_s=%.1f"), FVector::Dist2D(TankLocation, M_Goal),
	                      GetGameSeconds() - M_StepStartSeconds));

	const URTSVehicleCrushSubsystem* const Crush = GetWorld()->GetSubsystem<URTSVehicleCrushSubsystem>();
	if (not IsValid(Crush))
	{
		Finish(TEXT("no_crush_subsystem"));
		return;
	}
	int32 HostileCrushes = 0;
	int32 NonHostileCrushes = 0;
	for (const FIntPoint& Pair : Crush->GetCrushOwnerPairs())
	{
		(FRTSPlayers::AreHostile(Pair.X, Pair.Y) ? HostileCrushes : NonHostileCrushes)++;
		UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH crush vehicle_owner=%d soldier_owner=%d"),
		       Pair.X, Pair.Y);
	}
	Check(HostileCrushes > 0, TEXT("hostile_soldiers_run_over"), FString::Printf(TEXT("%d"), HostileCrushes));
	Check(NonHostileCrushes == 0, TEXT("no_own_or_allied_soldier_run_over"),
	      FString::Printf(TEXT("%d"), NonHostileCrushes));
	Check(Crush->GetDodgeCount() > 0, TEXT("soldiers_dodged"), FString::Printf(TEXT("%d"), Crush->GetDodgeCount()));
	for (const FLaneSquad& Lane : M_Squads)
	{
		const int32 Alive = Lane.Squad.IsValid() ? Lane.Squad->GetSquadUnitsChecked().Num() : 0;
		UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH lane %s alive=%d of %d"), *Lane.Label, Alive,
		       Lane.SoldiersPlaced);
	}
	Finish(TEXT("done"));
}

void UVehicleCrushTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	(bCondition ? M_Passed : M_Failed)++;
	UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH CHECK %s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail);
}

void UVehicleCrushTestSubsystem::Finish(const FString& Reason)
{
	M_Step = EVehicleCrushTestStep::Done;
	const bool bPass = M_Failed == 0 && M_Passed > 0;
	UE_LOG(LogRTSVehicleCrushTest, Display, TEXT("RTS_VEHICLE_CRUSH RESULT %s reason=%s passed=%d failed=%d"),
	       bPass ? TEXT("PASS") : TEXT("FAIL"), *Reason, M_Passed, M_Failed);
	FPlatformMisc::RequestExit(false, TEXT("VehicleCrushTest"));
}

bool UVehicleCrushTestSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace VehicleCrushTestPrivate;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleCrushTestGround), false);
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

float UVehicleCrushTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

#else

bool UVehicleCrushTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UVehicleCrushTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UVehicleCrushTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleCrushTestSubsystem, STATGROUP_Tickables);
}

#endif
