// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallGateRelationsTestSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallGate/WallGate.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallGate/WallGateNavigation.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Navigation/NavQueryFilterSettings/RTSNavQueryFilterSettings.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWallGateTest, Log, All);

namespace WallGateRelationsTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("WallGateRelationsTest");
	const TCHAR* const DirectorySwitch = TEXT("WallGateTestDir=");
	const TCHAR* const LocationSwitch = TEXT("WallGateTestAt=");
	const TCHAR* const GateClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/BP_RusWallGateA.BP_RusWallGateA_C");
	// The tank of the four-player skirmish: a Blueprint every player can own.
	const TCHAR* const TankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
			"Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	// The flat field the vehicle duel bench and the four-player skirmish use on RTS_ToCopySmall.
	const FVector DefaultArenaLocation(16796.f, -15808.f, 0.f);

	constexpr uint8 HumanPlayer = RTSPlayers::HumanPlayer;
	// The gate's owner and allowed player (the Soviet gate default).
	constexpr uint8 GatePlayer = WallGateDefaults::EnemyAIPlayer;
	constexpr uint8 HostileAIPlayer = 3;
	// Captures the gate at the end.
	constexpr uint8 CapturingPlayer = 4;
	// Player 2's own team number: putting player 3 on it allies players 2 and 3.
	constexpr int32 SharedTeamOfGateAndHostile = GatePlayer;

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 10.0 * 60.0;
	// Tanks park this far from the doorway, 120 degrees apart: one tank at the doorway is ~54 m or more from the
	// others, beyond the ~40 m weapon ranges, so nobody fights and no stray shell reaches the gate.
	constexpr float ParkRadiusCm = 6000.f;
	constexpr float ParkYawStepDegrees = 120.f;
	// In front of the doorway: inside the gate's OpenRadius (14 m), outside its doorway clearance (2 m).
	constexpr float ApproachDistanceCm = 700.f;
	// Path probes start and end this far in front of and behind the doorway.
	constexpr float PathProbeDistanceCm = 800.f;
	constexpr float SpawnLiftCm = 150.f;
	constexpr float TeleportLiftCm = 100.f;
	// Nothing the map placed may stand this close to the doorway: it would open or hold the gate on its own.
	constexpr float ForeignUnitClearRadiusCm = 2500.f;
	// Weapon shots start this far in front of the leaf and end this far behind it.
	constexpr float ShotStartDistanceCm = 900.f;
	constexpr float ShotEndBehindCm = 100.f;
	// Shots aim halfway between the doorway centre and the leaf's edge.
	constexpr float ShotSideFraction = 0.5f;
	// Used when the frame has no simple collision to measure: the doorway half width plus this.
	constexpr float FallbackFrameMarginCm = 150.f;
	// A tank brought to the doorway must really stand there before a "stays closed" verdict counts.
	constexpr float ApproachToleranceCm = 300.f;
	// The second gate (spawned for player 3) stands this far along the wall line from the main one: beyond the main
	// gate's frame and every approach point's 14 m open radius.
	constexpr float SecondGateOffsetCm = 3000.f;
	constexpr uint8 SecondGatePlayer = 3;

	constexpr float SettleGameSeconds = 3.f;
	constexpr float NavReadyTimeoutGameSeconds = 30.f;
	// Detection runs every 0.3 s; opening starts on the first scan that sees an allowed unit.
	constexpr float OpenTimeoutGameSeconds = 6.f;
	constexpr float HoldClosedGameSeconds = 5.f;
	// Close delay 2 s + closing 3 s + the doorway area rebuild.
	constexpr float CloseTimeoutGameSeconds = 25.f;

	// The infantry agent (smallest radius), so the doorway is wide enough for the path whoever plans it.
	constexpr float CharacterAgentRadiusCm = 35.f;
	constexpr float CharacterAgentHeightCm = 144.f;
	const FVector PolyQueryExtent(50.f, 50.f, 300.f);
	const FVector ProjectExtent(200.f, 200.f, 500.f);
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
	constexpr float HalfFactor = 0.5f;

	const TCHAR* GetOutcomeName(const EWallGatePathOutcome Outcome)
	{
		switch (Outcome)
		{
		case EWallGatePathOutcome::ThroughGate:
			return TEXT("through_gate");
		case EWallGatePathOutcome::AroundGate:
			return TEXT("around_gate");
		case EWallGatePathOutcome::NoPath:
			return TEXT("no_path");
		default:
			return TEXT("setup_failed");
		}
	}

	const TCHAR* GetGateStateName(const EWallGateState State)
	{
		switch (State)
		{
		case EWallGateState::Closed:
			return TEXT("Closed");
		case EWallGateState::Opening:
			return TEXT("Opening");
		case EWallGateState::Open:
			return TEXT("Open");
		case EWallGateState::Closing:
			return TEXT("Closing");
		default:
			return TEXT("Wrecked");
		}
	}

	/** @return The player whose tank a step brings to the doorway, or the neutral owner for other steps. */
	uint8 GetApproachingPlayerOfStep(const EWallGateRelationsTestStep Step)
	{
		switch (Step)
		{
		case EWallGateRelationsTestStep::OwnerApproach:
		case EWallGateRelationsTestStep::FormerOwnerApproach:
			return GatePlayer;
		case EWallGateRelationsTestStep::HostileApproach:
		case EWallGateRelationsTestStep::AllyApproach:
			return HostileAIPlayer;
		case EWallGateRelationsTestStep::HumanApproach:
			return HumanPlayer;
		default:
			return RTSPlayers::NeutralOwner;
		}
	}

	/** @return The player whose tank opened the gate and goes back to its park in a leave step, else neutral. */
	uint8 GetLeavingPlayerOfStep(const EWallGateRelationsTestStep Step)
	{
		if (Step == EWallGateRelationsTestStep::OwnerLeave)
		{
			return GatePlayer;
		}
		return Step == EWallGateRelationsTestStep::AllyLeave ? HostileAIPlayer : RTSPlayers::NeutralOwner;
	}

	ARecastNavMesh* GetCharacterNavMesh(UWorld* World)
	{
		UNavigationSystemV1* const NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavigationSystem == nullptr)
		{
			return nullptr;
		}
		const FNavAgentProperties CharacterAgent(CharacterAgentRadiusCm, CharacterAgentHeightCm);
		return Cast<ARecastNavMesh>(NavigationSystem->GetNavDataForProps(CharacterAgent));
	}

	/**
	 * @brief Crossings of the wall line (through GroundCentre along Along) by a path, measured along the wall.
	 * @return Smallest distance along the wall from the doorway centre at which the path crosses; max float when the
	 * path never crosses.
	 */
	float GetClosestWallLineCrossing(const TArray<FVector>& PathPoints, const FWallGateRelationsTestDoorway& Doorway)
	{
		float ClosestCrossingCm = TNumericLimits<float>::Max();
		for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
		{
			const FVector& SegmentStart = PathPoints[PointIndex - 1];
			const FVector& SegmentEnd = PathPoints[PointIndex];
			const double StartSide = FVector::DotProduct(SegmentStart - Doorway.GroundCentre, Doorway.Normal);
			const double EndSide = FVector::DotProduct(SegmentEnd - Doorway.GroundCentre, Doorway.Normal);
			if (StartSide * EndSide > 0.0 || StartSide == EndSide)
			{
				continue;
			}
			const double CrossingAlpha = StartSide / (StartSide - EndSide);
			const FVector Crossing = FMath::Lerp(SegmentStart, SegmentEnd, CrossingAlpha);
			const float AlongWallCm = FMath::Abs(FVector::DotProduct(Crossing - Doorway.GroundCentre, Doorway.Along));
			ClosestCrossingCm = FMath::Min(ClosestCrossingCm, AlongWallCm);
		}
		return ClosestCrossingCm;
	}
}

bool UWallGateRelationsTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), WallGateRelationsTestPrivate::TestSwitch);
}

void UWallGateRelationsTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ReadCommandLine();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*M_OutputDirectory);
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSWallGateTest, Display, TEXT("RTS_WALLGATE started dir=%s arena=%s"), *M_OutputDirectory,
	       *M_ArenaCentre.ToCompactString());
}

TStatId UWallGateRelationsTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWallGateRelationsTestSubsystem, STATGROUP_Tickables);
}

void UWallGateRelationsTestSubsystem::Tick(float DeltaTime)
{
	using namespace WallGateRelationsTestPrivate;
	if (not bM_IsActive || M_Step == EWallGateRelationsTestStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("real_time_watchdog_in_%s"), *GetStepName()));
		return;
	}
	KeepGameRunning();
	if (M_Step == EWallGateRelationsTestStep::StartGame)
	{
		TickStartGame(RealSeconds);
		return;
	}
	TickStep();
}

void UWallGateRelationsTestSubsystem::ReadCommandLine()
{
	using namespace WallGateRelationsTestPrivate;
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
	M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WallGateRelations"),
	                                    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FParse::Value(CommandLine, DirectorySwitch, M_OutputDirectory, false);
}

void UWallGateRelationsTestSubsystem::TickStartGame(const double RealSeconds)
{
	if (RealSeconds < WallGateRelationsTestPrivate::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	EnterStep(EWallGateRelationsTestStep::Spawn);
}

bool UWallGateRelationsTestSubsystem::TryStartGame()
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

void UWallGateRelationsTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(WallGateRelationsTestPrivate::DifficultyButtonName));
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

void UWallGateRelationsTestSubsystem::TickStep()
{
	using namespace WallGateRelationsTestPrivate;
	if (M_Step != EWallGateRelationsTestStep::Spawn && not M_Gate.IsValid())
	{
		Finish(false, TEXT("gate_destroyed"));
		return;
	}
	switch (M_Step)
	{
	case EWallGateRelationsTestStep::Spawn:
		SpawnGateAndTanks();
		break;
	case EWallGateRelationsTestStep::WaitReady:
		TickWaitReady(RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(GatePlayer),
		              EWallGateRelationsTestStep::FreeForAllChecks);
		break;
	case EWallGateRelationsTestStep::FreeForAllChecks:
		RunFreeForAllChecks();
		break;
	case EWallGateRelationsTestStep::OwnerApproach:
		TickApproach(GatePlayer, true, EWallGateRelationsTestStep::OwnerLeave);
		break;
	case EWallGateRelationsTestStep::OwnerLeave:
		TickLeave(EWallGateRelationsTestStep::HostileApproach);
		break;
	case EWallGateRelationsTestStep::HostileApproach:
		TickApproach(HostileAIPlayer, false, EWallGateRelationsTestStep::HumanApproach);
		break;
	case EWallGateRelationsTestStep::HumanApproach:
		TickApproach(HumanPlayer, false, EWallGateRelationsTestStep::TeamChecks);
		break;
	case EWallGateRelationsTestStep::TeamChecks:
		RunTeamChecks();
		break;
	case EWallGateRelationsTestStep::AllyApproach:
		TickApproach(HostileAIPlayer, true, EWallGateRelationsTestStep::AllyLeave);
		break;
	case EWallGateRelationsTestStep::AllyLeave:
		TickLeave(EWallGateRelationsTestStep::CaptureWaitReady);
		break;
	case EWallGateRelationsTestStep::CaptureWaitReady:
		TickWaitReady(RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(CapturingPlayer),
		              EWallGateRelationsTestStep::CaptureChecks);
		break;
	case EWallGateRelationsTestStep::CaptureChecks:
		RunCaptureChecks();
		break;
	case EWallGateRelationsTestStep::FormerOwnerApproach:
		TickApproach(GatePlayer, false, EWallGateRelationsTestStep::Done);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------------------------------

void UWallGateRelationsTestSubsystem::SpawnGateAndTanks()
{
	using namespace WallGateRelationsTestPrivate;
	FRTSPlayers::ResetToFreeForAll();
	float GroundZ = 0.f;
	UClass* const GateClass = LoadClass<AWallGate>(nullptr, GateClassPath);
	if (not IsValid(GateClass) || not TryGetGroundZ(M_ArenaCentre, GroundZ))
	{
		Finish(false, IsValid(GateClass) ? TEXT("no_ground_at_arena") : TEXT("no_gate_class"));
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWallGate* const Gate = GetWorld()->SpawnActor<AWallGate>(
		GateClass, FTransform(FVector(M_ArenaCentre.X, M_ArenaCentre.Y, GroundZ)), SpawnParameters);
	M_Gate = Gate;
	if (not IsValid(Gate) || not TryReadDoorway())
	{
		Finish(false, IsValid(Gate) ? TEXT("no_doorway_box") : TEXT("gate_spawn_failed"));
		return;
	}
	const URTSComponent* const GateRTSComponent = Gate->GetRTSComponent();
	const int32 GateOwner = IsValid(GateRTSComponent) ? GateRTSComponent->GetOwningPlayer() : INDEX_NONE;
	RecordCheck(TEXT("gate_defaults"), Gate->GetAllowedPlayer() == GatePlayer && GateOwner == GatePlayer,
	            FString::Printf(TEXT("allowed_player=%d owner=%d class=%s doorway=%s normal=%s frame_half_extent_cm=%.0f"),
	                            Gate->GetAllowedPlayer(), GateOwner, *GateClass->GetName(),
	                            *M_Doorway.GroundCentre.ToCompactString(), *M_Doorway.Normal.ToCompactString(),
	                            M_Doorway.FrameHalfExtentCm));
	SpawnSecondGateForPlayer3(GateClass);
	SpawnParkedTank(GatePlayer, 0.f);
	SpawnParkedTank(HostileAIPlayer, ParkYawStepDegrees);
	SpawnParkedTank(HumanPlayer, 2.f * ParkYawStepDegrees);
	for (const uint8 Player : {GatePlayer, HostileAIPlayer, HumanPlayer})
	{
		if (not M_TankByPlayer[Player].IsValid())
		{
			Finish(false, FString::Printf(TEXT("tank_spawn_failed_P%d"), Player));
			return;
		}
	}
	EnterStep(EWallGateRelationsTestStep::WaitReady);
}

bool UWallGateRelationsTestSubsystem::TryReadDoorway()
{
	using namespace WallGateRelationsTestPrivate;
	FBox LeafSpaceBox;
	FTransform LeafTransform;
	if (not M_Gate->GetDoorwayNavBox(LeafSpaceBox, LeafTransform))
	{
		return false;
	}
	// Same axis choice as AWallGate::GetDoorwayNavBox: the leaf is thin across the wall.
	const FVector WorldSize = LeafSpaceBox.GetSize() * LeafTransform.GetScale3D().GetAbs();
	const bool bIsThinAlongX = WorldSize.X <= WorldSize.Y;
	const FVector LocalNormal = bIsThinAlongX ? FVector::XAxisVector : FVector::YAxisVector;
	const FVector LocalAlong = bIsThinAlongX ? FVector::YAxisVector : FVector::XAxisVector;
	M_Doorway.Normal = LeafTransform.TransformVectorNoScale(LocalNormal).GetSafeNormal2D();
	M_Doorway.Along = LeafTransform.TransformVectorNoScale(LocalAlong).GetSafeNormal2D();
	M_Doorway.LeafCentre = LeafTransform.TransformPosition(LeafSpaceBox.GetCenter());
	M_Doorway.GroundCentre = M_Doorway.LeafCentre;
	float GroundZ = 0.f;
	if (TryGetGroundZ(M_Doorway.LeafCentre, GroundZ))
	{
		M_Doorway.GroundCentre.Z = GroundZ;
	}
	M_Doorway.DoorwayHalfWidthCm = (bIsThinAlongX ? WorldSize.Y : WorldSize.X) * HalfFactor;
	M_Doorway.FrameHalfExtentCm = GetFrameHalfExtentAlongWall(M_Doorway.DoorwayHalfWidthCm + FallbackFrameMarginCm);
	M_Doorway.ThroughDoorwayLimitCm = M_Doorway.DoorwayHalfWidthCm
		+ HalfFactor * FMath::Max(0.f, M_Doorway.FrameHalfExtentCm - M_Doorway.DoorwayHalfWidthCm);
	return M_Doorway.Normal.IsNormalized() && M_Doorway.Along.IsNormalized();
}

float UWallGateRelationsTestSubsystem::GetFrameHalfExtentAlongWall(const float FallbackHalfExtentCm) const
{
	TArray<FBox> FrameSpaceBoxes;
	FTransform FrameTransform;
	if (not M_Gate->GetFrameNavBoxes(FrameSpaceBoxes, FrameTransform))
	{
		return FallbackHalfExtentCm;
	}
	float HalfExtentCm = FallbackHalfExtentCm;
	for (const FBox& FrameSpaceBox : FrameSpaceBoxes)
	{
		FVector Corners[8];
		FrameSpaceBox.GetVertices(Corners);
		for (const FVector& Corner : Corners)
		{
			const FVector WorldCorner = FrameTransform.TransformPosition(Corner);
			const float AlongWallCm = FMath::Abs(
				FVector::DotProduct(WorldCorner - M_Doorway.GroundCentre, M_Doorway.Along));
			HalfExtentCm = FMath::Max(HalfExtentCm, AlongWallCm + M_Gate->GetFrameNavBoxPadding());
		}
	}
	return HalfExtentCm;
}

void UWallGateRelationsTestSubsystem::SpawnSecondGateForPlayer3(UClass* const GateClass)
{
	using namespace WallGateRelationsTestPrivate;
	FVector Location = M_Gate->GetActorLocation() - M_Doorway.Along * SecondGateOffsetCm;
	float GroundZ = Location.Z;
	(void)TryGetGroundZ(Location, GroundZ);
	Location.Z = GroundZ;
	// Owner set before BeginPlay, AllowedPlayer left at the class default: the gate must take player 3 as allowed.
	AWallGate* const SecondGate = Cast<AWallGate>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), GateClass, FTransform(M_Gate->GetActorRotation(), Location), SecondGatePlayer));
	M_SecondGate = SecondGate;
	FBox LeafSpaceBox;
	FTransform LeafTransform;
	if (not IsValid(SecondGate) || not SecondGate->GetDoorwayNavBox(LeafSpaceBox, LeafTransform))
	{
		RecordCheck(TEXT("second_gate_spawn_P3"), false, TEXT("spawn failed or no doorway box"));
		return;
	}
	M_SecondGateDoorwayGround = LeafTransform.TransformPosition(LeafSpaceBox.GetCenter());
	float DoorwayGroundZ = M_SecondGateDoorwayGround.Z;
	if (TryGetGroundZ(M_SecondGateDoorwayGround, DoorwayGroundZ))
	{
		M_SecondGateDoorwayGround.Z = DoorwayGroundZ;
	}
}

void UWallGateRelationsTestSubsystem::CheckSecondGate()
{
	using namespace WallGateRelationsTestPrivate;
	const AWallGate* const SecondGate = M_SecondGate.Get();
	if (not IsValid(SecondGate))
	{
		RecordCheck(TEXT("second_gate_allowed_P3"), false, TEXT("no second gate"));
		return;
	}
	const URTSComponent* const SecondGateRTSComponent = SecondGate->GetRTSComponent();
	const int32 SecondGateOwner = IsValid(SecondGateRTSComponent)
		                              ? SecondGateRTSComponent->GetOwningPlayer()
		                              : INDEX_NONE;
	RecordCheck(TEXT("second_gate_allowed_P3"),
	            SecondGate->GetAllowedPlayer() == SecondGatePlayer && SecondGateOwner == SecondGatePlayer,
	            FString::Printf(TEXT("allowed_player=%d owner=%d"), SecondGate->GetAllowedPlayer(), SecondGateOwner));
	FString Detail;
	const bool bHasPlayer3Area = GetIsDoorwayNavReady(
		RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(SecondGatePlayer), M_SecondGateDoorwayGround, Detail);
	RecordCheck(TEXT("second_gate_doorway_area_P3"), bHasPlayer3Area, Detail);
}

void UWallGateRelationsTestSubsystem::SpawnParkedTank(const uint8 Player, const float ParkYawDegrees)
{
	using namespace WallGateRelationsTestPrivate;
	UClass* const TankClass = LoadClass<AActor>(nullptr, TankClassPath);
	if (not IsValid(TankClass))
	{
		return;
	}
	const FVector ParkDirection = M_Doorway.Normal.RotateAngleAxis(ParkYawDegrees, FVector::UpVector);
	FVector Park = M_Doorway.GroundCentre + ParkDirection * ParkRadiusCm;
	float GroundZ = Park.Z;
	(void)TryGetGroundZ(Park, GroundZ);
	Park.Z = GroundZ;
	M_ParkByPlayer[Player] = Park;
	const FRotator FacingDoorway = (-ParkDirection).Rotation();
	AActor* const Tank = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), TankClass, FTransform(FacingDoorway, Park + FVector(0.f, 0.f, SpawnLiftCm)), Player);
	M_TankByPlayer[Player] = Tank;
	UE_LOG(LogRTSWallGateTest, Display, TEXT("RTS_WALLGATE spawned tank P%d at %s valid=%d"), Player,
	       *Park.ToCompactString(), IsValid(Tank) ? 1 : 0);
}

bool UWallGateRelationsTestSubsystem::GetIsAreaClearOfOtherUnits(FString& OutBlockingUnit) const
{
	using namespace WallGateRelationsTestPrivate;
	const UGameUnitManager* const GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		OutBlockingUnit = TEXT("no_game_unit_manager");
		return false;
	}
	TArray<const AActor*> RegisteredUnits;
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		const uint8 PlayerIndex = static_cast<uint8>(Player);
		for (const ATankMaster* const Tank : GameUnitManager->GetTanksOfPlayerView(PlayerIndex))
		{
			RegisteredUnits.Add(Tank);
		}
		for (const ASquadUnit* const SquadUnit : GameUnitManager->GetSquadUnitsOfPlayerView(PlayerIndex))
		{
			RegisteredUnits.Add(SquadUnit);
		}
	}
	const double ClearRadiusSquared = FMath::Square(static_cast<double>(ForeignUnitClearRadiusCm));
	for (const AActor* const Unit : RegisteredUnits)
	{
		const bool bIsNear = IsValid(Unit)
			&& FVector::DistSquared2D(Unit->GetActorLocation(), M_Doorway.GroundCentre) <= ClearRadiusSquared;
		if (bIsNear)
		{
			OutBlockingUnit = Unit->GetName();
			return false;
		}
	}
	return true;
}

void UWallGateRelationsTestSubsystem::TickWaitReady(const TSubclassOf<UNavArea> ExpectedClosedArea,
                                                    const EWallGateRelationsTestStep NextStep)
{
	using namespace WallGateRelationsTestPrivate;
	if (GetStepSeconds() < SettleGameSeconds)
	{
		return;
	}
	if (not M_Gate->GetUsesPlayerAwareNavArea())
	{
		RecordCheck(TEXT("nav_mode_player_aware"), false,
		            TEXT("gate fell back to BlockEveryone: an RTS nav filter does not derive from URTSWallGateNavQueryFilter"));
		Finish(false, TEXT("gate_nav_fallback_block_everyone"));
		return;
	}
	FString Detail;
	if (GetIsDoorwayNavReady(ExpectedClosedArea, M_Doorway.GroundCentre, Detail))
	{
		RecordCheck(GetStepName() + TEXT("_doorway_area"), true, Detail);
		EnterStep(NextStep);
		return;
	}
	M_LastWaitDetail = Detail;
	if (GetStepSeconds() < NavReadyTimeoutGameSeconds)
	{
		return;
	}
	RecordCheck(GetStepName() + TEXT("_doorway_area"), false, Detail);
	Finish(false, TEXT("doorway_nav_area_not_ready"));
}

bool UWallGateRelationsTestSubsystem::GetIsDoorwayNavReady(const TSubclassOf<UNavArea> ExpectedClosedArea,
                                                           const FVector& DoorwayGroundCentre, FString& OutDetail) const
{
	using namespace WallGateRelationsTestPrivate;
	UNavigationSystemV1* const NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ARecastNavMesh* const NavMesh = GetCharacterNavMesh(GetWorld());
	if (NavigationSystem == nullptr || not IsValid(NavMesh))
	{
		OutDetail = TEXT("no_character_navmesh");
		return false;
	}
	if (NavigationSystem->IsNavigationBuildInProgress())
	{
		OutDetail = TEXT("navigation_build_in_progress");
		return false;
	}
	const int32 ExpectedAreaId = NavMesh->GetAreaID(ExpectedClosedArea);
	if (ExpectedAreaId == INDEX_NONE)
	{
		OutDetail = FString::Printf(TEXT("area_not_registered=%s"), *GetNameSafe(ExpectedClosedArea));
		return false;
	}
	const NavNodeRef DoorwayPoly = NavMesh->FindNearestPoly(DoorwayGroundCentre, PolyQueryExtent);
	if (DoorwayPoly == INVALID_NAVNODEREF)
	{
		OutDetail = TEXT("no_navmesh_polygon_at_doorway");
		return false;
	}
	const uint32 DoorwayAreaId = NavMesh->GetPolyAreaID(DoorwayPoly);
	OutDetail = FString::Printf(TEXT("doorway_area_id=%u expected=%d(%s)"), DoorwayAreaId, ExpectedAreaId,
	                            *GetNameSafe(ExpectedClosedArea));
	return DoorwayAreaId == static_cast<uint32>(ExpectedAreaId);
}

// ---------------------------------------------------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------------------------------------------------

void UWallGateRelationsTestSubsystem::RunFreeForAllChecks()
{
	using namespace WallGateRelationsTestPrivate;
	FString BlockingUnit;
	if (not GetIsAreaClearOfOtherUnits(BlockingUnit))
	{
		Finish(false, FString::Printf(TEXT("map_unit_near_gate=%s_pass_-WallGateTestAt"), *BlockingUnit));
		return;
	}
	const ARecastNavMesh* const NavMesh = GetCharacterNavMesh(GetWorld());
	for (const UClass* const AreaClass : {UNavArea_WallGatePlayer3::StaticClass(), UNavArea_WallGatePlayer4::StaticClass()})
	{
		const int32 AreaId = IsValid(NavMesh) ? NavMesh->GetAreaID(AreaClass) : INDEX_NONE;
		RecordCheck(FString::Printf(TEXT("area_registered_%s"), *AreaClass->GetName()), AreaId != INDEX_NONE,
		            FString::Printf(TEXT("area_id=%d"), AreaId));
	}
	CheckSecondGate();
	// Owner and allowed player 2, free for all: only player 2's shots pass the closed leaf.
	CheckShotOfPlayer(HumanPlayer, true, TEXT("ffa"));
	CheckShotOfPlayer(GatePlayer, false, TEXT("ffa"));
	CheckShotOfPlayer(HostileAIPlayer, true, TEXT("ffa"));
	CheckShotOfPlayer(CapturingPlayer, true, TEXT("ffa"));
	CheckPathOfPlayer(GatePlayer, true, TEXT("ffa"));
	CheckPathOfPlayer(HostileAIPlayer, false, TEXT("ffa"));
	CheckPathOfPlayer(HumanPlayer, false, TEXT("ffa"));
	EnterStep(EWallGateRelationsTestStep::OwnerApproach);
}

void UWallGateRelationsTestSubsystem::RunTeamChecks()
{
	using namespace WallGateRelationsTestPrivate;
	FRTSPlayers::SetPlayerTeam(HostileAIPlayer, SharedTeamOfGateAndHostile);
	const bool bAreTeamsApplied = FRTSPlayers::AreAllied(GatePlayer, HostileAIPlayer)
		&& FRTSPlayers::AreHostile(HumanPlayer, GatePlayer) && FRTSPlayers::AreHostile(HumanPlayer, HostileAIPlayer);
	RecordCheck(TEXT("teams_2_and_3_allied"), bAreTeamsApplied,
	            FString::Printf(TEXT("relations_version=%u"), FRTSPlayers::GetRelationsVersion()));
	// The gate's owner did not change: allied player 3's shots now pass too, the human's still stop.
	CheckShotOfPlayer(GatePlayer, false, TEXT("team"));
	CheckShotOfPlayer(HostileAIPlayer, false, TEXT("team"));
	CheckShotOfPlayer(HumanPlayer, true, TEXT("team"));
	CheckShotOfPlayer(CapturingPlayer, true, TEXT("team"));
	// No navigation rebuild happened: player 3's next path request sees the new relations.
	CheckPathOfPlayer(HostileAIPlayer, true, TEXT("team"));
	CheckPathOfPlayer(GatePlayer, true, TEXT("team"));
	CheckPathOfPlayer(HumanPlayer, false, TEXT("team"));
	EnterStep(EWallGateRelationsTestStep::AllyApproach);
}

void UWallGateRelationsTestSubsystem::StartCapture()
{
	using namespace WallGateRelationsTestPrivate;
	FRTSPlayers::ResetToFreeForAll();
	URTSComponent* const GateRTSComponent = M_Gate->GetRTSComponent();
	if (not IsValid(GateRTSComponent))
	{
		Finish(false, TEXT("gate_without_rts_component"));
		return;
	}
	GateRTSComponent->SetOwningPlayerRuntime(CapturingPlayer);
	RecordCheck(TEXT("capture_allowed_player_follows_owner"),
	            M_Gate->GetAllowedPlayer() == CapturingPlayer && GateRTSComponent->GetOwningPlayer() == CapturingPlayer,
	            FString::Printf(TEXT("allowed_player=%d owner=%d"), M_Gate->GetAllowedPlayer(),
	                            GateRTSComponent->GetOwningPlayer()));
}

void UWallGateRelationsTestSubsystem::RunCaptureChecks()
{
	using namespace WallGateRelationsTestPrivate;
	// Captured by player 4, free for all: only player 4's shots pass, nobody else plans through.
	CheckShotOfPlayer(CapturingPlayer, false, TEXT("capture"));
	CheckShotOfPlayer(GatePlayer, true, TEXT("capture"));
	CheckShotOfPlayer(HostileAIPlayer, true, TEXT("capture"));
	CheckShotOfPlayer(HumanPlayer, true, TEXT("capture"));
	CheckPathOfPlayer(GatePlayer, false, TEXT("capture"));
	CheckPathOfPlayer(HostileAIPlayer, false, TEXT("capture"));
	EnterStep(EWallGateRelationsTestStep::FormerOwnerApproach);
}

void UWallGateRelationsTestSubsystem::CheckShotOfPlayer(const uint8 Player, const bool bExpectHit, const TCHAR* Stage)
{
	const bool bDidHit = GetDoesShotOfPlayerHitGate(Player);
	RecordCheck(FString::Printf(TEXT("%s_shot_P%d"), Stage, Player), bDidHit == bExpectHit,
	            FString::Printf(TEXT("expected=%s got=%s"), bExpectHit ? TEXT("hit") : TEXT("pass"),
	                            bDidHit ? TEXT("hit") : TEXT("pass")));
}

bool UWallGateRelationsTestSubsystem::GetDoesShotOfPlayerHitGate(const uint8 Player) const
{
	using namespace WallGateRelationsTestPrivate;
	// Off the centre line: a twin-leaf gate may leave a seam between its leaves there.
	const FVector Target = M_Doorway.LeafCentre + M_Doorway.Along * (M_Doorway.DoorwayHalfWidthCm * ShotSideFraction);
	const FVector Start = Target + M_Doorway.Normal * ShotStartDistanceCm;
	const FVector End = Target - M_Doorway.Normal * ShotEndBehindCm;
	FCollisionQueryParams QueryParams(FName(TEXT("WallGateRelationsShot")), false);
	FRTSPlayerCollision::ApplyWeaponIgnoreMask(QueryParams, Player);
	FHitResult Hit;
	if (not GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FRTSPlayerCollision::GetWeaponTraceChannel(Player),
	                                             QueryParams))
	{
		return false;
	}
	return Hit.GetActor() == M_Gate.Get();
}

void UWallGateRelationsTestSubsystem::CheckPathOfPlayer(const uint8 Player, const bool bExpectThrough,
                                                        const TCHAR* Stage)
{
	FString Detail;
	const EWallGatePathOutcome Outcome = FindPathOutcome(Player, Detail);
	const bool bIsThrough = Outcome == EWallGatePathOutcome::ThroughGate;
	const bool bPassed = Outcome != EWallGatePathOutcome::SetupFailed && bIsThrough == bExpectThrough;
	RecordCheck(FString::Printf(TEXT("%s_path_P%d"), Stage, Player), bPassed,
	            FString::Printf(TEXT("expected=%s got=%s %s"), bExpectThrough ? TEXT("through_gate") : TEXT("not_through"),
	                            WallGateRelationsTestPrivate::GetOutcomeName(Outcome), *Detail));
}

EWallGatePathOutcome UWallGateRelationsTestSubsystem::FindPathOutcome(const uint8 Player, FString& OutDetail) const
{
	using namespace WallGateRelationsTestPrivate;
	UNavigationSystemV1* const NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ARecastNavMesh* const NavMesh = GetCharacterNavMesh(GetWorld());
	const URTSNavQueryFilterSettings* const FilterSettings = URTSNavQueryFilterSettings::Get();
	const AActor* const Querier = M_TankByPlayer[Player].Get();
	if (NavigationSystem == nullptr || not IsValid(NavMesh) || FilterSettings == nullptr || not IsValid(Querier))
	{
		OutDetail = TEXT("missing_navigation_filter_settings_or_tank");
		return EWallGatePathOutcome::SetupFailed;
	}
	// The filter every infantry unit plans with; built for the tank, whose RTS component names the player.
	const TSubclassOf<UNavigationQueryFilter> FilterClass = FilterSettings->ResolveFilterClassForAgent(
		ERTSNavAgents::Character);
	const FSharedConstNavQueryFilter QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavMesh, Querier,
		FilterClass);
	FNavLocation Start;
	FNavLocation End;
	const bool bAreEndsOnNavMesh = NavigationSystem->ProjectPointToNavigation(
			M_Doorway.GroundCentre + M_Doorway.Normal * PathProbeDistanceCm, Start, ProjectExtent, NavMesh)
		&& NavigationSystem->ProjectPointToNavigation(
			M_Doorway.GroundCentre - M_Doorway.Normal * PathProbeDistanceCm, End, ProjectExtent, NavMesh);
	if (not bAreEndsOnNavMesh || not QueryFilter.IsValid())
	{
		OutDetail = bAreEndsOnNavMesh ? TEXT("no_query_filter") : TEXT("path_ends_off_navmesh");
		return EWallGatePathOutcome::SetupFailed;
	}
	const FPathFindingQuery Query(Querier, *NavMesh, Start.Location, End.Location, QueryFilter);
	const FPathFindingResult Result = NavigationSystem->FindPathSync(Query);
	if (not Result.IsSuccessful() || Result.IsPartial() || not Result.Path.IsValid())
	{
		OutDetail = FString::Printf(TEXT("filter=%s result=%d partial=%d"), *GetNameSafe(FilterClass),
		                            static_cast<int32>(Result.Result), Result.IsPartial() ? 1 : 0);
		return EWallGatePathOutcome::NoPath;
	}
	TArray<FVector> PathPoints;
	for (const FNavPathPoint& PathPoint : Result.Path->GetPathPoints())
	{
		PathPoints.Add(PathPoint.Location);
	}
	const EWallGatePathOutcome Outcome = ClassifyPath(PathPoints, OutDetail);
	OutDetail += FString::Printf(TEXT(" filter=%s length_cm=%.0f"), *GetNameSafe(FilterClass), Result.Path->GetLength());
	return Outcome;
}

EWallGatePathOutcome UWallGateRelationsTestSubsystem::ClassifyPath(const TArray<FVector>& PathPoints,
                                                                   FString& OutDetail) const
{
	const float ClosestCrossingCm = WallGateRelationsTestPrivate::GetClosestWallLineCrossing(PathPoints, M_Doorway);
	OutDetail = FString::Printf(TEXT("points=%d closest_wall_crossing_cm=%.0f frame_half_extent_cm=%.0f"),
	                            PathPoints.Num(), ClosestCrossingCm, M_Doorway.FrameHalfExtentCm);
	OutDetail += FString::Printf(TEXT(" through_limit_cm=%.0f"), M_Doorway.ThroughDoorwayLimitCm);
	// Crossing the wall line at the doorway, not round the frame's end (the frame's own boxes are blocked).
	return ClosestCrossingCm < M_Doorway.ThroughDoorwayLimitCm
		       ? EWallGatePathOutcome::ThroughGate
		       : EWallGatePathOutcome::AroundGate;
}

void UWallGateRelationsTestSubsystem::RecordCheck(const FString& CheckName, const bool bPassed, const FString& Detail)
{
	const FString Line = FString::Printf(TEXT("RTS_WALLGATE CHECK %s %s %s"), *CheckName,
	                                     bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
	UE_LOG(LogRTSWallGateTest, Display, TEXT("%s"), *Line);
	M_ReportLines.Add(Line);
	if (not bPassed)
	{
		M_FailedChecks.Add(CheckName);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Approaches
// ---------------------------------------------------------------------------------------------------------------------

void UWallGateRelationsTestSubsystem::TeleportTankTo(const uint8 Player, const FVector& GroundLocation) const
{
	AActor* const Tank = M_TankByPlayer[Player].Get();
	if (not IsValid(Tank))
	{
		return;
	}
	const FVector Destination = GroundLocation + FVector(0.f, 0.f, WallGateRelationsTestPrivate::TeleportLiftCm);
	Tank->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	UPrimitiveComponent* const RootPrimitive = Cast<UPrimitiveComponent>(Tank->GetRootComponent());
	if (IsValid(RootPrimitive) && RootPrimitive->IsSimulatingPhysics())
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
	}
}

FVector UWallGateRelationsTestSubsystem::GetApproachLocation() const
{
	FVector Approach = M_Doorway.GroundCentre + M_Doorway.Normal * WallGateRelationsTestPrivate::ApproachDistanceCm;
	float GroundZ = Approach.Z;
	(void)TryGetGroundZ(Approach, GroundZ);
	Approach.Z = GroundZ;
	return Approach;
}

void UWallGateRelationsTestSubsystem::EnterStep(const EWallGateRelationsTestStep NewStep)
{
	using namespace WallGateRelationsTestPrivate;
	M_Step = NewStep;
	M_StepStartGameSeconds = GetGameSeconds();
	M_LastWaitDetail.Reset();
	UE_LOG(LogRTSWallGateTest, Display, TEXT("RTS_WALLGATE step %s at_s=%.1f"), *GetStepName(), M_StepStartGameSeconds);
	const uint8 ApproachingPlayer = GetApproachingPlayerOfStep(NewStep);
	if (ApproachingPlayer != RTSPlayers::NeutralOwner)
	{
		TeleportTankTo(ApproachingPlayer, GetApproachLocation());
	}
	const uint8 LeavingPlayer = GetLeavingPlayerOfStep(NewStep);
	if (LeavingPlayer != RTSPlayers::NeutralOwner)
	{
		TeleportTankTo(LeavingPlayer, M_ParkByPlayer[LeavingPlayer]);
	}
	if (NewStep == EWallGateRelationsTestStep::CaptureWaitReady)
	{
		StartCapture();
	}
	if (NewStep == EWallGateRelationsTestStep::Done)
	{
		Finish(true, FString());
	}
}

void UWallGateRelationsTestSubsystem::TickApproach(const uint8 Player, const bool bExpectOpen,
                                                   const EWallGateRelationsTestStep NextStep)
{
	using namespace WallGateRelationsTestPrivate;
	if (not RTSFunctionLibrary::RTSIsValid(M_TankByPlayer[Player].Get()))
	{
		Finish(false, FString::Printf(TEXT("tank_lost_P%d"), Player));
		return;
	}
	const FString CheckName = FString::Printf(TEXT("%s_P%d"), *GetStepName(), Player);
	const bool bIsOpening = GetIsGateOpeningOrOpen();
	const FString Detail = FString::Printf(TEXT("expected=%s state=%s after_s=%.1f allowed_player=%d"),
	                                       bExpectOpen ? TEXT("opens") : TEXT("stays_closed"),
	                                       GetGateStateName(M_Gate->GetGateState()), GetStepSeconds(),
	                                       M_Gate->GetAllowedPlayer());
	const float VerdictSeconds = bExpectOpen ? OpenTimeoutGameSeconds : HoldClosedGameSeconds;
	if (not bIsOpening && GetStepSeconds() < VerdictSeconds)
	{
		return;
	}
	// Without the tank at the doorway a gate that stays shut proves nothing.
	const float TankDistanceCm = FVector::Dist2D(M_TankByPlayer[Player]->GetActorLocation(), M_Doorway.GroundCentre);
	const bool bIsTankAtDoorway = TankDistanceCm <= ApproachDistanceCm + ApproachToleranceCm;
	RecordCheck(CheckName, bIsTankAtDoorway && bIsOpening == bExpectOpen,
	            Detail + FString::Printf(TEXT(" tank_distance_cm=%.0f"), TankDistanceCm));
	// A tank that opened the gate leaves in the next step, which waits for the gate to shut again.
	if (not (bIsOpening && bExpectOpen))
	{
		TeleportTankTo(Player, M_ParkByPlayer[Player]);
	}
	EnterStep(NextStep);
}

void UWallGateRelationsTestSubsystem::TickLeave(const EWallGateRelationsTestStep NextStep)
{
	using namespace WallGateRelationsTestPrivate;
	FString Detail;
	const bool bIsClosed = M_Gate->GetGateState() == EWallGateState::Closed;
	const TSubclassOf<UNavArea> ClosedArea = RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(M_Gate->GetAllowedPlayer());
	if (bIsClosed && GetIsDoorwayNavReady(ClosedArea, M_Doorway.GroundCentre, Detail))
	{
		RecordCheck(GetStepName() + TEXT("_closed_again"), true,
		            FString::Printf(TEXT("after_s=%.1f %s"), GetStepSeconds(), *Detail));
		EnterStep(NextStep);
		return;
	}
	if (GetStepSeconds() < CloseTimeoutGameSeconds)
	{
		return;
	}
	RecordCheck(GetStepName() + TEXT("_closed_again"), false,
	            FString::Printf(TEXT("state=%s %s"), GetGateStateName(M_Gate->GetGateState()), *Detail));
	Finish(false, TEXT("gate_did_not_close"));
}

bool UWallGateRelationsTestSubsystem::GetIsGateOpeningOrOpen() const
{
	const EWallGateState State = M_Gate->GetGateState();
	return State == EWallGateState::Opening || State == EWallGateState::Open;
}

bool UWallGateRelationsTestSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace WallGateRelationsTestPrivate;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallGateRelationsGround), false);
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

float UWallGateRelationsTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UWallGateRelationsTestSubsystem::GetStepSeconds() const
{
	return GetGameSeconds() - M_StepStartGameSeconds;
}

FString UWallGateRelationsTestSubsystem::GetStepName() const
{
	switch (M_Step)
	{
	case EWallGateRelationsTestStep::StartGame: return TEXT("start_game");
	case EWallGateRelationsTestStep::Spawn: return TEXT("spawn");
	case EWallGateRelationsTestStep::WaitReady: return TEXT("wait_ready");
	case EWallGateRelationsTestStep::FreeForAllChecks: return TEXT("ffa_checks");
	case EWallGateRelationsTestStep::OwnerApproach: return TEXT("owner_approach");
	case EWallGateRelationsTestStep::OwnerLeave: return TEXT("owner_leave");
	case EWallGateRelationsTestStep::HostileApproach: return TEXT("hostile_approach");
	case EWallGateRelationsTestStep::HumanApproach: return TEXT("human_approach");
	case EWallGateRelationsTestStep::TeamChecks: return TEXT("team_checks");
	case EWallGateRelationsTestStep::AllyApproach: return TEXT("ally_approach");
	case EWallGateRelationsTestStep::AllyLeave: return TEXT("ally_leave");
	case EWallGateRelationsTestStep::CaptureWaitReady: return TEXT("capture_wait_ready");
	case EWallGateRelationsTestStep::CaptureChecks: return TEXT("capture_checks");
	case EWallGateRelationsTestStep::FormerOwnerApproach: return TEXT("former_owner_approach");
	default: return TEXT("done");
	}
}

void UWallGateRelationsTestSubsystem::Finish(const bool bPassed, const FString& Reason)
{
	if (M_Step == EWallGateRelationsTestStep::Done && not bM_IsActive)
	{
		return;
	}
	const FString StoppedInStep = GetStepName();
	M_Step = EWallGateRelationsTestStep::Done;
	bM_IsActive = false;
	TArray<FString> Reasons;
	if (not Reason.IsEmpty())
	{
		Reasons.Add(Reason);
	}
	Reasons.Append(M_FailedChecks);
	const bool bIsPass = bPassed && Reasons.IsEmpty();
	const FString ReasonText = Reasons.IsEmpty() ? FString(TEXT("none")) : FString::Join(Reasons, TEXT(";"));
	const FString ResultLine = FString::Printf(TEXT("RTS_WALLGATE RESULT %s reasons=%s step=%s checks=%d failed=%d"),
	                                           bIsPass ? TEXT("PASS") : TEXT("FAIL"), *ReasonText, *StoppedInStep,
	                                           M_ReportLines.Num(), M_FailedChecks.Num());
	TArray<FString> SummaryLines;
	SummaryLines.Add(TEXT("# Wall gate relations test"));
	SummaryLines.Add(FString());
	SummaryLines.Add(ResultLine);
	SummaryLines.Add(FString());
	SummaryLines.Append(M_ReportLines);
	if (not M_LastWaitDetail.IsEmpty())
	{
		SummaryLines.Add(TEXT("last wait: ") + M_LastWaitDetail);
	}
	FFileHelper::SaveStringToFile(FString::Join(SummaryLines, TEXT("\n")) + TEXT("\n"),
	                              *FPaths::Combine(M_OutputDirectory, TEXT("summary.md")));
	UE_LOG(LogRTSWallGateTest, Display, TEXT("%s"), *ResultLine);
	FPlatformMisc::RequestExit(false, TEXT("WallGateRelationsTest"));
}

#else

bool UWallGateRelationsTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UWallGateRelationsTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UWallGateRelationsTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UWallGateRelationsTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWallGateRelationsTestSubsystem, STATGROUP_Tickables);
}

#endif
