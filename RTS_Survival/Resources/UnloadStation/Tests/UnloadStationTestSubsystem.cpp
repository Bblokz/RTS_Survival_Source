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
	const TCHAR* const TruckSwitch = TEXT("UnloadStationTruck=");
	const TCHAR* const RefineryPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Ger_Refinery_Truck.Ger_Refinery_Truck_C");
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
	bM_IsRefinery = FParse::Value(FCommandLine::Get(), TruckSwitch, Truck)
		&& Truck.Equals(TEXT("Refinery"), ESearchCase::IgnoreCase);
	UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST truck=%s"),
	       bM_IsRefinery ? TEXT("Refinery") : TEXT("HQ"));
	FString Capture;
	if (FParse::Value(FCommandLine::Get(), CaptureSwitch, Capture))
	{
		M_CaptureRig.Init(*GetWorld(), Capture);
	}
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
	UClass* HQClass = LoadClass<ANomadicVehicle>(nullptr, bM_IsRefinery ? RefineryPath : HQPath);
	FVector Ground;
	if (not IsValid(World) || not IsValid(HQClass)
		|| not GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(Anchor, 0.f), Ground))
	{
		return;
	}
	M_Anchor = Ground;
	const FTransform SpawnTransform(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, 150.f));
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
	Check(Station->GetBayCount() == (bM_IsRefinery ? 1 : 2), TEXT("station_bay_count"),
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
	M_LastStoredRadixite = M_StoredRadixiteBefore;
	if (bM_IsRefinery)
	{
		// Both carry radixite (the refinery has no metal bin); keep the pair within what the store can take.
		const UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
		const int32 Room = IsValid(DropOff) ? DropOff->GetDropOffCapacity(ERTSResourceType::Resource_Radixite) : 0;
		M_RadixiteCargoEach = FMath::Min(RadixiteCargo, Room / 2);
		Check(M_RadixiteCargoEach >= 60, TEXT("refinery_has_room"),
		      FString::Printf(TEXT("room=%d each=%d"), Room, M_RadixiteCargoEach));
		OrderReturnCargo(M_Harvesters[0], ERTSResourceType::Resource_Radixite, M_RadixiteCargoEach);
		OrderReturnCargo(M_Harvesters[1], ERTSResourceType::Resource_Radixite, M_RadixiteCargoEach);
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
	if (Status == EHarvesterAIAction::CraneUnloading && not TestHarvester.bWasDocked)
	{
		TestHarvester.bWasDocked = true;
		for (int32 BayIndex = 0; BayIndex < Station.GetBayCount(); ++BayIndex)
		{
			if (Station.GetBayHarvester(BayIndex) != Harvester)
			{
				continue;
			}
			const FVector Pick = Station.GetBayPickWorldLocation(BayIndex);
			TestHarvester.DockErrorCm = FVector::Dist2D(Pick, Harvester->GetCargoWorldLocation());
			const float BayYaw = M_HQ.IsValid() ? M_HQ->GetActorRotation().Yaw : 0.f;
			TestHarvester.DockYawErrorDeg = FMath::Abs(FMath::FindDeltaAngleDegrees(
				TestHarvester.Actor->GetActorRotation().Yaw, BayYaw));
			TestHarvester.DockedLocation = TestHarvester.Actor->GetActorLocation();
			UE_LOG(LogRTSUnloadStationTest, Display,
			       TEXT("RTS_UNLOAD_TEST docked harvester=%s bay=%d dock_error_cm=%.0f yaw_error=%.1f t=%.1f"),
			       *TestHarvester.Actor->GetName(), BayIndex, TestHarvester.DockErrorCm,
			       TestHarvester.DockYawErrorDeg, GetPhaseSeconds());
		}
	}
	// On the refinery the bay-less harvester empties at the overflow point without ever docking.
	if ((TestHarvester.bWasDocked || bM_IsRefinery) && GetCargo(TestHarvester) == 0
		&& Status != EHarvesterAIAction::CraneUnloading)
	{
		TestHarvester.bFinished = true;
		TestHarvester.FinishedLocation = TestHarvester.Actor.IsValid()
			                                 ? TestHarvester.Actor->GetActorLocation()
			                                 : FVector::ZeroVector;
		UE_LOG(LogRTSUnloadStationTest, Display, TEXT("RTS_UNLOAD_TEST unloaded harvester=%s t=%.1f status=%s"),
		       *TestHarvester.Actor->GetName(), GetPhaseSeconds(), *GetStringHarvesterAIStatus(Status));
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
	if (bM_IsRefinery)
	{
		TickRefineryUnloadResults(*Station);
		return;
	}
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
	CaptureViews(TEXT("after_unload"));

	// Third trip, for the pack-up case: the radixite harvester goes again.
	EmptyStores();
	M_PackUpStoredBefore = GetStored(ERTSResourceType::Resource_Radixite);
	OrderReturnCargo(M_Harvesters[0], ERTSResourceType::Resource_Radixite, PackUpCargo);
	M_PackUpCargoBefore = GetCargo(M_Harvesters[0]);
	EnterPhase(EUnloadStationTestPhase::PackUpMidUnload);
}

void UUnloadStationTestSubsystem::TickRefineryUnloadResults(AResourceUnloadStation& Station)
{
	using namespace UnloadStationTestPrivate;
	bM_SawCraneBusy |= Station.GetTrolleyPhase(0) != EUnloadTrolleyPhase::Idle;
	if (not bM_CapturedBusy && Station.GetTrolleyPhase(0) == EUnloadTrolleyPhase::MovingToBin)
	{
		bM_CapturedBusy = true;
		CaptureViews(TEXT("boom_slewing"));
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
		      TEXT("dock_faces_away_from_refinery"),
		      FString::Printf(TEXT("%s yaw_error=%.1f"), *Name, TestHarvester.DockYawErrorDeg));
	}
	Check(DockedCount == 1, TEXT("one_harvester_docked"), FString::Printf(TEXT("docked=%d"), DockedCount));
	Check(bM_SawCraneBusy, TEXT("knuckle_crane_worked"));
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
	const int32 RadixiteGain = GetStored(ERTSResourceType::Resource_Radixite) - M_StoredRadixiteBefore;
	Check(RadixiteGain == 2 * M_RadixiteCargoEach, TEXT("radixite_credited_exactly"),
	      FString::Printf(TEXT("gain=%d expected=%d"), RadixiteGain, 2 * M_RadixiteCargoEach));
	// Three crane chunks plus the overflow drop.
	Check(M_RadixiteStoreChanges >= 4, TEXT("radixite_arrived_in_chunks"),
	      FString::Printf(TEXT("store_changes=%d"), M_RadixiteStoreChanges));
	Check(not Station.GetBayHarvester(0), TEXT("bay_released"));
	CaptureViews(TEXT("after_unload"));

	// Second trip of the crane harvester, for the pack-up case. The other one goes, so its deliveries cannot count.
	M_PackUpHarvesterIndex = DockedIndex == INDEX_NONE ? 0 : DockedIndex;
	if (AActor* Other = M_Harvesters[1 - M_PackUpHarvesterIndex].Actor.Get())
	{
		Other->Destroy();
	}
	EmptyStores();
	M_PackUpStoredBefore = GetStored(ERTSResourceType::Resource_Radixite);
	OrderReturnCargo(M_Harvesters[M_PackUpHarvesterIndex], ERTSResourceType::Resource_Radixite,
	                 FMath::Min(PackUpCargo, M_RadixiteCargoEach));
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
		M_PackUpStoredBefore = GetStored(ERTSResourceType::Resource_Radixite);
	}
	// Pack up while the second load hangs in the air, after one load has already landed.
	const bool bLoadInAir = Bay != INDEX_NONE && Station->GetTrolleyPhase(Bay) == EUnloadTrolleyPhase::MovingToBin;
	const bool bOneLanded = bM_HasPackUpBaseline
		&& GetStored(ERTSResourceType::Resource_Radixite) > M_PackUpStoredBefore;
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
	const int32 Credited = GetStored(ERTSResourceType::Resource_Radixite) - M_PackUpStoredBefore;
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

void UUnloadStationTestSubsystem::CaptureViews(const FString& Label) const
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
	M_CaptureRig.CaptureViews(*GetWorld(), Frame,
	                          bM_IsRefinery ? UnloadStationTestPrivate::RefineryViews : UnloadStationTestPrivate::Views,
	                          Label);
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
