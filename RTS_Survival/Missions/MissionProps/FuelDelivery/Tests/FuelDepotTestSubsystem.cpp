// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FuelDepotTestSubsystem.h"

#if RTS_WITH_MAP_TESTS
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/Missions/MissionProps/FuelCrane/MissionFuelCrane.h"
#include "RTS_Survival/Missions/MissionProps/FuelDelivery/FuelDeliveryDepotComponent.h"
#include "RTS_Survival/Missions/MissionProps/FuelDelivery/FuelDeliveryNodeComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFuelDepotTest, Display, All);

namespace FuelDepotTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("FuelDepotValidate");
	const TCHAR* const AtSwitch = TEXT("FuelDepotAt=");
	const TCHAR* const YawSwitch = TEXT("FuelDepotYaw=");
	const TCHAR* const ClassSwitch = TEXT("FuelDepotClass=");
	const TCHAR* const CaptureSwitch = TEXT("FuelDepotCapture");
	// Spawns the depot from OnWorldBeginPlay, before any actor began play: like a depot placed on the map.
	const TCHAR* const SpawnAtLoadSwitch = TEXT("FuelDepotSpawnAtLoad");
	const TCHAR* const DefaultClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/"
		"Rus_BXP_LongCamoBunker_FuelDepot.Rus_BXP_LongCamoBunker_FuelDepot_C");
	const FVector DefaultLocation(16796.f, -15808.f, 0.f);
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 30.0 * 60.0;
	constexpr float StepTimeoutSeconds = 300.f;
	constexpr float FirstTruckDelay = 2.f;
	constexpr float RefillSeconds = 6.f;
	constexpr float RespawnSeconds = 6.f;
	// Truck 2 is destroyed this long after it spawned (on its way to the park node).
	constexpr float SecondTruckLifeSeconds = 8.f;
	// The depot is destroyed this long after truck 3 spawned.
	constexpr float ThirdTruckLifeSeconds = 8.f;
	// After truck 3 is gone no truck may appear for this long (well over the refill and respawn delays).
	constexpr float NoTruckWatchSeconds = 20.f;
	constexpr int32 ExpectedCargoPieces = 6;
	constexpr float ParkDistanceLimitCm = 400.f;
	constexpr float ParkYawLimitDegrees = 20.f;
	constexpr float CaptureIntervalSeconds = 12.f;
	constexpr float CraneNodeToleranceCm = 10.f;
	constexpr float DepotGoneGraceSeconds = 10.f;
	constexpr int32 CaptureWidth = 1280;
	constexpr int32 CaptureHeight = 720;
}
#endif

bool UFuelDepotTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), FuelDepotTestPrivate::TestSwitch);
#else
	return false;
#endif
}

TStatId UFuelDepotTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFuelDepotTestSubsystem, STATGROUP_Tickables);
}

void UFuelDepotTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
#if RTS_WITH_MAP_TESTS
	ReadCommandLine();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*M_OutputDirectory);
	if (bM_Capture)
	{
		if (IConsoleVariable* const TextureStreaming = IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.TextureStreaming")))
		{
			TextureStreaming->Set(0, ECVF_SetByCode);
		}
	}
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT started class=%s at=%s yaw=%.0f spawn_at_load=%d dir=%s"), *M_ClassPath,
	                    *M_Location.ToCompactString(), M_Yaw, bM_SpawnAtLoad ? 1 : 0, *M_OutputDirectory));
	if (bM_SpawnAtLoad)
	{
		SpawnDepot();
	}
#endif
}

void UFuelDepotTestSubsystem::Tick(float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	using namespace FuelDepotTestPrivate;
	if (not bM_IsActive || M_Step == EFuelDepotTestStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		Fail(TEXT("real_time_watchdog"));
		Finish();
		return;
	}
	KeepGameRunning();
	if (not bM_HasStartedGame)
	{
		bM_HasStartedGame = RealSeconds >= HudBuildSeconds && TryStartGame();
		if (not bM_HasStartedGame)
		{
			return;
		}
		if (M_Step == EFuelDepotTestStep::StartGame)
		{
			SetStep(EFuelDepotTestStep::Spawn);
		}
	}
	if (M_Step == EFuelDepotTestStep::Spawn)
	{
		SpawnDepot();
		return;
	}
	if (GetStepSeconds() > StepTimeoutSeconds)
	{
		Fail(FString::Printf(TEXT("timeout_step_%d"), static_cast<int32>(M_Step)));
		Finish();
		return;
	}
	SuspendAITrainingOnce();
	TickStep();
	M_LastTickAIPoints = GetAIPoints();
#endif
}

void UFuelDepotTestSubsystem::HandleTruckSpawned(AActor* Truck)
{
#if RTS_WITH_MAP_TESTS
	++M_SpawnedTrucks;
	M_CurrentTruck = Truck;
	M_CurrentTruckSpawnSeconds = GetGameSeconds();
	Log(FString::Printf(TEXT("RTS_FUELDEPOT event spawned #%d %s at %s yaw=%.0f owner=%d cargo=%d t=%.1f"),
	                    M_SpawnedTrucks, *GetNameSafe(Truck),
	                    IsValid(Truck) ? *Truck->GetActorLocation().ToCompactString() : TEXT("-"),
	                    IsValid(Truck) ? Truck->GetActorRotation().Yaw : 0.f,
	                    FRTSPlayers::GetOwningPlayerOfActor(Truck), CountVisibleCargo(Truck), GetGameSeconds()));
#endif
}

void UFuelDepotTestSubsystem::HandleTruckUnloaded(AActor* Truck, const int32 TrainingPointsGranted)
{
#if RTS_WITH_MAP_TESTS
	++M_UnloadedTrucks;
	M_PointsGrantedByEvent += TrainingPointsGranted;
	// The grant happened just before this event, within this tick; the per-minute income cannot hide in that gap.
	M_AIPointsDeltaAtGrant = GetAIPoints() - M_LastTickAIPoints;
	M_UnloadedAtSeconds = GetGameSeconds();
	Log(FString::Printf(TEXT("RTS_FUELDEPOT event unloaded %s points=%d cargo_left=%d t=%.1f"), *GetNameSafe(Truck),
	                    TrainingPointsGranted, CountVisibleCargo(Truck), GetGameSeconds()));
#endif
}

void UFuelDepotTestSubsystem::HandleTruckDespawned(AActor* Truck)
{
#if RTS_WITH_MAP_TESTS
	++M_DespawnedTrucks;
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	const UFuelDeliveryNodeComponent* const SpawnNode = IsValid(Depot)
		                                                    ? Depot->MapTest_GetNode(EFuelDeliveryNodeRole::TruckSpawn)
		                                                    : nullptr;
	const float DistanceHome = IsValid(Truck) && IsValid(SpawnNode)
		                           ? FVector::Dist2D(Truck->GetActorLocation(), SpawnNode->GetComponentLocation())
		                           : -1.f;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT event despawned %s alive=%d dist_home=%.0f t=%.1f"), *GetNameSafe(Truck),
	                    IsValid(Truck) && Truck->IsUnitAlive() ? 1 : 0, DistanceHome, GetGameSeconds()));
#endif
}

void UFuelDepotTestSubsystem::HandleTruckDestroyed(AActor* Truck)
{
#if RTS_WITH_MAP_TESTS
	++M_DestroyedTrucks;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT event destroyed %s t=%.1f"), *GetNameSafe(Truck), GetGameSeconds()));
#endif
}

#if RTS_WITH_MAP_TESTS

void UFuelDepotTestSubsystem::ReadCommandLine()
{
	using namespace FuelDepotTestPrivate;
	const TCHAR* const CommandLine = FCommandLine::Get();
	M_Location = DefaultLocation;
	FString LocationText;
	if (FParse::Value(CommandLine, AtSwitch, LocationText, false))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_Location = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	FParse::Value(CommandLine, YawSwitch, M_Yaw);
	M_ClassPath = DefaultClassPath;
	FParse::Value(CommandLine, ClassSwitch, M_ClassPath, false);
	bM_Capture = FParse::Param(CommandLine, CaptureSwitch);
	bM_SpawnAtLoad = FParse::Param(CommandLine, SpawnAtLoadSwitch);
	M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FuelDepotTest"),
	                                    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
}

void UFuelDepotTestSubsystem::SetStep(const EFuelDepotTestStep Step)
{
	M_Step = Step;
	M_StepStartGameSeconds = GetGameSeconds();
	Log(FString::Printf(TEXT("RTS_FUELDEPOT step %d t=%.1f"), static_cast<int32>(Step), M_StepStartGameSeconds));
}

float UFuelDepotTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UFuelDepotTestSubsystem::GetStepSeconds() const
{
	return GetGameSeconds() - M_StepStartGameSeconds;
}

bool UFuelDepotTestSubsystem::TryStartGame()
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

void UFuelDepotTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(FuelDepotTestPrivate::DifficultyButtonName)))
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

void UFuelDepotTestSubsystem::SpawnDepot()
{
	using namespace FuelDepotTestPrivate;
	UClass* const DepotClass = LoadClass<AActor>(nullptr, *M_ClassPath);
	if (not IsValid(DepotClass))
	{
		Fail(TEXT("no_class_") + M_ClassPath);
		Finish();
		return;
	}
	FVector Location = M_Location;
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FuelDepotTestGround), false);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Location + FVector(0.f, 0.f, 20000.f),
	                                         Location - FVector(0.f, 0.f, 20000.f), COLLISION_TRACE_LANDSCAPE,
	                                         QueryParams))
	{
		Location.Z = GroundHit.ImpactPoint.Z;
	}
	AActor* const Depot = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), DepotClass, FTransform(FRotator(0.f, M_Yaw, 0.f), Location),
		static_cast<uint8>(RTSPlayers::FirstAIPlayer));
	UFuelDeliveryDepotComponent* const DepotComponent = IsValid(Depot)
		                                                    ? Depot->FindComponentByClass<UFuelDeliveryDepotComponent>()
		                                                    : nullptr;
	if (not IsValid(DepotComponent))
	{
		Fail(IsValid(Depot) ? TEXT("no_depot_component") : TEXT("spawn_failed"));
		Finish();
		return;
	}
	M_Depot = Depot;
	M_DepotComponent = DepotComponent;
	DepotComponent->OnTruckSpawned.AddDynamic(this, &UFuelDepotTestSubsystem::HandleTruckSpawned);
	DepotComponent->OnTruckUnloaded.AddDynamic(this, &UFuelDepotTestSubsystem::HandleTruckUnloaded);
	DepotComponent->OnTruckDespawned.AddDynamic(this, &UFuelDepotTestSubsystem::HandleTruckDespawned);
	DepotComponent->OnTruckDestroyed.AddDynamic(this, &UFuelDepotTestSubsystem::HandleTruckDestroyed);
	DepotComponent->MapTest_OverrideTimings(FirstTruckDelay, RefillSeconds, RespawnSeconds);
	for (const EFuelDeliveryNodeRole Role : {EFuelDeliveryNodeRole::TruckSpawn, EFuelDeliveryNodeRole::TruckPark,
	                                         EFuelDeliveryNodeRole::Crane})
	{
		const UFuelDeliveryNodeComponent* const Node = DepotComponent->MapTest_GetNode(Role);
		Log(FString::Printf(TEXT("RTS_FUELDEPOT node %d at %s yaw=%.0f"), static_cast<int32>(Role),
		                    IsValid(Node) ? *Node->GetComponentLocation().ToCompactString() : TEXT("-"),
		                    IsValid(Node) ? Node->GetComponentRotation().Yaw : 0.f));
	}
	Log(FString::Printf(TEXT("RTS_FUELDEPOT depot %s at %s owner=%d ai_points=%d"), *Depot->GetName(),
	                    *Location.ToCompactString(), FRTSPlayers::GetOwningPlayerOfActor(Depot), M_AIPointsBefore));
	SetStep(EFuelDepotTestStep::FirstTruckSpawned);
}

void UFuelDepotTestSubsystem::TickStep()
{
	switch (M_Step)
	{
	case EFuelDepotTestStep::FirstTruckSpawned:
		TickFirstTruckSpawned();
		break;
	case EFuelDepotTestStep::FirstTruckParked:
		TickFirstTruckParked();
		break;
	case EFuelDepotTestStep::FirstTruckUnloaded:
		TickFirstTruckUnloaded();
		break;
	case EFuelDepotTestStep::FirstTruckDespawned:
		TickFirstTruckDespawned();
		break;
	case EFuelDepotTestStep::SecondTruckSpawned:
		TickSecondTruckSpawned();
		break;
	case EFuelDepotTestStep::SecondTruckDestroyed:
		TickSecondTruckDestroyed();
		break;
	case EFuelDepotTestStep::ThirdTruckSpawned:
		TickThirdTruckSpawned();
		break;
	case EFuelDepotTestStep::DepotDestroyed:
		TickDepotDestroyed();
		break;
	case EFuelDepotTestStep::NoMoreTrucks:
		TickNoMoreTrucks();
		break;
	default:
		break;
	}
}

void UFuelDepotTestSubsystem::SuspendAITrainingOnce()
{
	if (bM_HasSuspendedAITraining)
	{
		return;
	}
	const AEnemyController* const Controller = FRTS_Statics::GetEnemyControllerOfUnit(M_Depot.Get());
	UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
		                                                ? Controller->GetEnemyStrategicAIComponent()
		                                                : nullptr;
	if (not IsValid(StrategicAI))
	{
		return;
	}
	// Only the depot and the per-minute income may move the points while the test watches them.
	StrategicAI->MapTest_SetTrainingThinkSuspended(true);
	bM_HasSuspendedAITraining = true;
	M_AIPointsBefore = GetAIPoints();
	M_LastTickAIPoints = M_AIPointsBefore;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT ai training suspended, points=%d t=%.1f"), M_AIPointsBefore,
	                    GetGameSeconds()));
}

EFuelDeliveryState UFuelDepotTestSubsystem::GetDepotState() const
{
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	return IsValid(Depot) ? Depot->GetDeliveryState() : EFuelDeliveryState::Stopped;
}

void UFuelDepotTestSubsystem::CheckCraneAtNode()
{
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	AMissionFuelCrane* const Crane = IsValid(Depot) ? Depot->GetCrane() : nullptr;
	const UFuelDeliveryNodeComponent* const CraneNode = IsValid(Depot)
		                                                    ? Depot->MapTest_GetNode(EFuelDeliveryNodeRole::Crane)
		                                                    : nullptr;
	if (not IsValid(Crane) || not IsValid(CraneNode))
	{
		Fail(TEXT("no_crane_spawned"));
		return;
	}
	M_Crane = Crane;
	M_CraneCountBefore = Crane->GetUnloadedCount();
	const float CraneOffset = FVector::Dist2D(Crane->GetActorLocation(), CraneNode->GetComponentLocation());
	Log(FString::Printf(TEXT("RTS_FUELDEPOT crane %s at %s offset_from_node=%.0f"), *Crane->GetName(),
	                    *Crane->GetActorLocation().ToCompactString(), CraneOffset));
	if (CraneOffset > FuelDepotTestPrivate::CraneNodeToleranceCm)
	{
		Fail(TEXT("crane_not_at_node"));
	}
}

void UFuelDepotTestSubsystem::TickFirstTruckSpawned()
{
	if (M_SpawnedTrucks < 1)
	{
		return;
	}
	if (not bM_HasSuspendedAITraining)
	{
		Fail(TEXT("no_enemy_strategic_ai_for_depot_owner"));
	}
	CheckCraneAtNode();
	if (CountVisibleCargo(M_CurrentTruck.Get()) != FuelDepotTestPrivate::ExpectedCargoPieces)
	{
		Fail(TEXT("truck1_not_full"));
	}
	SetStep(EFuelDepotTestStep::FirstTruckParked);
}

void UFuelDepotTestSubsystem::TickFirstTruckParked()
{
	const EFuelDeliveryState State = GetDepotState();
	if (State == EFuelDeliveryState::DrivingHome || State == EFuelDeliveryState::WaitingForTruck)
	{
		LogTruckPose(TEXT("turned_back"));
		Fail(TEXT("truck1_never_unloaded"));
		Finish();
		return;
	}
	if (State != EFuelDeliveryState::Unloading)
	{
		return;
	}
	M_ParkedAtSeconds = GetGameSeconds();
	LogTruckPose(TEXT("parked"));
	if (bM_Capture)
	{
		Capture(TEXT("a_parked"));
		M_NextCaptureSeconds = GetGameSeconds() + FuelDepotTestPrivate::CaptureIntervalSeconds;
	}
	SetStep(EFuelDepotTestStep::FirstTruckUnloaded);
}

void UFuelDepotTestSubsystem::TickFirstTruckUnloaded()
{
	using namespace FuelDepotTestPrivate;
	if (bM_Capture && GetGameSeconds() >= M_NextCaptureSeconds && GetDepotState() == EFuelDeliveryState::Unloading)
	{
		Capture(FString::Printf(TEXT("b_unloading_%02d"), ++M_CaptureIndex));
		M_NextCaptureSeconds = GetGameSeconds() + CaptureIntervalSeconds;
	}
	if (M_UnloadedTrucks < 1)
	{
		return;
	}
	CheckFirstUnload();
	if (bM_Capture)
	{
		Capture(TEXT("c_unloaded"));
	}
	SetStep(EFuelDepotTestStep::FirstTruckDespawned);
}

void UFuelDepotTestSubsystem::CheckFirstUnload()
{
	using namespace FuelDepotTestPrivate;
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	const AMissionFuelCrane* const Crane = M_Crane.Get();
	const int32 Lifted = IsValid(Crane) ? Crane->GetUnloadedCount() - M_CraneCountBefore : -1;
	const int32 Expected = IsValid(Depot) ? Depot->MapTest_GetTrainingPointsPerDelivery() : 0;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT unload lifted=%d seconds=%.1f ai_points %d->%d delta_at_grant=%d "
		                    "expected+%d event+%d depot_total=%d"), Lifted, M_UnloadedAtSeconds - M_ParkedAtSeconds,
	                    M_AIPointsBefore, GetAIPoints(), M_AIPointsDeltaAtGrant, Expected, M_PointsGrantedByEvent,
	                    IsValid(Depot) ? Depot->GetTotalTrainingPointsGranted() : -1));
	if (Lifted != ExpectedCargoPieces)
	{
		Fail(FString::Printf(TEXT("lifted_%d_not_%d"), Lifted, ExpectedCargoPieces));
	}
	if (M_PointsGrantedByEvent != Expected || M_AIPointsDeltaAtGrant != Expected)
	{
		Fail(TEXT("training_points_not_granted"));
	}
	if (CountVisibleCargo(M_CurrentTruck.Get()) != 0)
	{
		Fail(TEXT("cargo_left_on_truck"));
	}
}

void UFuelDepotTestSubsystem::TickFirstTruckDespawned()
{
	if (M_DespawnedTrucks < 1)
	{
		return;
	}
	if (M_DestroyedTrucks != 0)
	{
		Fail(TEXT("truck1_counted_destroyed"));
	}
	if (GetDepotState() != EFuelDeliveryState::WaitingForTruck)
	{
		Fail(TEXT("not_waiting_after_despawn"));
	}
	SetStep(EFuelDepotTestStep::SecondTruckSpawned);
}

void UFuelDepotTestSubsystem::TickSecondTruckSpawned()
{
	using namespace FuelDepotTestPrivate;
	if (M_SpawnedTrucks < 2 || GetGameSeconds() - M_CurrentTruckSpawnSeconds < SecondTruckLifeSeconds)
	{
		return;
	}
	const int32 Cargo = CountVisibleCargo(M_CurrentTruck.Get());
	Log(FString::Printf(TEXT("RTS_FUELDEPOT truck2 cargo=%d state=%d; destroying it"), Cargo,
	                    static_cast<int32>(GetDepotState())));
	if (Cargo != ExpectedCargoPieces)
	{
		Fail(TEXT("truck2_not_full"));
	}
	AHpPawnMaster* const Pawn = Cast<AHpPawnMaster>(M_CurrentTruck.Get());
	if (not IsValid(Pawn))
	{
		Fail(TEXT("truck2_not_killable"));
		Finish();
		return;
	}
	Pawn->TriggerDestroyActor(ERTSDeathType::Kinetic);
	M_SecondTruckDestroyedAt = GetGameSeconds();
	SetStep(EFuelDepotTestStep::SecondTruckDestroyed);
}

void UFuelDepotTestSubsystem::TickSecondTruckDestroyed()
{
	using namespace FuelDepotTestPrivate;
	if (M_DestroyedTrucks < 1 || M_SpawnedTrucks < 3)
	{
		return;
	}
	const float Gap = GetGameSeconds() - M_SecondTruckDestroyedAt;
	Log(FString::Printf(TEXT("RTS_FUELDEPOT truck3 after %.1f s (respawn %.0f)"), Gap, RespawnSeconds));
	if (Gap < RespawnSeconds - 1.f)
	{
		Fail(TEXT("respawn_too_early"));
	}
	SetStep(EFuelDepotTestStep::ThirdTruckSpawned);
}

void UFuelDepotTestSubsystem::TickThirdTruckSpawned()
{
	if (GetStepSeconds() < FuelDepotTestPrivate::ThirdTruckLifeSeconds)
	{
		return;
	}
	AHPActorObjectsMaster* const Building = Cast<AHPActorObjectsMaster>(M_Depot.Get());
	if (not IsValid(Building))
	{
		Fail(TEXT("depot_not_killable"));
		Finish();
		return;
	}
	Log(FString::Printf(TEXT("RTS_FUELDEPOT destroying the depot building (truck3 state=%d)"),
	                    static_cast<int32>(GetDepotState())));
	Building->TriggerDestroyActor(ERTSDeathType::Kinetic);
	SetStep(EFuelDepotTestStep::DepotDestroyed);
}

void UFuelDepotTestSubsystem::TickDepotDestroyed()
{
	if (M_DespawnedTrucks >= 2)
	{
		SetStep(EFuelDepotTestStep::NoMoreTrucks);
		return;
	}
	if (not M_DepotComponent.IsValid() && GetStepSeconds() > FuelDepotTestPrivate::DepotGoneGraceSeconds)
	{
		// The building actor is gone; the truck keeps its last order (documented). Not a failure by itself.
		Log(FString::Printf(TEXT("RTS_FUELDEPOT depot actor removed before truck3 got home (truck alive=%d)"),
		                    M_CurrentTruck.IsValid() ? 1 : 0));
		SetStep(EFuelDepotTestStep::NoMoreTrucks);
	}
}

void UFuelDepotTestSubsystem::TickNoMoreTrucks()
{
	if (M_SpawnedTrucks > 3)
	{
		Fail(TEXT("truck_after_depot_destroyed"));
		Finish();
		return;
	}
	if (GetStepSeconds() < FuelDepotTestPrivate::NoTruckWatchSeconds)
	{
		return;
	}
	AMissionFuelCrane* const Crane = M_Crane.Get();
	const bool bCraneAlive = IsValid(Crane) && Crane->IsUnitAlive();
	Log(FString::Printf(TEXT("RTS_FUELDEPOT end state=%d crane_alive=%d depot_actor=%d truck3_still_there=%d"),
	                    static_cast<int32>(GetDepotState()), bCraneAlive ? 1 : 0, M_Depot.IsValid() ? 1 : 0,
	                    M_CurrentTruck.IsValid() ? 1 : 0));
	if (not bCraneAlive)
	{
		Fail(TEXT("crane_died_with_bunker"));
	}
	if (M_CurrentTruck.IsValid())
	{
		// With or without the depot actor, the last truck must have driven home and been removed by now.
		Fail(TEXT("truck3_not_removed_after_depot_died"));
	}
	Finish();
}

void UFuelDepotTestSubsystem::Fail(const FString& Reason)
{
	M_Failures.Add(Reason);
	Log(TEXT("RTS_FUELDEPOT FAIL ") + Reason);
}

void UFuelDepotTestSubsystem::Log(const FString& Line)
{
	M_Lines.Add(Line);
	UE_LOG(LogRTSFuelDepotTest, Display, TEXT("%s"), *Line);
}

void UFuelDepotTestSubsystem::Finish()
{
	M_Step = EFuelDepotTestStep::Done;
	const bool bPassed = M_Failures.IsEmpty();
	const FString Result = FString::Printf(TEXT("RTS_FUELDEPOT RESULT %s spawned=%d unloaded=%d despawned=%d "
		                                       "destroyed=%d reasons=%s dir=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
	                                       M_SpawnedTrucks, M_UnloadedTrucks, M_DespawnedTrucks, M_DestroyedTrucks,
	                                       bPassed ? TEXT("none") : *FString::Join(M_Failures, TEXT(";")),
	                                       *M_OutputDirectory);
	Log(Result);
	FFileHelper::SaveStringToFile(TEXT("# Fuel depot test\n\n```\n") + FString::Join(M_Lines, TEXT("\n"))
	                              + TEXT("\n```\n"), *FPaths::Combine(M_OutputDirectory, TEXT("summary.md")));
	FPlatformMisc::RequestExit(false, TEXT("FuelDepotTest"));
}

int32 UFuelDepotTestSubsystem::GetAIPoints() const
{
	const AEnemyController* const Controller = FRTS_Statics::GetEnemyControllerOfUnit(M_Depot.Get());
	const UEnemyStrategicAIComponent* const StrategicAI = IsValid(Controller)
		                                                      ? Controller->GetEnemyStrategicAIComponent()
		                                                      : nullptr;
	return IsValid(StrategicAI) ? StrategicAI->MapTest_GetTrainingPoints() : -1;
}

int32 UFuelDepotTestSubsystem::CountVisibleCargo(const AActor* Truck) const
{
	if (not IsValid(Truck))
	{
		return -1;
	}
	int32 Count = 0;
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Truck);
	for (const UStaticMeshComponent* const Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->GetName().StartsWith(TEXT("Cargo")) && Mesh->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

void UFuelDepotTestSubsystem::LogTruckPose(const TCHAR* Label) const
{
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	const AActor* const Truck = M_CurrentTruck.Get();
	const UFuelDeliveryNodeComponent* const Park = IsValid(Depot)
		                                               ? Depot->MapTest_GetNode(EFuelDeliveryNodeRole::TruckPark)
		                                               : nullptr;
	if (not IsValid(Truck) || not IsValid(Park))
	{
		return;
	}
	const float Distance = FVector::Dist2D(Truck->GetActorLocation(), Park->GetComponentLocation());
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Truck->GetActorRotation().Yaw,
	                                                                Park->GetComponentRotation().Yaw));
	const_cast<UFuelDepotTestSubsystem*>(this)->Log(FString::Printf(
		TEXT("RTS_FUELDEPOT truck %s %s at %s yaw=%.0f park_dist=%.0f yaw_err=%.0f"), Label, *Truck->GetName(),
		*Truck->GetActorLocation().ToCompactString(), Truck->GetActorRotation().Yaw, Distance, YawError));
	if (Distance > FuelDepotTestPrivate::ParkDistanceLimitCm || YawError > FuelDepotTestPrivate::ParkYawLimitDegrees)
	{
		const_cast<UFuelDepotTestSubsystem*>(this)->Fail(FString::Printf(TEXT("park_pose_%s"), Label));
	}
}

bool UFuelDepotTestSubsystem::EnsureCaptureActor()
{
	using namespace FuelDepotTestPrivate;
	if (IsValid(M_CaptureActor) && IsValid(M_CaptureTarget))
	{
		return true;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_CaptureActor = GetWorld()->SpawnActor<ASceneCapture2D>(FVector::ZeroVector, FRotator::ZeroRotator,
	                                                         SpawnParameters);
	if (not IsValid(M_CaptureActor))
	{
		return false;
	}
	M_CaptureTarget = NewObject<UTextureRenderTarget2D>(this);
	M_CaptureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	M_CaptureTarget->InitAutoFormat(CaptureWidth, CaptureHeight);
	M_CaptureTarget->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* const CaptureComponent = M_CaptureActor->GetCaptureComponent2D();
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = 50.f;
	CaptureComponent->TextureTarget = M_CaptureTarget;
	// The fog of war is a post-process material: it would hide the enemy depot.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComponent->PostProcessSettings.AutoExposureBias = 2.f;
	CaptureComponent->PostProcessBlendWeight = 1.f;
	return true;
}

void UFuelDepotTestSubsystem::Capture(const FString& Suffix)
{
	const UFuelDeliveryDepotComponent* const Depot = M_DepotComponent.Get();
	const UFuelDeliveryNodeComponent* const Park = IsValid(Depot)
		                                               ? Depot->MapTest_GetNode(EFuelDeliveryNodeRole::TruckPark)
		                                               : nullptr;
	const UFuelDeliveryNodeComponent* const CraneNode = IsValid(Depot)
		                                                    ? Depot->MapTest_GetNode(EFuelDeliveryNodeRole::Crane)
		                                                    : nullptr;
	if (not IsValid(Park) || not IsValid(CraneNode) || not EnsureCaptureActor())
	{
		return;
	}
	// The fog of war hides enemy actors the human player cannot see.
	for (AActor* const Actor : {M_Depot.Get(), M_CurrentTruck.Get(), static_cast<AActor*>(Depot->GetCrane())})
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
	const FVector Focus = (Park->GetComponentLocation() + CraneNode->GetComponentLocation()) * 0.5f
		+ FVector(0.f, 0.f, 300.f);
	// RTS-like: 50 degrees down, from the side of the crane away from the truck.
	const FVector Side = (CraneNode->GetComponentLocation() - Park->GetComponentLocation()).GetSafeNormal2D();
	const FVector CameraDirection = Side.RotateAngleAxis(-35.f, FVector::UpVector);
	const FRotator LookRotation(-50.f, (-CameraDirection).Rotation().Yaw, 0.f);
	M_CaptureActor->SetActorLocationAndRotation(Focus - LookRotation.Vector() * 4200.f, LookRotation);
	M_CaptureActor->GetCaptureComponent2D()->CaptureScene();
	FImage Image;
	if (not FImageUtils::GetRenderTargetImage(M_CaptureTarget, Image))
	{
		return;
	}
	const FString Path = FPaths::Combine(M_OutputDirectory, Suffix + TEXT(".png"));
	FImageUtils::SaveImageByExtension(*Path, Image);
	Log(TEXT("RTS_FUELDEPOT capture ") + Path);
}

#endif
