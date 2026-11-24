// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnloadStationTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/Harvester/HarvesterStatus/HarvesterStatus.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Resources/UnloadStation/ResourceUnloadStation.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnloadStationTest, Log, All);

namespace UnloadStationTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("UnloadStationValidate");
	const TCHAR* const AnchorSwitch = TEXT("UnloadStationAt=");
	const TCHAR* const CaptureSwitch = TEXT("UnloadStationCapture=");
	const TCHAR* const CaptureEverySwitch = TEXT("UnloadStationCaptureEvery=");
	// Unload lock check: the held move goes this far ahead of the docked harvester; while it waits the harvester may
	// not drift off the dock by more than the slack below.
	constexpr float LockMoveDistanceCm = 1800.f;
	constexpr float MaxLockDriftCm = 30.f;
	// Harvesters are ordered home at the start of the phase and dock about 10 s later; the old 18 s reverse timeout
	// docked them after 24 s.
	constexpr float MaxDockSeconds = 16.f;
	// Rig geometry: how far (2D) the grab may hang from the pick point when it grabs and from the bin socket when it
	// releases. The HQ's bay L drops from the loop's outer leg (Y -335), about 1.05 m from the radixite socket but well
	// inside the 2.9 m wide bin (Y -426..-135, build_monorail.py).
	constexpr float MaxGrabOffPick2DCm = 80.f;
	constexpr float MaxReleaseOffBin2DCm = 120.f;
	const TCHAR* const TruckSwitch = TEXT("UnloadStationTruck=");
	const TCHAR* const RefineryPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Ger_Refinery_Truck.Ger_Refinery_Truck_C");
	const TCHAR* const MetalDepotPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/Opel_MetalVault/Ger_MetalVault_Truck.Ger_MetalVault_Truck_C");
	// The bay-less harvester must unload near the overflow point and keep clear of the docked one (hull centres).
	constexpr float MaxOverflowErrorCm = 550.f;
	constexpr float MinOverflowClearanceCm = 450.f;
	const TCHAR* const HQPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Ger_HQ_Truck.Ger_HQ_Truck_C");
	const TCHAR* const HarvesterPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Harvester/Ger_Harvester_PanzerIV.Ger_Harvester_PanzerIV_C");
	const FVector2D DefaultAnchor(16796.f, -15808.f);
	constexpr uint8 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float StationTimeoutSeconds = 40.f;
	constexpr float HarvesterSetupSeconds = 3.f;
	constexpr float UnloadTimeoutSeconds = 200.f;
	constexpr float PackUpTimeoutSeconds = 120.f;
	// Harvesters start this far ahead of the HQ (building +X) and to either side.
	constexpr float HarvesterAheadCm = 2600.f;
	constexpr float HarvesterSideCm = 700.f;
	constexpr int32 RadixiteCargo = 450;
	constexpr int32 MetalCargo = 225;
	constexpr int32 PackUpCargo = 300;
	constexpr float MaxDockErrorCm = 120.f;
	constexpr float MaxDockYawErrorDeg = 12.f;
	constexpr int32 FramesToWriteLastCapture = 3;
	// Seconds into a single-bay unload's busy phase for extra frames (bars on the belt, bars landing in the bin).
	const TArray<float> MidUnloadCaptureSeconds = {6.f, 14.f, 24.f};
	const TArray<FB4HowitzerCameraView> Views = {
		{TEXT("bays"), FVector(2300.f, -1700.f, 1500.f), FVector(500.f, 0.f, 300.f), 50.f},
		{TEXT("bins"), FVector(600.f, -1500.f, 1100.f), FVector(-150.f, -260.f, 250.f), 50.f},
		{TEXT("top"), FVector(250.f, -60.f, 3000.f), FVector(250.f, -59.f, 0.f), 55.f}
	};
	const TArray<FB4HowitzerCameraView> RefineryViews = {
		{TEXT("crane"), FVector(1500.f, -1250.f, 1050.f), FVector(200.f, 0.f, 330.f), 50.f},
		{TEXT("side"), FVector(1250.f, 1550.f, 950.f), FVector(150.f, 150.f, 380.f), 50.f},
		{TEXT("top"), FVector(250.f, -100.f, 3000.f), FVector(250.f, -99.f, 0.f), 60.f}
	};
	// The test map's low sun lights the depot's -X side: the "sunlit" and "head" views look from there (apron and belt,
	// then the head pulley throwing bars into the bin).
	const TArray<FB4HowitzerCameraView> MetalDepotViews = {
		{TEXT("sunlit"), FVector(-1300.f, -750.f, 650.f), FVector(-22.f, -280.f, 250.f), 45.f},
		{TEXT("head"), FVector(-650.f, -260.f, 780.f), FVector(-70.f, -60.f, 300.f), 40.f},
		{TEXT("side"), FVector(1350.f, -300.f, 420.f), FVector(-20.f, -300.f, 250.f), 50.f},
		{TEXT("top"), FVector(-20.f, -450.f, 3000.f), FVector(-20.f, -449.f, 0.f), 60.f}
	};
}

bool UUnloadStationTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), UnloadStationTestPrivate::ValidateSwitch);
}

TStatId UUnloadStationTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnloadStationTestSubsystem, STATGROUP_Tickables);
}

void UUnloadStationTestSubsystem::Tick(const float DeltaTime)
{
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	const UWorld* World = GetWorld();
	if (M_Phase == EUnloadStationTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST started map=%s"), *World->GetName());
	}
	if (M_Phase == EUnloadStationTestPhase::BothBaysUnloading || M_Phase == EUnloadStationTestPhase::PackUpMidUnload)
	{
		TickCaptureSequence();
	}
	switch (M_Phase)
	{
	case EUnloadStationTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EUnloadStationTestPhase::WaitingForStation:
		TickWaitingForStation();
		return;
	case EUnloadStationTestPhase::WaitingForHarvesters:
		TickWaitingForHarvesters();
		return;
	case EUnloadStationTestPhase::BothBaysUnloading:
		TickBothBaysUnloading();
		return;
	case EUnloadStationTestPhase::PackUpMidUnload:
		TickPackUpMidUnload();
		return;
	case EUnloadStationTestPhase::WaitingForPackUpResult:
		TickWaitingForPackUpResult();
		return;
	default:
		return;
	}
}

void UUnloadStationTestSubsystem::TickWaitingForStart()
{
	using namespace UnloadStationTestPrivate;
	StartGameAndUnpause();
	if (GetPhaseSeconds() < StartDelaySeconds)
	{
		return;
	}
	FString Truck;
	(void)FParse::Value(FCommandLine::Get(), TruckSwitch, Truck);
	M_StationKind = Truck.Equals(TEXT("Refinery"), ESearchCase::IgnoreCase)
		                ? EUnloadTestStation::Refinery
		                : Truck.Equals(TEXT("MetalDepot"), ESearchCase::IgnoreCase)
		                ? EUnloadTestStation::MetalDepot
		                : EUnloadTestStation::HQ;
	M_TrackedResource = M_StationKind == EUnloadTestStation::MetalDepot
		                    ? ERTSResourceType::Resource_Metal
		                    : ERTSResourceType::Resource_Radixite;
	UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST truck=%s"),
	       M_StationKind == EUnloadTestStation::Refinery ? TEXT("Refinery")
	       : M_StationKind == EUnloadTestStation::MetalDepot ? TEXT("MetalDepot") : TEXT("HQ"));
	FString Capture;
	if (FParse::Value(FCommandLine::Get(), CaptureSwitch, Capture))
	{
		M_CaptureRig.Init(*GetWorld(), Capture);
	}
	(void)FParse::Value(FCommandLine::Get(), CaptureEverySwitch, M_CaptureEverySeconds);
	SpawnHQ();
	Check(M_HQ.IsValid(), TEXT("hq_spawned"));
	if (not M_HQ.IsValid())
	{
		Finish();
		return;
	}
	EnterPhase(EUnloadStationTestPhase::WaitingForStation);
}

void UUnloadStationTestSubsystem::SpawnHQ()
{
	using namespace UnloadStationTestPrivate;
	UWorld* World = GetWorld();
	FVector2D Anchor = DefaultAnchor;
	FString AnchorText;
	if (FParse::Value(FCommandLine::Get(), AnchorSwitch, AnchorText))
	{
		FString X, Y;
		if (AnchorText.Split(TEXT(","), &X, &Y))
		{
			Anchor = FVector2D(FCString::Atof(*X), FCString::Atof(*Y));
		}
	}
	const TCHAR* const TruckPath = M_StationKind == EUnloadTestStation::Refinery
		                               ? RefineryPath
		                               : M_StationKind == EUnloadTestStation::MetalDepot
		                               ? MetalDepotPath
		                               : HQPath;
	UClass* HQClass = LoadClass<ANomadicVehicle>(nullptr, TruckPath);
	FVector Ground;
	if (not IsValid(World) || not IsValid(HQClass)
		|| not GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(Anchor, 0.f), Ground))
	{
		return;
	}
	M_Anchor = Ground;
	// On the ground: it starts converted to a building, so it never drops onto the terrain like a driving truck.
	const FTransform SpawnTransform(FRotator::ZeroRotator, Ground);
	ANomadicVehicle* HQ = World->SpawnActorDeferred<ANomadicVehicle>(
		HQClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(HQ))
	{
		return;
	}
	// Protected Blueprint setting; set through reflection like a level-placed, already unpacked HQ.
	if (const FBoolProperty* StartAsBuilding = FindFProperty<FBoolProperty>(
		ANomadicVehicle::StaticClass(), TEXT("bStartAsConvertedToBuilding")))
	{
		StartAsBuilding->SetPropertyValue_InContainer(HQ, true);
	}
	HQ->FinishSpawning(SpawnTransform);
	M_HQ = HQ;
}

void UUnloadStationTestSubsystem::TickWaitingForStation()
{
	using namespace UnloadStationTestPrivate;
	StartGameAndUnpause();
	// Unattended runs never draw the truck, so its deploy montage never ends on its own.
	if (M_HQ.IsValid())
	{
		M_HQ->ShippingTest_ForceFinishConversionToBuilding();
	}
	AResourceUnloadStation* Station = FindStation();
	if (not IsValid(Station) || Station->GetBayCount() == 0)
	{
		if (GetPhaseSeconds() > StationTimeoutSeconds)
		{
			const int32 NomadStatus = M_HQ.IsValid() ? static_cast<int32>(M_HQ->GetNomadicStatus()) : -1;
			Check(false, TEXT("station_spawned"), FString::Printf(
				      TEXT("no AResourceUnloadStation on the unpacked HQ (station=%d nomad_status=%d)"),
				      IsValid(Station) ? 1 : 0, NomadStatus));
			Finish();
		}
		return;
	}
	M_Station = Station;
	Check(Station->GetBayCount() == (GetIsSingleBay() ? 1 : 2), TEXT("station_bay_count"),
	      FString::FromInt(Station->GetBayCount()));
	const UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
	Check(IsValid(DropOff) && DropOff->GetUnloadStation() == Station, TEXT("dropoff_knows_station"));
	EmptyStores();
	const FVector Forward = M_HQ->GetActorForwardVector();
	const FVector Right = M_HQ->GetActorRightVector();
	const FVector Base = M_HQ->GetActorLocation() + Forward * HarvesterAheadCm;
	for (const float Side : {-HarvesterSideCm, HarvesterSideCm})
	{
		FUnloadTestHarvester TestHarvester;
		TestHarvester.Actor = SpawnHarvester(Base + Right * Side);
		TestHarvester.Harvester = TestHarvester.Actor.IsValid()
			                          ? TestHarvester.Actor->FindComponentByClass<UHarvester>()
			                          : nullptr;
		M_Harvesters.Add(TestHarvester);
	}
	Check(M_Harvesters[0].Harvester.IsValid() && M_Harvesters[1].Harvester.IsValid(), TEXT("harvesters_spawned"));
	EnterPhase(EUnloadStationTestPhase::WaitingForHarvesters);
}

AResourceUnloadStation* UUnloadStationTestSubsystem::FindStation() const
{
	for (TActorIterator<AResourceUnloadStation> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->GetAttachParentActor() == M_HQ.Get())
		{
			return *It;
		}
	}
	return nullptr;
}

AActor* UUnloadStationTestSubsystem::SpawnHarvester(const FVector& Location) const
{
	using namespace UnloadStationTestPrivate;
	UWorld* World = GetWorld();
	UClass* HarvesterClass = LoadClass<AActor>(nullptr, HarvesterPath);
	FVector Ground;
	if (not IsValid(World) || not IsValid(HarvesterClass)
		|| not GlobalAbilityHelpers::TryFindGroundLocation(this, Location, Ground))
	{
		return nullptr;
	}
	// Facing the HQ, as a harvester coming back from the field would.
	const FTransform SpawnTransform(FRotator(0.f, M_HQ->GetActorRotation().Yaw + 180.f, 0.f),
	                                Ground + FVector(0.f, 0.f, 150.f));
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, HarvesterClass, SpawnTransform, TestPlayer);
}

void UUnloadStationTestSubsystem::TickWaitingForHarvesters()
{
	using namespace UnloadStationTestPrivate;
	if (GetPhaseSeconds() < HarvesterSetupSeconds)
	{
		return;
	}
	M_StoredRadixiteBefore = GetStored(ERTSResourceType::Resource_Radixite);
	M_StoredMetalBefore = GetStored(ERTSResourceType::Resource_Metal);
	M_LastStoredRadixite = GetStored(M_TrackedResource);
	if (GetIsSingleBay())
	{
		// Both carry the station's one resource; keep the pair within what the store can take.
		const UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
		const int32 Room = IsValid(DropOff) ? DropOff->GetDropOffCapacity(M_TrackedResource) : 0;
		const int32 FullCargo = M_TrackedResource == ERTSResourceType::Resource_Metal ? MetalCargo : RadixiteCargo;
		M_SingleBayCargoEach = FMath::Min(FullCargo, Room / 2);
		Check(M_SingleBayCargoEach >= 60, TEXT("store_has_room"),
		      FString::Printf(TEXT("room=%d each=%d"), Room, M_SingleBayCargoEach));
		OrderReturnCargo(M_Harvesters[0], M_TrackedResource, M_SingleBayCargoEach);
		OrderReturnCargo(M_Harvesters[1], M_TrackedResource, M_SingleBayCargoEach);
		EnterPhase(EUnloadStationTestPhase::BothBaysUnloading);
		return;
	}
	OrderReturnCargo(M_Harvesters[0], ERTSResourceType::Resource_Radixite, RadixiteCargo);
	OrderReturnCargo(M_Harvesters[1], ERTSResourceType::Resource_Metal, MetalCargo);
	EnterPhase(EUnloadStationTestPhase::BothBaysUnloading);
}

void UUnloadStationTestSubsystem::OrderReturnCargo(FUnloadTestHarvester& TestHarvester,
                                                   const ERTSResourceType Resource, const int32 Amount)
{
	UHarvester* Harvester = TestHarvester.Harvester.Get();
	ICommands* Commands = Cast<ICommands>(TestHarvester.Actor.Get());
	if (not IsValid(Harvester) || Commands == nullptr)
	{
		Check(false, TEXT("harvester_ready_for_order"));
		return;
	}
	Harvester->MapTest_SetCargo(Resource, Amount);
	TestHarvester.Resource = Resource;
	TestHarvester.StartCargo = GetCargo(TestHarvester);
	TestHarvester.bWasDocked = false;
	TestHarvester.bFinished = false;
	Check(TestHarvester.StartCargo == Amount, TEXT("harvester_cargo_set"),
	      FString::Printf(TEXT("%s=%d"), *Global_GetResourceTypeAsString(Resource), TestHarvester.StartCargo));
	const ECommandQueueError Error = Commands->ReturnCargo(true);
	Check(Error == ECommandQueueError::NoError, TEXT("return_cargo_ordered"));
}

void UUnloadStationTestSubsystem::TrackHarvester(FUnloadTestHarvester& TestHarvester, AResourceUnloadStation& Station)
{
	const UHarvester* Harvester = TestHarvester.Harvester.Get();
	if (not IsValid(Harvester) || TestHarvester.bFinished)
	{
		return;
	}
	const EHarvesterAIAction Status = Harvester->MapTest_GetHarvestStatus();
	if (static_cast<uint8>(Status) != TestHarvester.LastLoggedStatus && TestHarvester.Actor.IsValid())
	{
		// The route into the bay, step by step: shows where a slow dock waits (approach, reverse, guided slide).
		TestHarvester.LastLoggedStatus = static_cast<uint8>(Status);
		const FVector Location = TestHarvester.Actor->GetActorLocation();
		UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST status harvester=%s status=%s at=(%.0f,%.0f) t=%.1f"),
		       *TestHarvester.Actor->GetName(), *GetStringHarvesterAIStatus(Status), Location.X, Location.Y,
		       GetPhaseSeconds());
	}
	if (Status == EHarvesterAIAction::CraneUnloading && not TestHarvester.bWasDocked)
	{
		TestHarvester.bWasDocked = true;
		for (int32 BayIndex = 0; BayIndex < Station.GetBayCount(); ++BayIndex)
		{
			if (Station.GetBayHarvester(BayIndex) != Harvester)
			{
				continue;
			}
			// Against the station's own dock transform: the cargo socket over the pick point on the cranes, the rear
			// bumper on it at the depot.
			FTransform Dock;
			if (not Station.GetDockTransform(BayIndex, *Harvester, Dock))
			{
				continue;
			}
			TestHarvester.DockErrorCm = FVector::Dist2D(Dock.GetLocation(), TestHarvester.Actor->GetActorLocation());
			TestHarvester.DockYawErrorDeg = FMath::Abs(FMath::FindDeltaAngleDegrees(
				TestHarvester.Actor->GetActorRotation().Yaw, Dock.Rotator().Yaw));
			TestHarvester.DockedLocation = TestHarvester.Actor->GetActorLocation();
			TestHarvester.DockedPhaseSeconds = GetPhaseSeconds();
			UE_LOG(LogRTSUnloadStationTest, Display,
			       TEXT("RTS_UNLOAD_TEST docked harvester=%s bay=%d dock_error_cm=%.0f yaw_error=%.1f t=%.1f"),
			       *TestHarvester.Actor->GetName(), BayIndex, TestHarvester.DockErrorCm,
			       TestHarvester.DockYawErrorDeg, GetPhaseSeconds());
		}
		if (M_Phase == EUnloadStationTestPhase::BothBaysUnloading)
		{
			GiveLockOrder(TestHarvester);
		}
	}
	TrackLockedHarvester(TestHarvester, Status);
	if (Status == EHarvesterAIAction::CraneUnloading)
	{
		float Fill = 0.f;
		if (Harvester->MapTest_GetUnloadProgress(Fill))
		{
			TestHarvester.bSawProgressShown = true;
			TestHarvester.bProgressWentDown |= Fill + KINDA_SMALL_NUMBER < TestHarvester.LastProgressFill;
			TestHarvester.LastProgressFill = Fill;
			TestHarvester.MaxProgressFill = FMath::Max(TestHarvester.MaxProgressFill, Fill);
		}
	}
	// On a single-bay station the bay-less harvester empties at the overflow point without ever docking.
	if ((TestHarvester.bWasDocked || GetIsSingleBay()) && GetCargo(TestHarvester) == 0
		&& Status != EHarvesterAIAction::CraneUnloading)
	{
		TestHarvester.bFinished = true;
		TestHarvester.FinishedLocation = TestHarvester.Actor.IsValid()
			                                 ? TestHarvester.Actor->GetActorLocation()
			                                 : FVector::ZeroVector;
		UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST unloaded harvester=%s t=%.1f status=%s"),
		       *TestHarvester.Actor->GetName(), GetPhaseSeconds(), *GetStringHarvesterAIStatus(Status));
		OnLockedHarvesterFinished(TestHarvester);
		float FillAfter = 0.f;
		TestHarvester.bProgressHiddenAfterUnload = not Harvester->MapTest_GetUnloadProgress(FillAfter);
	}
}

void UUnloadStationTestSubsystem::TickBothBaysUnloading()
{
	using namespace UnloadStationTestPrivate;
	AResourceUnloadStation* Station = M_Station.Get();
	if (not IsValid(Station))
	{
		Check(false, TEXT("station_alive_while_unloading"));
		Finish();
		return;
	}
	for (FUnloadTestHarvester& TestHarvester : M_Harvesters)
	{
		TrackHarvester(TestHarvester, *Station);
	}
	if (GetIsSingleBay())
	{
		TickSingleBayUnloadResults(*Station);
		return;
	}
	TickRigGeometry(*Station);
	bM_SawBothDocked |= Station->GetIsBayDocked(0) && Station->GetIsBayDocked(1);
	const bool bBothBusy = Station->GetTrolleyPhase(0) != EUnloadTrolleyPhase::Idle
		&& Station->GetTrolleyPhase(1) != EUnloadTrolleyPhase::Idle;
	bM_SawBothTrolleysBusy |= bBothBusy;
	if (bBothBusy && not bM_CapturedBusy && Station->GetTrolleyPhase(0) == EUnloadTrolleyPhase::MovingToBin)
	{
		bM_CapturedBusy = true;
		CaptureViews(TEXT("both_working"));
	}
	const int32 StoredRadixite = GetStored(ERTSResourceType::Resource_Radixite);
	if (StoredRadixite != M_LastStoredRadixite)
	{
		++M_RadixiteStoreChanges;
		M_LastStoredRadixite = StoredRadixite;
	}
	const bool bAllFinished = M_Harvesters[0].bFinished && M_Harvesters[1].bFinished;
	if (not bAllFinished && GetPhaseSeconds() < UnloadTimeoutSeconds)
	{
		return;
	}
	for (const FUnloadTestHarvester& TestHarvester : M_Harvesters)
	{
		const FString Name = TestHarvester.Actor.IsValid() ? TestHarvester.Actor->GetName() : TEXT("none");
		Check(TestHarvester.bWasDocked, TEXT("harvester_docked"), Name);
		Check(TestHarvester.bFinished, TEXT("harvester_unloaded"),
		      FString::Printf(TEXT("%s cargo_left=%d"), *Name, GetCargo(TestHarvester)));
		Check(TestHarvester.DockErrorCm >= 0.f && TestHarvester.DockErrorCm <= MaxDockErrorCm,
		      TEXT("dock_cargo_over_pick_point"), FString::Printf(TEXT("%s error_cm=%.0f"), *Name, TestHarvester.DockErrorCm));
		Check(TestHarvester.DockYawErrorDeg >= 0.f && TestHarvester.DockYawErrorDeg <= MaxDockYawErrorDeg,
		      TEXT("dock_faces_away_from_hq"), FString::Printf(TEXT("%s yaw_error=%.1f"), *Name, TestHarvester.DockYawErrorDeg));
	}
	Check(bM_SawBothDocked, TEXT("both_bays_docked_at_once"));
	Check(bM_SawBothTrolleysBusy, TEXT("both_trolleys_worked_at_once"));
	const int32 RadixiteGain = GetStored(ERTSResourceType::Resource_Radixite) - M_StoredRadixiteBefore;
	const int32 MetalGain = GetStored(ERTSResourceType::Resource_Metal) - M_StoredMetalBefore;
	Check(RadixiteGain == RadixiteCargo, TEXT("radixite_credited_exactly"), FString::Printf(TEXT("gain=%d"), RadixiteGain));
	Check(MetalGain == MetalCargo, TEXT("metal_credited_exactly"), FString::Printf(TEXT("gain=%d"), MetalGain));
	Check(M_RadixiteStoreChanges >= 3, TEXT("radixite_arrived_in_chunks"),
	      FString::Printf(TEXT("store_changes=%d"), M_RadixiteStoreChanges));
	Check(not Station->GetBayHarvester(0) && not Station->GetBayHarvester(1), TEXT("bays_released"));
	CheckLockResults();
	CheckUnloadProgressResults();
	CheckRigGeometryResults();
	CaptureViews(TEXT("after_unload"));

	// Third trip, for the pack-up case: the radixite harvester goes again.
	EmptyStores();
	M_PackUpStoredBefore = GetStored(ERTSResourceType::Resource_Radixite);
	OrderReturnCargo(M_Harvesters[0], ERTSResourceType::Resource_Radixite, PackUpCargo);
	M_PackUpCargoBefore = GetCargo(M_Harvesters[0]);
	EnterPhase(EUnloadStationTestPhase::PackUpMidUnload);
}

void UUnloadStationTestSubsystem::TickSingleBayUnloadResults(AResourceUnloadStation& Station)
{
	using namespace UnloadStationTestPrivate;
	bM_SawCraneBusy |= Station.GetTrolleyPhase(0) != EUnloadTrolleyPhase::Idle;
	TickSingleBayCaptures(Station);
	TickRigGeometry(Station);
	const int32 StoredRadixite = GetStored(M_TrackedResource);
	if (StoredRadixite != M_LastStoredRadixite)
	{
		++M_RadixiteStoreChanges;
		M_LastStoredRadixite = StoredRadixite;
	}
	const bool bAllFinished = M_Harvesters[0].bFinished && M_Harvesters[1].bFinished;
	if (not bAllFinished && GetPhaseSeconds() < UnloadTimeoutSeconds)
	{
		return;
	}
	int32 DockedIndex = INDEX_NONE;
	int32 DockedCount = 0;
	for (int32 Index = 0; Index < M_Harvesters.Num(); ++Index)
	{
		const FUnloadTestHarvester& TestHarvester = M_Harvesters[Index];
		const FString Name = TestHarvester.Actor.IsValid() ? TestHarvester.Actor->GetName() : TEXT("none");
		Check(TestHarvester.bFinished, TEXT("harvester_unloaded"),
		      FString::Printf(TEXT("%s cargo_left=%d docked=%d"), *Name, GetCargo(TestHarvester),
		                      TestHarvester.bWasDocked ? 1 : 0));
		if (not TestHarvester.bWasDocked)
		{
			continue;
		}
		DockedIndex = Index;
		++DockedCount;
		Check(TestHarvester.DockErrorCm >= 0.f && TestHarvester.DockErrorCm <= MaxDockErrorCm,
		      TEXT("dock_cargo_over_pick_point"),
		      FString::Printf(TEXT("%s error_cm=%.0f"), *Name, TestHarvester.DockErrorCm));
		Check(TestHarvester.DockYawErrorDeg >= 0.f && TestHarvester.DockYawErrorDeg <= MaxDockYawErrorDeg,
		      TEXT("dock_faces_bay_yaw"),
		      FString::Printf(TEXT("%s yaw_error=%.1f"), *Name, TestHarvester.DockYawErrorDeg));
	}
	Check(DockedCount == 1, TEXT("one_harvester_docked"), FString::Printf(TEXT("docked=%d"), DockedCount));
	Check(bM_SawCraneBusy, TEXT("station_rig_worked"));
	if (DockedCount == 1)
	{
		const FUnloadTestHarvester& Docked = M_Harvesters[DockedIndex];
		const FUnloadTestHarvester& Overflow = M_Harvesters[1 - DockedIndex];
		FVector OverflowPoint = FVector::ZeroVector;
		Check(Station.GetOverflowDropOffLocation(OverflowPoint), TEXT("station_has_overflow_point"));
		const float OverflowError = FVector::Dist2D(Overflow.FinishedLocation, OverflowPoint);
		const float Clearance = FVector::Dist2D(Overflow.FinishedLocation, Docked.DockedLocation);
		Check(OverflowError <= MaxOverflowErrorCm, TEXT("bayless_harvester_used_overflow_point"),
		      FString::Printf(TEXT("error_cm=%.0f"), OverflowError));
		Check(Clearance >= MinOverflowClearanceCm, TEXT("bayless_harvester_clear_of_bay"),
		      FString::Printf(TEXT("centre_distance_cm=%.0f"), Clearance));
	}
	const int32 StoredBefore = M_TrackedResource == ERTSResourceType::Resource_Metal
		                           ? M_StoredMetalBefore
		                           : M_StoredRadixiteBefore;
	const int32 Gain = GetStored(M_TrackedResource) - StoredBefore;
	Check(Gain == 2 * M_SingleBayCargoEach, TEXT("cargo_credited_exactly"),
	      FString::Printf(TEXT("%s gain=%d expected=%d"), *Global_GetResourceTypeAsString(M_TrackedResource), Gain,
	                      2 * M_SingleBayCargoEach));
	// Three (crane) or four (elevator) chunks plus the overflow drop.
	Check(M_RadixiteStoreChanges >= 4, TEXT("cargo_arrived_in_chunks"),
	      FString::Printf(TEXT("store_changes=%d"), M_RadixiteStoreChanges));
	Check(not Station.GetBayHarvester(0), TEXT("bay_released"));
	CheckLockResults();
	CheckUnloadProgressResults();
	CheckRigGeometryResults();
	CaptureViews(TEXT("after_unload"));

	// Second trip of the crane harvester, for the pack-up case. The other one goes, so its deliveries cannot count.
	M_PackUpHarvesterIndex = DockedIndex == INDEX_NONE ? 0 : DockedIndex;
	if (AActor* Other = M_Harvesters[1 - M_PackUpHarvesterIndex].Actor.Get())
	{
		Other->Destroy();
	}
	EmptyStores();
	M_PackUpStoredBefore = GetStored(M_TrackedResource);
	OrderReturnCargo(M_Harvesters[M_PackUpHarvesterIndex], M_TrackedResource,
	                 FMath::Min(PackUpCargo, M_SingleBayCargoEach));
	M_PackUpCargoBefore = GetCargo(M_Harvesters[M_PackUpHarvesterIndex]);
	EnterPhase(EUnloadStationTestPhase::PackUpMidUnload);
}

void UUnloadStationTestSubsystem::TickPackUpMidUnload()
{
	using namespace UnloadStationTestPrivate;
	AResourceUnloadStation* Station = M_Station.Get();
	if (not IsValid(Station) || not M_HQ.IsValid())
	{
		Check(false, TEXT("station_alive_before_pack_up"));
		Finish();
		return;
	}
	const UHarvester* Harvester = M_Harvesters[M_PackUpHarvesterIndex].Harvester.Get();
	int32 Bay = INDEX_NONE;
	for (int32 BayIndex = 0; BayIndex < Station->GetBayCount(); ++BayIndex)
	{
		Bay = Station->GetBayHarvester(BayIndex) == Harvester ? BayIndex : Bay;
	}
	if (Bay != INDEX_NONE && Station->GetIsBayDocked(Bay) && not bM_HasPackUpBaseline)
	{
		bM_HasPackUpBaseline = true;
		M_PackUpCargoBefore = GetCargo(M_Harvesters[M_PackUpHarvesterIndex]);
		M_PackUpStoredBefore = GetStored(M_TrackedResource);
	}
	// Pack up while the second load hangs in the air, after one load has already landed.
	const bool bLoadInAir = Bay != INDEX_NONE && Station->GetTrolleyPhase(Bay) == EUnloadTrolleyPhase::MovingToBin;
	const bool bOneLanded = bM_HasPackUpBaseline
		&& GetStored(M_TrackedResource) > M_PackUpStoredBefore;
	if (bLoadInAir && bOneLanded)
	{
		CaptureViews(TEXT("pack_up_moment"));
		ICommands* HQCommands = Cast<ICommands>(M_HQ.Get());
		const ECommandQueueError Error = HQCommands ? HQCommands->ConvertToVehicle(true) : ECommandQueueError::CommandDataInvalid;
		Check(Error == ECommandQueueError::NoError, TEXT("hq_pack_up_ordered"));
		bM_PackUpOrdered = true;
		EnterPhase(EUnloadStationTestPhase::WaitingForPackUpResult);
		return;
	}
	if (GetPhaseSeconds() > UnloadTimeoutSeconds)
	{
		Check(false, TEXT("pack_up_moment_reached"), FString::Printf(TEXT("bay=%d"), Bay));
		Finish();
	}
}

void UUnloadStationTestSubsystem::TickWaitingForPackUpResult()
{
	using namespace UnloadStationTestPrivate;
	if (M_Station.IsValid() && not M_Station->IsActorBeingDestroyed())
	{
		if (GetPhaseSeconds() > PackUpTimeoutSeconds)
		{
			Check(false, TEXT("station_removed_on_pack_up"));
			Finish();
		}
		return;
	}
	const UHarvester* Harvester = M_Harvesters[M_PackUpHarvesterIndex].Harvester.Get();
	const int32 CargoLeft = GetCargo(M_Harvesters[M_PackUpHarvesterIndex]);
	const int32 Credited = GetStored(M_TrackedResource) - M_PackUpStoredBefore;
	Check(true, TEXT("station_removed_on_pack_up"), FString::Printf(TEXT("t=%.1f"), GetPhaseSeconds()));
	Check(IsValid(Harvester) && Harvester->MapTest_GetHarvestStatus() != EHarvesterAIAction::CraneUnloading,
	      TEXT("harvester_left_crane_state"));
	Check(CargoLeft + Credited == M_PackUpCargoBefore, TEXT("pack_up_conserves_cargo"),
	      FString::Printf(TEXT("before=%d credited=%d on_board=%d"), M_PackUpCargoBefore, Credited, CargoLeft));
	Check(CargoLeft > 0, TEXT("pack_up_left_cargo_on_board"), FString::Printf(TEXT("on_board=%d"), CargoLeft));
	Finish();
}

void UUnloadStationTestSubsystem::EmptyStores() const
{
	UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
	if (not IsValid(DropOff))
	{
		return;
	}
	for (const ERTSResourceType Resource : {ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal})
	{
		const int32 Stored = DropOff->GetDropOffAmountStored(Resource);
		if (Stored > 0)
		{
			DropOff->RemoveResources(Resource, Stored);
		}
	}
}

int32 UUnloadStationTestSubsystem::GetStored(const ERTSResourceType Resource) const
{
	const UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
	return IsValid(DropOff) ? DropOff->GetDropOffAmountStored(Resource) : 0;
}

int32 UUnloadStationTestSubsystem::GetCargo(const FUnloadTestHarvester& TestHarvester) const
{
	const UHarvester* Harvester = TestHarvester.Harvester.Get();
	if (not IsValid(Harvester))
	{
		return -1;
	}
	int32 Current = 0;
	ERTSResourceType Resource = ERTSResourceType::Resource_None;
	(void)Harvester->GetMaxCapacityForTargetResource(Current, Resource);
	return Resource == TestHarvester.Resource ? Current : 0;
}

void UUnloadStationTestSubsystem::StartGameAndUnpause() const
{
	ACPPController* Controller = GetController();
	UPlayerStartGameControl* StartGameControl = IsValid(Controller)
		                                            ? Controller->FindComponentByClass<UPlayerStartGameControl>()
		                                            : nullptr;
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UUnloadStationTestSubsystem::TickSingleBayCaptures(const AResourceUnloadStation& Station)
{
	using namespace UnloadStationTestPrivate;
	const EUnloadTrolleyPhase Phase = Station.GetTrolleyPhase(0);
	if (not bM_CapturedLowering && Phase == EUnloadTrolleyPhase::LoweringToPick)
	{
		bM_CapturedLowering = true;
		CaptureViews(TEXT("rig_lowering"));
	}
	if (not bM_CapturedBusy && Phase == EUnloadTrolleyPhase::MovingToBin)
	{
		bM_CapturedBusy = true;
		M_BusyStartWorldSeconds = GetWorld()->GetTimeSeconds();
		CaptureViews(TEXT("station_busy"));
	}
	if (M_BusyStartWorldSeconds < 0.f || M_MidCapturesTaken >= MidUnloadCaptureSeconds.Num())
	{
		return;
	}
	const float BusySeconds = GetWorld()->GetTimeSeconds() - M_BusyStartWorldSeconds;
	if (BusySeconds >= MidUnloadCaptureSeconds[M_MidCapturesTaken])
	{
		CaptureViews(FString::Printf(TEXT("mid_unload_%02.0fs"), MidUnloadCaptureSeconds[M_MidCapturesTaken]));
		++M_MidCapturesTaken;
	}
}

void UUnloadStationTestSubsystem::GiveLockOrder(FUnloadTestHarvester& TestHarvester)
{
	using namespace UnloadStationTestPrivate;
	ICommands* Commands = Cast<ICommands>(TestHarvester.Actor.Get());
	const UHarvester* Harvester = TestHarvester.Harvester.Get();
	if (TestHarvester.bLockOrderGiven || Commands == nullptr || not IsValid(Harvester))
	{
		return;
	}
	TestHarvester.bLockOrderGiven = true;
	TestHarvester.bLockOrderIsStop = M_LockOrdersGiven == 1;
	++M_LockOrdersGiven;
	Check(Harvester->GetIsLockedForUnload(), TEXT("docked_harvester_is_locked"));
	// Straight ahead: it reversed in, so its nose points away from the station.
	const FVector MoveTarget = TestHarvester.Actor->GetActorLocation()
		+ TestHarvester.Actor->GetActorForwardVector().GetSafeNormal2D() * LockMoveDistanceCm;
	const ECommandQueueError MoveError = Commands->MoveToLocation(MoveTarget, true, FRotator::ZeroRotator, false);
	Check(MoveError == ECommandQueueError::NoError, TEXT("lock_move_order_accepted"));
	if (TestHarvester.bLockOrderIsStop)
	{
		(void)Commands->StopCommand();
	}
	TestHarvester.bLockOrderHeld = Harvester->MapTest_GetIsHoldingOrder();
	TestHarvester.bLockKeptUnloading = Harvester->MapTest_GetHarvestStatus() == EHarvesterAIAction::CraneUnloading;
	UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST lock_order harvester=%s stop=%d held=%d t=%.1f"),
	       *TestHarvester.Actor->GetName(), TestHarvester.bLockOrderIsStop ? 1 : 0,
	       TestHarvester.bLockOrderHeld ? 1 : 0, GetPhaseSeconds());
}

void UUnloadStationTestSubsystem::TrackLockedHarvester(FUnloadTestHarvester& TestHarvester,
                                                       const EHarvesterAIAction Status) const
{
	if (not TestHarvester.bLockOrderGiven || TestHarvester.bFinished || not TestHarvester.Actor.IsValid())
	{
		return;
	}
	if (Status != EHarvesterAIAction::CraneUnloading)
	{
		return;
	}
	const float Drift = FVector::Dist2D(TestHarvester.Actor->GetActorLocation(), TestHarvester.DockedLocation);
	TestHarvester.MaxDockDriftCm = FMath::Max(TestHarvester.MaxDockDriftCm, Drift);
}

void UUnloadStationTestSubsystem::OnLockedHarvesterFinished(FUnloadTestHarvester& TestHarvester) const
{
	ICommands* Commands = Cast<ICommands>(TestHarvester.Actor.Get());
	if (not TestHarvester.bLockOrderGiven || Commands == nullptr)
	{
		return;
	}
	TestHarvester.bHeldMoveRanAfterUnload = Commands->GetActiveCommandID() == EAbilityID::IdMove;
	UE_LOG(LogRTSUnloadStationTest, Display,
	       TEXT("RTS_UNLOAD_TEST lock_released harvester=%s move_active=%d drift_cm=%.0f"),
	       *TestHarvester.Actor->GetName(), TestHarvester.bHeldMoveRanAfterUnload ? 1 : 0,
	       TestHarvester.MaxDockDriftCm);
}

void UUnloadStationTestSubsystem::CheckLockResults()
{
	using namespace UnloadStationTestPrivate;
	Check(M_LockOrdersGiven > 0, TEXT("lock_order_given"), FString::Printf(TEXT("orders=%d"), M_LockOrdersGiven));
	for (const FUnloadTestHarvester& TestHarvester : M_Harvesters)
	{
		if (not TestHarvester.bLockOrderGiven)
		{
			continue;
		}
		const FString Name = FString::Printf(TEXT("%s stop=%d"),
		                                     TestHarvester.Actor.IsValid() ? *TestHarvester.Actor->GetName() : TEXT("none"),
		                                     TestHarvester.bLockOrderIsStop ? 1 : 0);
		Check(TestHarvester.DockedPhaseSeconds >= 0.f && TestHarvester.DockedPhaseSeconds <= MaxDockSeconds,
		      TEXT("docked_without_stall"), FString::Printf(TEXT("%s t=%.1f"), *Name, TestHarvester.DockedPhaseSeconds));
		Check(TestHarvester.bLockOrderHeld, TEXT("lock_order_held"), Name);
		Check(TestHarvester.bLockKeptUnloading, TEXT("lock_kept_unloading"), Name);
		Check(TestHarvester.MaxDockDriftCm <= MaxLockDriftCm, TEXT("lock_stayed_docked"),
		      FString::Printf(TEXT("%s drift_cm=%.0f"), *Name, TestHarvester.MaxDockDriftCm));
		// The held move runs once the cargo is gone; after a stop nothing waits, so no move runs.
		Check(TestHarvester.bHeldMoveRanAfterUnload != TestHarvester.bLockOrderIsStop,
		      TEXT("held_order_ran_after_unload"),
		      FString::Printf(TEXT("%s move_active=%d"), *Name, TestHarvester.bHeldMoveRanAfterUnload ? 1 : 0));
	}
}

void UUnloadStationTestSubsystem::CheckUnloadProgressResults()
{
	for (const FUnloadTestHarvester& TestHarvester : M_Harvesters)
	{
		if (not TestHarvester.bWasDocked)
		{
			continue;
		}
		const FString Name = TestHarvester.Actor.IsValid() ? TestHarvester.Actor->GetName() : TEXT("none");
		Check(TestHarvester.bSawProgressShown, TEXT("unload_progress_shown"), Name);
		Check(not TestHarvester.bProgressWentDown, TEXT("unload_progress_never_drops"), Name);
		// Three loads land over ~37 s: two of three credited plus the time estimate reach well past half.
		Check(TestHarvester.MaxProgressFill >= 0.6f, TEXT("unload_progress_filled"),
		      FString::Printf(TEXT("%s max_fill=%.2f"), *Name, TestHarvester.MaxProgressFill));
		Check(TestHarvester.bProgressHiddenAfterUnload, TEXT("unload_progress_hidden_after"), Name);
	}
}

void UUnloadStationTestSubsystem::TickRigGeometry(const AResourceUnloadStation& Station)
{
	if (M_StationKind == EUnloadTestStation::MetalDepot)
	{
		return;
	}
	M_LastRigPhases.SetNumZeroed(Station.GetBayCount());
	for (int32 BayIndex = 0; BayIndex < Station.GetBayCount(); ++BayIndex)
	{
		const EUnloadTrolleyPhase Phase = Station.GetTrolleyPhase(BayIndex);
		const bool bEntered = static_cast<uint8>(Phase) != M_LastRigPhases[BayIndex];
		M_LastRigPhases[BayIndex] = static_cast<uint8>(Phase);
		if (not bEntered || not Station.GetIsBayDocked(BayIndex))
		{
			continue;
		}
		const FVector Grab = Station.GetGrabWorldLocation(BayIndex);
		if (Phase == EUnloadTrolleyPhase::Grabbing)
		{
			const FVector Pick = Station.GetBayPickWorldLocation(BayIndex);
			const float Off2D = FVector::Dist2D(Grab, Pick);
			M_MaxGrabOffPick2DCm = FMath::Max(M_MaxGrabOffPick2DCm, Off2D);
			++M_GrabMoments;
			UE_LOG(LogRTSUnloadStationTest, Display,
			       TEXT("RTS_UNLOAD_TEST rig_grab bay=%d off_pick_2d_cm=%.0f grab_above_pick_cm=%.0f t=%.1f"),
			       BayIndex, Off2D, Grab.Z - Pick.Z, GetPhaseSeconds());
		}
		FVector Bin;
		if (Phase == EUnloadTrolleyPhase::Releasing && Station.GetBayBinWorldLocation(BayIndex, Bin))
		{
			const float Off2D = FVector::Dist2D(Grab, Bin);
			M_MaxReleaseOffBin2DCm = FMath::Max(M_MaxReleaseOffBin2DCm, Off2D);
			++M_ReleaseMoments;
			UE_LOG(LogRTSUnloadStationTest, Display,
			       TEXT("RTS_UNLOAD_TEST rig_release bay=%d off_bin_2d_cm=%.0f grab_above_bin_cm=%.0f t=%.1f"),
			       BayIndex, Off2D, Grab.Z - Bin.Z, GetPhaseSeconds());
		}
	}
}

void UUnloadStationTestSubsystem::CheckRigGeometryResults()
{
	using namespace UnloadStationTestPrivate;
	if (M_StationKind == EUnloadTestStation::MetalDepot)
	{
		return;
	}
	Check(M_GrabMoments > 0 && M_ReleaseMoments > 0, TEXT("rig_grabbed_and_released"),
	      FString::Printf(TEXT("grabs=%d releases=%d"), M_GrabMoments, M_ReleaseMoments));
	Check(M_MaxGrabOffPick2DCm <= MaxGrabOffPick2DCm, TEXT("rig_grabs_over_hopper"),
	      FString::Printf(TEXT("max_off_cm=%.0f"), M_MaxGrabOffPick2DCm));
	Check(M_MaxReleaseOffBin2DCm <= MaxReleaseOffBin2DCm, TEXT("rig_releases_over_bin"),
	      FString::Printf(TEXT("max_off_cm=%.0f"), M_MaxReleaseOffBin2DCm));
}

void UUnloadStationTestSubsystem::TickCaptureSequence()
{
	if (M_CaptureEverySeconds <= 0.f || not M_CaptureRig.GetIsEnabled() || not M_Station.IsValid() || not M_HQ.IsValid())
	{
		return;
	}
	bool bAnyBayDocked = false;
	for (int32 BayIndex = 0; BayIndex < M_Station->GetBayCount(); ++BayIndex)
	{
		bAnyBayDocked |= M_Station->GetIsBayDocked(BayIndex);
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (not bAnyBayDocked || Now - M_LastSequenceCaptureWorldSeconds < M_CaptureEverySeconds)
	{
		return;
	}
	M_LastSequenceCaptureWorldSeconds = Now;
	// The first two views: the rig and where the load lands.
	constexpr int32 SequenceViewCount = 2;
	CaptureViews(FString::Printf(TEXT("seq_%03d"), M_SequenceFrameIndex++), SequenceViewCount);
}

void UUnloadStationTestSubsystem::CaptureViews(const FString& Label, const int32 MaxViews) const
{
	if (not M_CaptureRig.GetIsEnabled() || not M_HQ.IsValid())
	{
		return;
	}
	const UStaticMeshComponent* BuildingMesh = M_HQ->MapTest_GetBuildingMeshComponent();
	if (not IsValid(BuildingMesh))
	{
		return;
	}
	FTransform Frame = BuildingMesh->GetComponentTransform();
	Frame.SetScale3D(FVector::OneVector);
	const TArray<FB4HowitzerCameraView>& Views = M_StationKind == EUnloadTestStation::Refinery
		                                             ? UnloadStationTestPrivate::RefineryViews
		                                             : M_StationKind == EUnloadTestStation::MetalDepot
		                                             ? UnloadStationTestPrivate::MetalDepotViews
		                                             : UnloadStationTestPrivate::Views;
	if (MaxViews <= 0 || MaxViews >= Views.Num())
	{
		M_CaptureRig.CaptureViews(*GetWorld(), Frame, Views, Label);
		return;
	}
	TArray<FB4HowitzerCameraView> FirstViews;
	for (int32 ViewIndex = 0; ViewIndex < MaxViews; ++ViewIndex)
	{
		FirstViews.Add(Views[ViewIndex]);
	}
	M_CaptureRig.CaptureViews(*GetWorld(), Frame, FirstViews, Label);
}

void UUnloadStationTestSubsystem::EnterPhase(const EUnloadStationTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UUnloadStationTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UUnloadStationTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UUnloadStationTestSubsystem::Finish()
{
	M_Phase = EUnloadStationTestPhase::Inactive;
	UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	bM_IsExitPending = true;
	M_ExitFramesLeft = UnloadStationTestPrivate::FramesToWriteLastCapture;
}

void UUnloadStationTestSubsystem::TickExitPending()
{
	if (--M_ExitFramesLeft > 0)
	{
		return;
	}
	bM_IsExitPending = false;
	FPlatformMisc::RequestExit(false, TEXT("UnloadStationValidate"));
}

ACPPController* UUnloadStationTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

#else

bool UUnloadStationTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UUnloadStationTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UUnloadStationTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnloadStationTestSubsystem, STATGROUP_Tickables);
}

#endif
