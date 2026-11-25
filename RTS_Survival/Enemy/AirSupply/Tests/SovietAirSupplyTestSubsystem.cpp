// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietAirSupplyTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "RTS_Survival/Enemy/AirSupply/SovietAirSupplyPad.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/VTOLDropship/VTOLDropshipAircraft.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAirSupplyTest, Log, All);

namespace SovietAirSupplyTestPrivate
{
	const TCHAR* const Switch = TEXT("SovietAirSupplyValidate");
	const TCHAR* const PadClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/BP_SovietAirSupplyPad.BP_SovietAirSupplyPad_C");
	constexpr double StartDelaySeconds = 8.0;
	constexpr double GiveUpSeconds = 600.0;
	constexpr float GroundTraceHalfHeight = 100000.f;
	constexpr int32 EnemyPlayer = 2;
	constexpr int32 TrainingPoints = 4;
	constexpr int32 BuildingPoints = 5;
	constexpr int32 FuelItems = 2;
	constexpr int32 MunitionItems = 2;
	constexpr float FirstPlaneDelay = 2.f;
	constexpr float BetweenPlanes = 4.f;
	// Shorter flights than a map would use: the test waits for three planes.
	constexpr float ApproachDistance = 9000.f;
	constexpr float DepartureDistance = 9000.f;
	constexpr double UnloadShotAfterSeconds = 4.0;
	constexpr double ShootDownOnApproachAfterSeconds = 3.0;
	const FVector CameraOffset(-2600.f, -2200.f, 1500.f);
	// Low on the pad's far corner, looking across the cargo pile (right of the plane's tail) towards the sheds.
	const FVector CloseCameraOffset(900.f, -250.f, 520.f);
	const FVector CloseCameraTarget(-250.f, 1000.f, 60.f);
	const TArray<double> CloseShotTimes = {3.0, 9.0, 16.0, 24.0};
	// Frames on the close camera before its screenshot, so the view has switched.
	constexpr int32 CloseShotSettleFrames = 2;
	constexpr float MaxLandingOffset = 400.f;
	constexpr float TakeOffShotHeight = 600.f;
	constexpr float MinLiftOffHeight = 800.f;
}

bool USovietAirSupplyTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), SovietAirSupplyTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId USovietAirSupplyTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovietAirSupplyTestSubsystem, STATGROUP_Tickables);
}

void USovietAirSupplyTestSubsystem::Tick(const float DeltaTime)
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
		M_TestStartSeconds = World->GetTimeSeconds() + SovietAirSupplyTestPrivate::StartDelaySeconds;
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
	if (GetSeconds() > SovietAirSupplyTestPrivate::GiveUpSeconds)
	{
		Check(false, TEXT("finished_in_time"), FString::Printf(TEXT("stuck at step %d"), static_cast<int32>(M_Step)));
		Evaluate();
		return;
	}
	UpdateCamera();
	TickSteps();
#endif
}

double USovietAirSupplyTestSubsystem::GetSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_TestStartSeconds;
}

void USovietAirSupplyTestSubsystem::AdvanceStep(const EStep NextStep)
{
	M_Step = NextStep;
	M_StepStartSeconds = GetSeconds();
	M_UnloadingSinceSeconds = -1.0;
	M_ApproachSinceSeconds = -1.0;
	UE_LOG(LogRTSAirSupplyTest, Display, TEXT("RTS_AIRSUPPLY_TEST step %d at %.1f s"), static_cast<int32>(NextStep),
	       M_StepStartSeconds);
}

#if RTS_WITH_MAP_TESTS

void USovietAirSupplyTestSubsystem::StartTest()
{
	using namespace SovietAirSupplyTestPrivate;
	UWorld* World = GetWorld();
	AEnemyController* const EnemyController = FRTS_Statics::GetEnemyController(this);
	UEnemyStrategicAIComponent* StrategicAI = IsValid(EnemyController)
		                                          ? EnemyController->GetEnemyStrategicAIComponent()
		                                          : nullptr;
	UClass* PadClass = LoadClass<ASovietAirSupplyPad>(nullptr, PadClassPath);
	Check(IsValid(StrategicAI) && IsValid(PadClass), TEXT("setup"),
	      FString::Printf(TEXT("strategic_ai=%d pad_class=%d"), IsValid(StrategicAI), IsValid(PadClass)));
	if (not IsValid(StrategicAI) || not IsValid(PadClass))
	{
		Evaluate();
		return;
	}
	M_StrategicAI = StrategicAI;

	const ACPPController* const Controller = FRTS_Statics::GetRTSController(World);
	const FVector CameraLocation = IsValid(Controller) && IsValid(Controller->PlayerCameraManager)
		                               ? Controller->PlayerCameraManager->GetCameraLocation()
		                               : FVector::ZeroVector;
	FCollisionObjectQueryParams ObjectQuery(ECC_WorldStatic);
	FHitResult Hit;
	const FVector Top(CameraLocation.X, CameraLocation.Y, GroundTraceHalfHeight);
	const FVector Bottom(CameraLocation.X, CameraLocation.Y, -GroundTraceHalfHeight);
	const FVector Ground = World->LineTraceSingleByObjectType(Hit, Top, Bottom, ObjectQuery)
		                       ? Hit.ImpactPoint
		                       : CameraLocation;

	M_Pad = World->SpawnActorDeferred<ASovietAirSupplyPad>(PadClass, FTransform(FRotator::ZeroRotator, Ground),
	                                                       nullptr, nullptr,
	                                                       ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	FSovietAirSupplySettings& Settings = M_Pad->MapTest_GetEditableSettings();
	Settings.bEnabled = true;
	Settings.EnemyPlayer = EnemyPlayer;
	Settings.FirstPlaneDelaySeconds = FirstPlaneDelay;
	Settings.SecondsBetweenPlanes = BetweenPlanes;
	Settings.TrainingPointsPerDelivery = TrainingPoints;
	Settings.BuildingPointsPerDelivery = BuildingPoints;
	Settings.FuelBarrelsPerPlane = FuelItems;
	Settings.MunitionCratesPerPlane = MunitionItems;
	Settings.ApproachDistance = ApproachDistance;
	Settings.DepartureDistance = DepartureDistance;
	Check(not Settings.AircraftClass.IsNull(), TEXT("bp_has_aircraft_class"), Settings.AircraftClass.ToString());
	M_Pad->FinishSpawning(FTransform(FRotator::ZeroRotator, Ground));

	M_TrainingBefore = StrategicAI->MapTest_GetTrainingPoints();
	M_BuildingBefore = StrategicAI->GetBuildingPoints();
	M_WorkerStart = M_Pad->MapTest_GetWorkerLocation(0);

	M_Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Ground + CameraOffset));
	UE_LOG(LogRTSAirSupplyTest, Display, TEXT("RTS_AIRSUPPLY_TEST pad at %s"), *Ground.ToCompactString());
	Screenshot(TEXT("pad_idle"));
	AdvanceStep(EStep::FirstDelivery);
}

void USovietAirSupplyTestSubsystem::TickSteps()
{
	if (not IsValid(M_Pad))
	{
		Check(false, TEXT("pad_alive"), TEXT("pad gone"));
		Evaluate();
		return;
	}
	switch (M_Step)
	{
	case EStep::FirstDelivery:
		TickFirstDelivery();
		break;
	case EStep::FirstPlaneLeaves:
		TickFirstPlaneLeaves();
		break;
	case EStep::ShootDownWhileUnloading:
		TickShootDownWhileUnloading();
		break;
	case EStep::ShootDownOnApproach:
		TickShootDownOnApproach();
		break;
	default:
		break;
	}
}

void USovietAirSupplyTestSubsystem::TickFirstDelivery()
{
	AAircraftMaster* Plane = M_Pad->GetCurrentPlane();
	if (IsValid(Plane) && not bM_SawLanded)
	{
		// The aircraft hides in the player's fog; the pictures need it.
		if (UFowComp* Fow = Plane->GetFowComponent())
		{
			Fow->StopFowParticipation();
		}
	}
	M_WorkerMaxTravel = FMath::Max(M_WorkerMaxTravel,
	                               static_cast<float>(FVector::Dist2D(M_Pad->MapTest_GetWorkerLocation(0),
	                                                                  M_WorkerStart)));
	if (M_Pad->GetSupplyState() == ESovietAirSupplyState::Unloading && IsValid(Plane))
	{
		if (not bM_SawLanded)
		{
			OnFirstPlaneLanded(*Plane);
		}
		bM_SawCarry |= M_Pad->MapTest_GetIsAnyWorkerCarrying();
		TickUnloadScreenshots();
	}
	if (M_Pad->GetCompletedDeliveries() >= 1)
	{
		CheckFirstDelivery();
		AdvanceStep(EStep::FirstPlaneLeaves);
	}
}

void USovietAirSupplyTestSubsystem::OnFirstPlaneLanded(AAircraftMaster& Plane)
{
	using namespace SovietAirSupplyTestPrivate;
	bM_SawLanded = true;
	M_UnloadingSinceSeconds = GetSeconds();
	M_LandedOffset2D = FVector::Dist2D(Plane.GetActorLocation(), M_Pad->MapTest_GetLandingLocation());
	const URTSComponent* RTSComponent = Plane.FindComponentByClass<URTSComponent>();
	const int32 Owner = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1;
	Check(Owner == EnemyPlayer && not Plane.IsA<AVTOLDropshipAircraft>(), TEXT("plane_is_enemy_aircraft"),
	      FString::Printf(TEXT("owner=%d class=%s"), Owner, *Plane.GetClass()->GetName()));
	Check(M_LandedOffset2D < MaxLandingOffset, TEXT("plane_landed_on_pad"),
	      FString::Printf(TEXT("offset=%.0f"), M_LandedOffset2D));
}

void USovietAirSupplyTestSubsystem::TickUnloadScreenshots()
{
	using namespace SovietAirSupplyTestPrivate;
	const double UnloadingSeconds = GetSeconds() - M_UnloadingSinceSeconds;
	if (not bM_TookUnloadShot && bM_SawCarry && UnloadingSeconds > UnloadShotAfterSeconds)
	{
		bM_TookUnloadShot = true;
		Screenshot(TEXT("unloading"));
	}
	// Close-ups of the pile, the workers and the storages while the cargo moves.
	if (not bM_UseCloseCamera && M_CloseShotCount < CloseShotTimes.Num()
		&& UnloadingSeconds > CloseShotTimes[M_CloseShotCount])
	{
		bM_UseCloseCamera = true;
		M_CloseShotDelayFrames = CloseShotSettleFrames;
	}
	if (not bM_UseCloseCamera || --M_CloseShotDelayFrames > 0)
	{
		return;
	}
	Screenshot(FString::Printf(TEXT("close_%d"), M_CloseShotCount));
	++M_CloseShotCount;
	bM_UseCloseCamera = false;
}

void USovietAirSupplyTestSubsystem::CheckFirstDelivery()
{
	using namespace SovietAirSupplyTestPrivate;
	Check(bM_SawLanded && bM_SawCarry, TEXT("workers_carried_cargo"),
	      FString::Printf(TEXT("landed=%d carry=%d worker_travel=%.0f"), bM_SawLanded, bM_SawCarry,
	                      M_WorkerMaxTravel));
	Check(M_Pad->MapTest_GetItemsOnPile() == 0 && not M_Pad->MapTest_GetIsAnyWorkerCarrying(),
	      TEXT("pad_clear_after_delivery"), FString::Printf(TEXT("pile=%d"), M_Pad->MapTest_GetItemsOnPile()));
	Check(M_Pad->GetTotalTrainingPointsGranted() == TrainingPoints
	      && M_Pad->GetTotalBuildingPointsGranted() == BuildingPoints, TEXT("pad_granted_points"),
	      FString::Printf(TEXT("training=%d building=%d"), M_Pad->GetTotalTrainingPointsGranted(),
	                      M_Pad->GetTotalBuildingPointsGranted()));
	Check(GetBuildingPoints() == M_BuildingBefore + BuildingPoints, TEXT("ai_building_pool_filled"),
	      FString::Printf(TEXT("before=%d now=%d"), M_BuildingBefore, GetBuildingPoints()));
	const int32 TrainingNow = M_StrategicAI.IsValid() ? M_StrategicAI->MapTest_GetTrainingPoints() : -1;
	// The live AI earns and spends training points on its own: only report the pool.
	M_Lines.Add(FString::Printf(TEXT("INFO training_pool before=%d now=%d (AI earns/spends itself)"),
	                            M_TrainingBefore, TrainingNow));
	Check(M_Pad->GetSupplyState() == ESovietAirSupplyState::PlaneDeparting, TEXT("plane_departs_after_delivery"),
	      UEnum::GetValueAsString(M_Pad->GetSupplyState()));
}

void USovietAirSupplyTestSubsystem::TickFirstPlaneLeaves()
{
	using namespace SovietAirSupplyTestPrivate;
	const AAircraftMaster* Plane = M_Pad->GetCurrentPlane();
	const float PadZ = M_Pad->MapTest_GetLandingLocation().Z;
	if (IsValid(Plane))
	{
		M_MaxPlaneZAfterDelivery = FMath::Max(M_MaxPlaneZAfterDelivery,
		                                      static_cast<float>(Plane->GetActorLocation().Z));
	}
	if (IsValid(Plane) && not bM_TookTakeOffShot && Plane->GetActorLocation().Z > PadZ + TakeOffShotHeight)
	{
		bM_TookTakeOffShot = true;
		Screenshot(TEXT("vtol_takeoff"));
	}
	if (M_Pad->GetSupplyState() != ESovietAirSupplyState::WaitingForNextPlane || IsValid(Plane))
	{
		return;
	}
	const float LiftOff = M_MaxPlaneZAfterDelivery - PadZ;
	Check(LiftOff > MinLiftOffHeight, TEXT("plane_lifted_off_and_left"),
	      FString::Printf(TEXT("max_height=%.0f"), LiftOff));
	Check(GetBuildingPoints() == M_BuildingBefore + BuildingPoints, TEXT("points_unchanged_after_takeoff"),
	      FString::Printf(TEXT("building=%d"), GetBuildingPoints()));
	AdvanceStep(EStep::ShootDownWhileUnloading);
}

void USovietAirSupplyTestSubsystem::TickShootDownWhileUnloading()
{
	using namespace SovietAirSupplyTestPrivate;
	AAircraftMaster* Plane = M_Pad->GetCurrentPlane();
	if (M_Pad->GetSupplyState() != ESovietAirSupplyState::Unloading || not IsValid(Plane))
	{
		return;
	}
	if (M_UnloadingSinceSeconds < 0.0)
	{
		M_UnloadingSinceSeconds = GetSeconds();
	}
	// Shot while the workers carry: cargo on its way, none of it counts until the last item is stored.
	if (GetSeconds() - M_UnloadingSinceSeconds < UnloadShotAfterSeconds)
	{
		return;
	}
	const int32 Stored = M_Pad->MapTest_GetStoredItems();
	const int32 BuildingBefore = GetBuildingPoints();
	const int32 GrantedBefore = M_Pad->GetTotalTrainingPointsGranted();
	Plane->TakeFatalDamage();
	Check(M_Pad->GetLostDeliveries() == 1, TEXT("unloading_shot_down_counts_lost"),
	      FString::Printf(TEXT("lost=%d stored_before=%d of %d"), M_Pad->GetLostDeliveries(), Stored,
	                      FuelItems + MunitionItems));
	Check(M_Pad->GetTotalTrainingPointsGranted() == GrantedBefore && GetBuildingPoints() == BuildingBefore,
	      TEXT("unloading_shot_down_no_points"),
	      FString::Printf(TEXT("training_total=%d building=%d"), M_Pad->GetTotalTrainingPointsGranted(),
	                      GetBuildingPoints()));
	Check(M_Pad->MapTest_GetItemsOnPile() == 0 && not M_Pad->MapTest_GetIsAnyWorkerCarrying(),
	      TEXT("unloading_shot_down_pad_cleared"),
	      FString::Printf(TEXT("pile=%d carrying=%d"), M_Pad->MapTest_GetItemsOnPile(),
	                      M_Pad->MapTest_GetIsAnyWorkerCarrying()));
	Check(M_Pad->GetSupplyState() == ESovietAirSupplyState::WaitingForNextPlane,
	      TEXT("unloading_shot_down_next_scheduled"), UEnum::GetValueAsString(M_Pad->GetSupplyState()));
	AdvanceStep(EStep::ShootDownOnApproach);
}

void USovietAirSupplyTestSubsystem::TickShootDownOnApproach()
{
	using namespace SovietAirSupplyTestPrivate;
	AAircraftMaster* Plane = M_Pad->GetCurrentPlane();
	if (M_Pad->GetSupplyState() != ESovietAirSupplyState::PlaneApproaching || not IsValid(Plane))
	{
		return;
	}
	if (M_ApproachSinceSeconds < 0.0)
	{
		M_ApproachSinceSeconds = GetSeconds();
	}
	if (GetSeconds() - M_ApproachSinceSeconds < ShootDownOnApproachAfterSeconds)
	{
		return;
	}
	const int32 BuildingBefore = GetBuildingPoints();
	Plane->TakeFatalDamage();
	Check(M_Pad->GetLostDeliveries() == 2 && GetBuildingPoints() == BuildingBefore
	      && M_Pad->GetCompletedDeliveries() == 1,
	      TEXT("approach_shot_down_no_points"),
	      FString::Printf(TEXT("lost=%d completed=%d building=%d"), M_Pad->GetLostDeliveries(),
	                      M_Pad->GetCompletedDeliveries(), GetBuildingPoints()));
	Evaluate();
}

void USovietAirSupplyTestSubsystem::UpdateCamera()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (not IsValid(PlayerController) || not IsValid(M_Camera) || not IsValid(M_Pad))
	{
		return;
	}
	using namespace SovietAirSupplyTestPrivate;
	const FVector PadLocation = M_Pad->GetActorLocation();
	const FVector Target = PadLocation + (bM_UseCloseCamera ? CloseCameraTarget : FVector(0.f, 600.f, 200.f));
	M_Camera->SetActorLocation(PadLocation + (bM_UseCloseCamera ? CloseCameraOffset : CameraOffset));
	M_Camera->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(M_Camera->GetActorLocation(), Target));
	// The game's camera takes the view back; it is handed to the test camera every frame.
	PlayerController->SetViewTarget(M_Camera);
}

void USovietAirSupplyTestSubsystem::Screenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("SovietAirSupplyTest") / FString::Printf(
		TEXT("%02d_%s.png"), M_ShotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSAirSupplyTest, Display, TEXT("RTS_AIRSUPPLY_TEST screenshot %s"), *FileName);
}

int32 USovietAirSupplyTestSubsystem::GetBuildingPoints() const
{
	return M_StrategicAI.IsValid() ? M_StrategicAI->GetBuildingPoints() : -1;
}

#else

void USovietAirSupplyTestSubsystem::StartTest() {}
void USovietAirSupplyTestSubsystem::TickSteps() {}
void USovietAirSupplyTestSubsystem::TickFirstDelivery() {}
void USovietAirSupplyTestSubsystem::OnFirstPlaneLanded(AAircraftMaster&) {}
void USovietAirSupplyTestSubsystem::TickUnloadScreenshots() {}
void USovietAirSupplyTestSubsystem::CheckFirstDelivery() {}
void USovietAirSupplyTestSubsystem::TickFirstPlaneLeaves() {}
void USovietAirSupplyTestSubsystem::TickShootDownWhileUnloading() {}
void USovietAirSupplyTestSubsystem::TickShootDownOnApproach() {}
void USovietAirSupplyTestSubsystem::UpdateCamera() {}
void USovietAirSupplyTestSubsystem::Screenshot(const FString&) {}
int32 USovietAirSupplyTestSubsystem::GetBuildingPoints() const { return 0; }

#endif

void USovietAirSupplyTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSAirSupplyTest, Display, TEXT("RTS_AIRSUPPLY_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}

void USovietAirSupplyTestSubsystem::Evaluate()
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSAirSupplyTest, Display, TEXT("RTS_AIRSUPPLY_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("SovietAirSupplyTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	M_Step = EStep::Finished;
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}
