// Copyright (C) Bas Blokzijl - All rights reserved.

#include "NomadicAirDeliveryTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Missions/NomadicAirDelivery/NomadicAirDeliverySubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSNomadicAirTest, Log, All);

namespace NomadicAirDeliveryTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr int32 CargoCasesPerBatch = 3;
	constexpr float SiteSpacing = 4500.f;
	constexpr float CargoLandTimeoutSeconds = 150.f;
	constexpr float GliderLandTimeoutSeconds = 200.f;
	// The HQ drives out of the glider to its spot first; a rope-slung truck only turns to the build yaw.
	constexpr float ConversionStartTimeoutSeconds = 60.f;
	constexpr float CargoLandTolerance = 250.f;
	constexpr float BuiltTolerance = 400.f;
	constexpr float ClearBatchSeconds = 4.f;
	constexpr float BatchShotDelaySeconds = 3.f;
	constexpr float SlungShotDelaySeconds = 6.f;
	// The low shot is taken when the slung truck is this close above the ground: the ropes and the hovering plane.
	constexpr float LowShotHeightAboveGround = 500.f;
	constexpr float MissionModeTimeoutSeconds = 400.f;
	constexpr float MissionStartTimeoutSeconds = 120.f;
	constexpr float CameraPitch = -45.f;
	constexpr float CameraYaw = 35.f;
	constexpr float CameraDistanceSite = 9000.f;
	constexpr float CameraDistanceFollow = 5500.f;
	const TCHAR* TrainsStartMissionClassFragment = TEXT("Mission_Trains_Start");
}

bool UNomadicAirDeliveryTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("NomadicAirDeliveryValidate"))
		|| FParse::Param(FCommandLine::Get(), TEXT("NomadicAirDeliveryTrainsMission"));
}

TStatId UNomadicAirDeliveryTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNomadicAirDeliveryTestSubsystem, STATGROUP_Tickables);
}

void UNomadicAirDeliveryTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case ENomadicAirDeliveryTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case ENomadicAirDeliveryTestStage::RunningBatch:
		TickBatch();
		UpdateCamera();
		return;
	case ENomadicAirDeliveryTestStage::ClearingBatch:
		TickClearingBatch();
		return;
	case ENomadicAirDeliveryTestStage::WatchingMission:
		TickWatchingMission();
		UpdateCamera();
		return;
	case ENomadicAirDeliveryTestStage::Finished:
		return;
	}
}

void UNomadicAirDeliveryTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSNomadicAirTest, Error, TEXT("RTS_NOMADIC_AIR_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UNomadicAirDeliveryTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UNomadicAirDeliveryTestSubsystem::EnterStage(const ENomadicAirDeliveryTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
}

void UNomadicAirDeliveryTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(ENomadicAirDeliveryTestStage::Finished);
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UNomadicAirDeliveryTestSubsystem::TickWaitingForStart()
{
	using namespace NomadicAirDeliveryTestPrivate;
	if (not StartGameAndGetIsReady())
	{
		return;
	}
	if (M_BootSeconds < 0.f)
	{
		M_BootSeconds = GetNow();
		return;
	}
	if (GetNow() - M_BootSeconds < StartDelaySeconds)
	{
		return;
	}
	if (UWorld* World = GetWorld(); IsValid(World) && not IsValid(M_Camera))
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	bM_IsTrainsMissionMode = FParse::Param(FCommandLine::Get(), TEXT("NomadicAirDeliveryTrainsMission"));
	if (bM_IsTrainsMissionMode)
	{
		// The mission manager starts its missions a while after the map is up.
		const bool bTimedOut = GetNow() - M_BootSeconds > MissionStartTimeoutSeconds;
		if (not IsValid(FindStartedTrainsStartMission()) && not bTimedOut)
		{
			return;
		}
		if (not StartTrainsMission())
		{
			Finish();
		}
		return;
	}
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	BuildRosterCases();
	StartBatch();
}

bool UNomadicAirDeliveryTestSubsystem::StartGameAndGetIsReady() const
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return false;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	return true;
}

bool UNomadicAirDeliveryTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace NomadicAirDeliveryTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("NomadicAirDeliveryAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("NomadicAirDeliveryAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller = FRTS_Statics::GetRTSController(World);
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

void UNomadicAirDeliveryTestSubsystem::BuildRosterCases()
{
	using namespace NomadicAirDeliveryTestPrivate;
	// The HQ comes by glider on its own first: its landing run needs the room.
	AddCase(ENomadicAirDeliveryMethod::MammutGlider, ENomadicSubtype::Nomadic_GerHq, M_Anchor, 0);

	// Every other German nomadic by rope-slung cargo plane, a few at a time.
	const uint8 FirstAfterHq = static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq) + 1;
	const uint8 LastGerman = static_cast<uint8>(ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory);
	int32 CargoIndex = 0;
	for (uint8 SubtypeValue = FirstAfterHq; SubtypeValue <= LastGerman; ++SubtypeValue)
	{
		const int32 SlotInBatch = CargoIndex % CargoCasesPerBatch;
		const int32 Batch = 1 + CargoIndex / CargoCasesPerBatch;
		const float SlotOffset = (static_cast<float>(SlotInBatch) - 1.f) * SiteSpacing;
		const int32 CasesBefore = M_Cases.Num();
		AddCase(ENomadicAirDeliveryMethod::CargoPlaneRopes, static_cast<ENomadicSubtype>(SubtypeValue),
		        M_Anchor + FVector(SlotOffset, 0.f, 0.f), Batch);
		CargoIndex += M_Cases.Num() - CasesBefore;
	}
	// The HQ by cargo plane too, alone in the last batch: its build radius would cover the other sites.
	const int32 LastBatch = 1 + FMath::DivideAndRoundUp(CargoIndex, CargoCasesPerBatch);
	AddCase(ENomadicAirDeliveryMethod::CargoPlaneRopes, ENomadicSubtype::Nomadic_GerHq, M_Anchor, LastBatch);
	UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST SKIP Nomadic_RusHq: Soviet HQ, not a player nomadic"));
	M_BatchCount = LastBatch + 1;
}

void UNomadicAirDeliveryTestSubsystem::AddCase(const ENomadicAirDeliveryMethod Method, const ENomadicSubtype Subtype,
                                               const FVector& Site, const int32 Batch)
{
	// Some nomadic subtypes have no truck yet (no class in the player's spawner); the game cannot deliver those.
	const ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	const FTrainingOption Option(EAllUnitType::UNType_Nomadic, static_cast<uint8>(Subtype));
	if (IsValid(AsyncSpawner) && not AsyncSpawner->GetHasClassForTrainingOption(Option))
	{
		UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST SKIP %s: no truck class in the spawner"),
		       *UEnum::GetValueAsString(Subtype));
		return;
	}
	FNomadicAirDeliveryTestCase Case;
	Case.Order.Method = Method;
	Case.Order.NomadicSubtype = Subtype;
	Case.Order.OwningPlayer = NomadicAirDeliveryTestPrivate::TestPlayer;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Site, Case.Order.BuildLocation);
	Case.Batch = Batch;
	M_Cases.Add(Case);
}

void UNomadicAirDeliveryTestSubsystem::StartBatch()
{
	EnterStage(ENomadicAirDeliveryTestStage::RunningBatch);
	bM_TookBatchShot = false;
	bM_TookSlungShot = false;
	UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST BATCH %d of %d"), M_CurrentBatch + 1,
	       M_BatchCount);
	for (int32 CaseIndex = 0; CaseIndex < M_Cases.Num(); ++CaseIndex)
	{
		if (M_Cases[CaseIndex].Batch == M_CurrentBatch)
		{
			StartCase(CaseIndex);
		}
	}
}

void UNomadicAirDeliveryTestSubsystem::StartCase(const int32 CaseIndex)
{
	UWorld* World = GetWorld();
	UNomadicAirDeliverySubsystem* Deliveries = IsValid(World)
		                                           ? World->GetSubsystem<UNomadicAirDeliverySubsystem>()
		                                           : nullptr;
	FNomadicAirDeliveryTestCase& Case = M_Cases[CaseIndex];
	Case.bStarted = true;
	Case.StartSeconds = GetNow();
	const FString Name = UEnum::GetValueAsString(Case.Order.NomadicSubtype);
	if (not IsValid(Deliveries))
	{
		Check(false, TEXT("delivery_subsystem_exists"), Name);
		Case.bDone = true;
		return;
	}
	TWeakObjectPtr<UNomadicAirDeliveryTestSubsystem> WeakThis(this);
	const int32 DeliveryId = Deliveries->StartDelivery(
		Case.Order, FOnNomadicAirDeliveryLanded::CreateLambda([WeakThis, CaseIndex](ANomadicVehicle* Nomadic)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnCaseLanded(CaseIndex, Nomadic);
			}
		}));
	Check(DeliveryId != INDEX_NONE, TEXT("delivery_started"), Name);
	UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST START %s by %s site=%s"), *Name,
	       *UEnum::GetValueAsString(Case.Order.Method), *Case.Order.BuildLocation.ToCompactString());
	if (DeliveryId == INDEX_NONE)
	{
		Case.bDone = true;
	}
}

void UNomadicAirDeliveryTestSubsystem::OnCaseLanded(const int32 CaseIndex, ANomadicVehicle* Nomadic)
{
	using namespace NomadicAirDeliveryTestPrivate;
	if (not M_Cases.IsValidIndex(CaseIndex))
	{
		return;
	}
	FNomadicAirDeliveryTestCase& Case = M_Cases[CaseIndex];
	const FString Name = UEnum::GetValueAsString(Case.Order.NomadicSubtype);
	Case.bLandedCallback = true;
	Case.LandedSeconds = GetNow();
	Check(IsValid(Nomadic), TEXT("delivered"), Name);
	if (not IsValid(Nomadic))
	{
		Case.bDone = true;
		return;
	}
	Case.Nomadic = Nomadic;
	Case.LandedLocation = Nomadic->GetActorLocation();
	const URTSComponent* RTSComponent = Nomadic->GetRTSComponent();
	Check(IsValid(RTSComponent) && RTSComponent->GetSubtypeAsNomadicSubtype() == Case.Order.NomadicSubtype,
	      TEXT("delivered_right_nomadic"), Name + TEXT(" ") + Nomadic->GetClass()->GetName());
	Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("owned_by_player"), Name);
	Check(Nomadic->GetAttachParentActor() == nullptr, TEXT("released_from_aircraft"), Name);
	// The observer of the mission mode hears of the truck after its build order, when it already left truck mode.
	if (not bM_IsTrainsMissionMode)
	{
		Check(Nomadic->GetIsInTruckMode(), TEXT("lands_as_truck"), Name);
	}
	const USelectionComponent* Selection = Nomadic->GetSelectionComponent();
	Check(IsValid(Selection) && Selection->GetCanBeSelected(), TEXT("selectable_after_landing"), Name);
	if (Case.Order.Method == ENomadicAirDeliveryMethod::CargoPlaneRopes)
	{
		const float FromSite = FVector::Dist2D(Case.LandedLocation, Case.Order.BuildLocation);
		const float AboveGround = Case.LandedLocation.Z - Case.Order.BuildLocation.Z;
		Check(FromSite <= CargoLandTolerance, TEXT("set_down_on_site"),
		      FString::Printf(TEXT("%s from_site=%.0f above_ground=%.0f"), *Name, FromSite, AboveGround));
	}
	RequestScreenshot(TEXT("landed_") + Name);
}

void UNomadicAirDeliveryTestSubsystem::TickBatch()
{
	using namespace NomadicAirDeliveryTestPrivate;
	if (not bM_TookBatchShot && GetNow() - M_StageSeconds >= BatchShotDelaySeconds)
	{
		bM_TookBatchShot = true;
		RequestScreenshot(FString::Printf(TEXT("batch%d_beacons"), M_CurrentBatch));
	}
	if (not bM_TookSlungShot && GetNow() - M_StageSeconds >= SlungShotDelaySeconds && IsValid(FindSlungNomadic()))
	{
		bM_TookSlungShot = true;
		RequestScreenshot(FString::Printf(TEXT("batch%d_slung_in_flight"), M_CurrentBatch));
	}
	for (FNomadicAirDeliveryTestCase& Case : M_Cases)
	{
		if (Case.Batch == M_CurrentBatch && Case.bStarted && not Case.bDone)
		{
			TickCase(Case);
		}
	}
	if (not GetIsBatchDone())
	{
		return;
	}
	RequestScreenshot(FString::Printf(TEXT("batch%d_built"), M_CurrentBatch));
	EnterStage(ENomadicAirDeliveryTestStage::ClearingBatch);
}

void UNomadicAirDeliveryTestSubsystem::TickCase(FNomadicAirDeliveryTestCase& Case)
{
	using namespace NomadicAirDeliveryTestPrivate;
	const FString Name = UEnum::GetValueAsString(Case.Order.NomadicSubtype);
	if (not Case.bLandedCallback)
	{
		TickCaseScreenshots(Case);
		const float LandTimeout = Case.Order.Method == ENomadicAirDeliveryMethod::MammutGlider
			                          ? GliderLandTimeoutSeconds
			                          : CargoLandTimeoutSeconds;
		if (GetNow() - Case.StartSeconds > LandTimeout)
		{
			Check(false, TEXT("landed_in_time"), Name);
			Case.bDone = true;
		}
		return;
	}
	ANomadicVehicle* Nomadic = Case.Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		Check(false, TEXT("survived_until_built"), Name);
		Case.bDone = true;
		return;
	}
	// Headless runs never finish the truck montage; once the conversion started it is fast-forwarded.
	if (Nomadic->GetNomadicStatus() != ENomadStatus::Truck && Nomadic->ShippingTest_ForceFinishConversionToBuilding())
	{
		CheckBuiltCase(Case);
		Case.bDone = true;
		return;
	}
	if (GetNow() - Case.LandedSeconds > ConversionStartTimeoutSeconds)
	{
		Check(false, TEXT("started_building_in_time"), FString::Printf(TEXT("%s status=%s from_site=%.0f"), *Name,
			      *UEnum::GetValueAsString(Nomadic->GetNomadicStatus()),
			      FVector::Dist2D(Nomadic->GetActorLocation(), Case.Order.BuildLocation)));
		Case.bDone = true;
	}
}

void UNomadicAirDeliveryTestSubsystem::TickCaseScreenshots(FNomadicAirDeliveryTestCase& Case)
{
	using namespace NomadicAirDeliveryTestPrivate;
	const FString Name = UEnum::GetValueAsString(Case.Order.NomadicSubtype);
	if (Case.Order.Method == ENomadicAirDeliveryMethod::MammutGlider)
	{
		if (not Case.bTookSlungShot && GetNow() - Case.StartSeconds > SlungShotDelaySeconds * 2.f)
		{
			Case.bTookSlungShot = true;
			RequestScreenshot(TEXT("glider_approach_") + Name);
		}
		return;
	}
	// Shot while the truck hangs low over its own spot: the ropes and the hovering plane.
	const ANomadicVehicle* Slung = FindSlungNomadicNear(Case.Order.BuildLocation, CargoLandTolerance * 4.f);
	if (not IsValid(Slung))
	{
		return;
	}
	const float AboveGround = Slung->GetActorLocation().Z - Case.Order.BuildLocation.Z;
	if (not Case.bTookLowShot && AboveGround < LowShotHeightAboveGround)
	{
		Case.bTookLowShot = true;
		RequestScreenshot(TEXT("lowering_") + Name);
	}
}

void UNomadicAirDeliveryTestSubsystem::CheckBuiltCase(const FNomadicAirDeliveryTestCase& Case)
{
	using namespace NomadicAirDeliveryTestPrivate;
	const ANomadicVehicle* Nomadic = Case.Nomadic.Get();
	const FString Name = UEnum::GetValueAsString(Case.Order.NomadicSubtype);
	if (not IsValid(Nomadic))
	{
		return;
	}
	const float FromSite = FVector::Dist2D(Nomadic->GetActorLocation(), Case.Order.BuildLocation);
	Check(Nomadic->GetNomadicStatus() == ENomadStatus::Building, TEXT("became_building"), Name);
	Check(FromSite <= BuiltTolerance, TEXT("built_on_site"), FString::Printf(TEXT("%s from_site=%.0f"), *Name, FromSite));
	// The air delivery is the one way to build outside an HQ build radius; the test sites have no HQ around.
	const bool bInsideRadius = GetIsInsideAnyHQBuildRadius(*Nomadic);
	UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST BUILT %s inside_hq_radius=%d took=%.1fs"), *Name,
	       bInsideRadius ? 1 : 0, GetNow() - Case.StartSeconds);
	if (Case.Order.NomadicSubtype != ENomadicSubtype::Nomadic_GerHq && not bM_IsTrainsMissionMode)
	{
		Check(not bInsideRadius, TEXT("built_outside_hq_radius"), Name);
	}
}

bool UNomadicAirDeliveryTestSubsystem::GetIsBatchDone() const
{
	for (const FNomadicAirDeliveryTestCase& Case : M_Cases)
	{
		if (Case.Batch == M_CurrentBatch && not Case.bDone)
		{
			return false;
		}
	}
	return true;
}

void UNomadicAirDeliveryTestSubsystem::TickClearingBatch()
{
	using namespace NomadicAirDeliveryTestPrivate;
	if (GetNow() - M_StageSeconds < ClearBatchSeconds)
	{
		return;
	}
	// The next batch reuses the sites.
	for (FNomadicAirDeliveryTestCase& Case : M_Cases)
	{
		if (Case.Batch == M_CurrentBatch && Case.Nomadic.IsValid())
		{
			Case.Nomadic->Destroy();
		}
	}
	++M_CurrentBatch;
	if (M_CurrentBatch >= M_BatchCount)
	{
		Finish();
		return;
	}
	StartBatch();
}

UMissionBase* UNomadicAirDeliveryTestSubsystem::FindStartedTrainsStartMission() const
{
	using namespace NomadicAirDeliveryTestPrivate;
	// The manager empties its Missions list once it activated them; the running mission is found as an object.
	const UWorld* World = GetWorld();
	for (TObjectIterator<UMissionBase> It; It; ++It)
	{
		UMissionBase* Mission = *It;
		if (IsValid(Mission) && Mission->GetWorld() == World && not Mission->IsTemplate()
			&& Mission->GetClass()->GetName().Contains(TrainsStartMissionClassFragment)
			&& Mission->GetHasMissionStarted())
		{
			return Mission;
		}
	}
	return nullptr;
}

bool UNomadicAirDeliveryTestSubsystem::StartTrainsMission()
{
	using namespace NomadicAirDeliveryTestPrivate;
	const AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(this);
	Check(IsValid(MissionManager), TEXT("mission_manager_exists"));
	if (not IsValid(MissionManager))
	{
		return false;
	}
	UMissionBase* StartMission = FindStartedTrainsStartMission();
	Check(IsValid(StartMission) && StartMission->GetHasMissionStarted(), TEXT("trains_start_mission_running"));
	if (not IsValid(StartMission) || not StartMission->GetHasMissionStarted())
	{
		return false;
	}
	UWorld* World = GetWorld();
	UNomadicAirDeliverySubsystem* Deliveries = World->GetSubsystem<UNomadicAirDeliverySubsystem>();
	if (not IsValid(Deliveries))
	{
		Check(false, TEXT("delivery_subsystem_exists"));
		return false;
	}
	M_TrainsStartMission = StartMission;
	Deliveries->OnAnyDeliveryFinished.AddUObject(this, &UNomadicAirDeliveryTestSubsystem::OnTrainsMissionDeliveryFinished);
	EnterStage(ENomadicAirDeliveryTestStage::WatchingMission);

	// Completing the mission runs its BP_OnMissionComplete: CreatePlayerNomadics orders the four deliveries.
	StartMission->OnMissionComplete();
	const int32 OrderedDeliveries = Deliveries->GetActiveDeliveryCount();
	Check(OrderedDeliveries == 4, TEXT("mission_ordered_four_deliveries"), FString::FromInt(OrderedDeliveries));
	const UMissionBase* NextMission = StartMission->NextMission;
	bM_NextMissionStartedBeforeHQ = IsValid(NextMission) && NextMission->GetHasMissionStarted();
	Check(not bM_NextMissionStartedBeforeHQ, TEXT("next_mission_waits_for_hq"));
	RequestScreenshot(TEXT("trains_beacons"));
	return true;
}

void UNomadicAirDeliveryTestSubsystem::OnTrainsMissionDeliveryFinished(ANomadicVehicle* Nomadic,
                                                                       const FNomadicAirDeliveryOrder& Order)
{
	FNomadicAirDeliveryTestCase Case;
	Case.Order = Order;
	Case.bStarted = true;
	Case.StartSeconds = M_StageSeconds;
	M_Cases.Add(Case);
	const int32 CaseIndex = M_Cases.Num() - 1;
	if (Order.NomadicSubtype == ENomadicSubtype::Nomadic_GerHq && M_TrainsStartMission.IsValid())
	{
		const UMissionBase* NextMission = M_TrainsStartMission->NextMission;
		Check(IsValid(NextMission) && NextMission->GetHasMissionStarted(), TEXT("next_mission_started_on_hq"));
	}
	OnCaseLanded(CaseIndex, Nomadic);
}

void UNomadicAirDeliveryTestSubsystem::TickWatchingMission()
{
	using namespace NomadicAirDeliveryTestPrivate;
	for (FNomadicAirDeliveryTestCase& Case : M_Cases)
	{
		if (not Case.bDone)
		{
			TickCase(Case);
		}
	}
	const UNomadicAirDeliverySubsystem* Deliveries = GetWorld()->GetSubsystem<UNomadicAirDeliverySubsystem>();
	const bool bAllLanded = IsValid(Deliveries) && Deliveries->GetActiveDeliveryCount() == 0;
	const bool bAllBuilt = not M_Cases.ContainsByPredicate([](const FNomadicAirDeliveryTestCase& Case)
	{
		return not Case.bDone;
	});
	if ((bAllLanded && bAllBuilt) || GetNow() - M_StageSeconds > MissionModeTimeoutSeconds)
	{
		FinishTrainsMission();
	}
}

void UNomadicAirDeliveryTestSubsystem::FinishTrainsMission()
{
	RequestScreenshot(TEXT("trains_base_built"));
	Check(M_Cases.Num() == 4, TEXT("all_four_delivered"), FString::FromInt(M_Cases.Num()));
	// The mission clears the "place your HQ here" hint once the HQ is on its spot.
	int32 RemainingHints = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("Hint_ExpandHQ")))
		{
			++RemainingHints;
		}
	}
	Check(RemainingHints == 0, TEXT("expand_hq_hint_removed"), FString::FromInt(RemainingHints));
	for (int32 First = 0; First < M_Cases.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < M_Cases.Num(); ++Second)
		{
			const float Between = FVector::Dist2D(M_Cases[First].Order.BuildLocation,
			                                      M_Cases[Second].Order.BuildLocation);
			UE_LOG(LogRTSNomadicAirTest, Display, TEXT("RTS_NOMADIC_AIR_TEST SPACING %s-%s %.0f"),
			       *UEnum::GetValueAsString(M_Cases[First].Order.NomadicSubtype),
			       *UEnum::GetValueAsString(M_Cases[Second].Order.NomadicSubtype), Between);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UNomadicAirDeliverySubsystem* Deliveries = World->GetSubsystem<UNomadicAirDeliverySubsystem>())
		{
			Deliveries->OnAnyDeliveryFinished.RemoveAll(this);
		}
	}
	Finish();
}

const AGliderTankDelivery* UNomadicAirDeliveryTestSubsystem::FindActiveGlider() const
{
	for (TActorIterator<AGliderTankDelivery> It(GetWorld()); It; ++It)
	{
		// Followed until it is down; the empty glider lingering on the field is no longer of interest.
		if (static_cast<uint8>(It->GetDeliveryPhase()) < static_cast<uint8>(EGliderDeliveryPhase::Lingering))
		{
			return *It;
		}
	}
	return nullptr;
}

const ANomadicVehicle* UNomadicAirDeliveryTestSubsystem::FindSlungNomadicNear(const FVector& Location,
                                                                             const float Radius) const
{
	for (TActorIterator<ANomadicVehicle> It(GetWorld()); It; ++It)
	{
		if (Cast<AAircraftMaster>(It->GetAttachParentActor())
			&& FVector::Dist2D(It->GetActorLocation(), Location) <= Radius)
		{
			return *It;
		}
	}
	return nullptr;
}

ANomadicVehicle* UNomadicAirDeliveryTestSubsystem::FindSlungNomadic() const
{
	for (TActorIterator<ANomadicVehicle> It(GetWorld()); It; ++It)
	{
		if (Cast<AAircraftMaster>(It->GetAttachParentActor()))
		{
			return *It;
		}
	}
	return nullptr;
}

bool UNomadicAirDeliveryTestSubsystem::GetIsInsideAnyHQBuildRadius(const ANomadicVehicle& Nomadic) const
{
	for (TActorIterator<ANomadicVehicle> It(GetWorld()); It; ++It)
	{
		const ANomadicVehicle* Other = *It;
		const URTSComponent* RTSComponent = Other->GetRTSComponent();
		if (Other == &Nomadic || not IsValid(RTSComponent)
			|| RTSComponent->GetSubtypeAsNomadicSubtype() != ENomadicSubtype::Nomadic_GerHq
			|| Other->GetIsInTruckMode())
		{
			continue;
		}
		if (FVector::Dist2D(Other->GetActorLocation(), Nomadic.GetActorLocation()) <= Other->GetBuildingRadius())
		{
			return true;
		}
	}
	return false;
}

void UNomadicAirDeliveryTestSubsystem::UpdateCamera()
{
	using namespace NomadicAirDeliveryTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	FVector Focus = M_Anchor;
	float Distance = CameraDistanceSite;
	if (const ANomadicVehicle* Slung = FindSlungNomadic())
	{
		Focus = Slung->GetActorLocation();
		Distance = CameraDistanceFollow;
	}
	else if (const AGliderTankDelivery* Glider = FindActiveGlider())
	{
		Focus = Glider->GetActorLocation();
		Distance = CameraDistanceFollow * 1.5f;
	}
	else if (M_Cases.Num() > 0)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FNomadicAirDeliveryTestCase& Case : M_Cases)
		{
			if (Case.Batch == M_CurrentBatch || bM_IsTrainsMissionMode)
			{
				Sum += Case.Order.BuildLocation;
				++Count;
			}
		}
		Focus = Count > 0 ? Sum / static_cast<float>(Count) : Focus;
	}
	const FRotator ViewRotation(CameraPitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UNomadicAirDeliveryTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("NomadicAirDeliveryTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label.Replace(TEXT("ENomadicSubtype::"), TEXT("")));
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UNomadicAirDeliveryTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UNomadicAirDeliveryTestSubsystem::Tick(float DeltaTime) {}
TStatId UNomadicAirDeliveryTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNomadicAirDeliveryTestSubsystem, STATGROUP_Tickables);
}

#endif
