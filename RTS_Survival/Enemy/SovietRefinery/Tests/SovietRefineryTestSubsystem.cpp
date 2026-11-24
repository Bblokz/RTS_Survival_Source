// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietRefineryTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "EngineUtils.h"
#include "RTS_Survival/Enemy/SovietRefinery/SovietRefineryComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/Harvester/HarvesterStatus/HarvesterStatus.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/UnloadStation/SkeletalCraneUnloadStation.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovietRefineryTest, Log, All);

namespace SovietRefineryTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("SovietRefineryValidate");
	const TCHAR* const DesignSwitch = TEXT("SovietRefineryDesign=");
	const TCHAR* const AnchorSwitch = TEXT("SovietRefineryAt=");
	const TCHAR* const CaptureSwitch = TEXT("SovietRefineryCapture=");
	const TCHAR* const A1Path = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietRefinery/Rus_Building_SovietRefinery_A1.Rus_Building_SovietRefinery_A1_C");
	const TCHAR* const G2Path = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietRefinery/Rus_Building_SovietRefinery_G2.Rus_Building_SovietRefinery_G2_C");
	const FVector2D DefaultAnchor(16796.f, -15808.f);
	// RTS_ToCopySmall has no metal and only a few small radixite crystals: each refinery gets its own two fields in
	// front of it (building frame +Y, past the dock), so the natural loop has something to harvest.
	const TCHAR* const RadixiteFieldPath =
		TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/Resource_Radixite_Crystal_NoDecal.Resource_Radixite_Crystal_NoDecal_C");
	const TCHAR* const MetalFieldPath =
		TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/Resource_Metal_Barrels.Resource_Metal_Barrels_C");
	const FVector2D RadixiteFieldOffset(-1800.f, 6500.f);
	const FVector2D MetalFieldOffset(1800.f, 6500.f);
	// The second design stands this far along X from the first (RTS_ToCopySmall has no ground past X ~25000).
	constexpr float SecondDesignOffsetCm = -9000.f;
	constexpr uint8 EnemyPlayer = 2;
	constexpr int32 TestMaxHarvesters = 2;
	constexpr float TestTrainSeconds = 3.f;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float ActiveTimeoutSeconds = 30.f;
	constexpr float HarvesterTimeoutSeconds = 90.f;
	constexpr float ForcedTimeoutSeconds = 240.f;
	constexpr float NaturalTimeoutSeconds = 480.f;
	constexpr int32 ForcedRadixiteCargo = 300;
	constexpr int32 ForcedMetalCargo = 150;
	// The jaws must come down within this distance (2D) of the docked hopper centre.
	constexpr float MaxJawOffPick2DCm = 70.f;
	constexpr int32 FramesToWriteLastCapture = 3;
}

bool USovietRefineryTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), SovietRefineryTestPrivate::ValidateSwitch);
}

TStatId USovietRefineryTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovietRefineryTestSubsystem, STATGROUP_Tickables);
}

void USovietRefineryTestSubsystem::Tick(const float DeltaTime)
{
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	const UWorld* World = GetWorld();
	if (M_Phase == ESovietRefineryTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST started map=%s"), *World->GetName());
	}
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		TrackCrane(Test);
		TrackHarvesters(Test);
	}
	switch (M_Phase)
	{
	case ESovietRefineryTestPhase::WaitingForStart: TickWaitingForStart(); return;
	case ESovietRefineryTestPhase::WaitingForActive: TickWaitingForActive(); return;
	case ESovietRefineryTestPhase::WaitingForHarvesters: TickWaitingForHarvesters(); return;
	case ESovietRefineryTestPhase::ForcedUnload: TickForcedUnload(); return;
	case ESovietRefineryTestPhase::NaturalHarvest: TickNaturalHarvest(); return;
	default: return;
	}
}

void USovietRefineryTestSubsystem::TickWaitingForStart()
{
	using namespace SovietRefineryTestPrivate;
	StartGameAndUnpause();
	if (GetPhaseSeconds() < StartDelaySeconds)
	{
		return;
	}
	FString Capture;
	if (FParse::Value(FCommandLine::Get(), CaptureSwitch, Capture))
	{
		M_CaptureRig.Init(*GetWorld(), Capture);
	}
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
	FString Design = TEXT("Both");
	(void)FParse::Value(FCommandLine::Get(), DesignSwitch, Design);
	if (not Design.Equals(TEXT("G2"), ESearchCase::IgnoreCase))
	{
		SpawnRefinery(TEXT("A1"), Anchor);
	}
	if (not Design.Equals(TEXT("A1"), ESearchCase::IgnoreCase))
	{
		SpawnRefinery(TEXT("G2"), Anchor + FVector2D(M_Refineries.Num() * SecondDesignOffsetCm, 0.f));
	}
	M_PlayerRadixiteBefore = GetPlayerResource(ERTSResourceType::Resource_Radixite);
	M_PlayerMetalBefore = GetPlayerResource(ERTSResourceType::Resource_Metal);
	for (const FSovietRefineryUnderTest& Test : M_Refineries)
	{
		Check(Test.Refinery.IsValid(), TEXT("refinery_spawned"), Test.Design);
	}
	if (M_Refineries.IsEmpty())
	{
		Finish();
		return;
	}
	EnterPhase(ESovietRefineryTestPhase::WaitingForActive);
}

void USovietRefineryTestSubsystem::SpawnRefinery(const FString& Design, const FVector2D& Anchor)
{
	using namespace SovietRefineryTestPrivate;
	UWorld* World = GetWorld();
	UClass* RefineryClass = LoadClass<AActor>(nullptr, Design == TEXT("A1") ? A1Path : G2Path);
	FVector Ground;
	FSovietRefineryUnderTest Test;
	Test.Design = Design;
	if (IsValid(World) && IsValid(RefineryClass)
		&& GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(Anchor, 0.f), Ground))
	{
		const FTransform SpawnTransform(FRotator::ZeroRotator, Ground);
		AActor* Building = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, RefineryClass, SpawnTransform, EnemyPlayer);
		Test.Building = Building;
		for (const TPair<const TCHAR*, FVector2D>& Field : {
			     TPair<const TCHAR*, FVector2D>(RadixiteFieldPath, RadixiteFieldOffset),
			     TPair<const TCHAR*, FVector2D>(MetalFieldPath, MetalFieldOffset)})
		{
			UClass* FieldClass = LoadClass<AActor>(nullptr, Field.Key);
			FVector FieldGround;
			if (IsValid(FieldClass)
				&& GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(Anchor + Field.Value, 0.f), FieldGround))
			{
				FActorSpawnParameters FieldSpawn;
				FieldSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const AActor* FieldActor = World->SpawnActor<AActor>(FieldClass, FTransform(FieldGround), FieldSpawn);
				UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST field design=%s field=%s at=%s"), *Design,
				       *GetNameSafe(FieldActor), *FieldGround.ToCompactString());
			}
		}
		Test.Refinery = IsValid(Building) ? Building->FindComponentByClass<USovietRefineryComponent>() : nullptr;
		if (Test.Refinery.IsValid())
		{
			Test.Refinery->MapTest_SetHarvesterTrainSeconds(TestTrainSeconds);
			Test.Refinery->MapTest_SetMaxHarvestersOverride(TestMaxHarvesters);
		}
	}
	UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST spawned design=%s class=%s at=%s refinery=%d"), *Design,
	       *GetNameSafe(RefineryClass), *Ground.ToCompactString(), Test.Refinery.IsValid() ? 1 : 0);
	M_Refineries.Add(Test);
}

void USovietRefineryTestSubsystem::TickWaitingForActive()
{
	using namespace SovietRefineryTestPrivate;
	StartGameAndUnpause();
	bool bAllReady = true;
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		USovietRefineryComponent* Refinery = Test.Refinery.Get();
		if (IsValid(Refinery) && Refinery->GetIsRefineryActive())
		{
			Test.Station = Cast<ASkeletalCraneUnloadStation>(Refinery->GetUnloadStation());
		}
		bAllReady &= Test.Station.IsValid() && Test.Station->GetIsRigReady();
	}
	if (not bAllReady && GetPhaseSeconds() < ActiveTimeoutSeconds)
	{
		return;
	}
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		const ASkeletalCraneUnloadStation* Station = Test.Station.Get();
		Check(IsValid(Station), TEXT("crane_station_spawned"), Test.Design);
		Check(IsValid(Station) && Station->GetIsRigReady(), TEXT("crane_rig_ready"), Test.Design);
		Check(IsValid(Station) && Station->GetBayCount() == 1, TEXT("crane_one_bay"), Test.Design);
		Check(IsValid(Station) && Station->GetHasDropPose(ERTSResourceType::Resource_Radixite)
		      && Station->GetHasDropPose(ERTSResourceType::Resource_Metal), TEXT("crane_reaches_both_drops"), Test.Design);
		Check(IsValid(Station) && Station->GetCanUnloadResource(ERTSResourceType::Resource_Radixite)
		      && Station->GetCanUnloadResource(ERTSResourceType::Resource_Metal), TEXT("crane_has_both_bins"),
		      Test.Design);
		const UResourceDropOff* DropOff = Test.Refinery.IsValid() ? Test.Refinery->GetRefineryDropOff() : nullptr;
		Check(IsValid(DropOff) && DropOff->GetUnloadStation() == Station, TEXT("dropoff_knows_crane"), Test.Design);
		CaptureCrane(Test, TEXT("rest"));
	}
	if (not bAllReady)
	{
		Finish();
		return;
	}
	EnterPhase(ESovietRefineryTestPhase::WaitingForHarvesters);
}

void USovietRefineryTestSubsystem::TickWaitingForHarvesters()
{
	using namespace SovietRefineryTestPrivate;
	bool bAllTrained = true;
	for (const FSovietRefineryUnderTest& Test : M_Refineries)
	{
		bAllTrained &= Test.Refinery.IsValid() && Test.Refinery->GetAliveHarvesterCount() >= TestMaxHarvesters;
	}
	if (not bAllTrained && GetPhaseSeconds() < HarvesterTimeoutSeconds)
	{
		return;
	}
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		const int32 Alive = Test.Refinery.IsValid() ? Test.Refinery->GetAliveHarvesterCount() : 0;
		Check(Alive == TestMaxHarvesters, TEXT("harvesters_trained_to_max"),
		      FString::Printf(TEXT("%s alive=%d"), *Test.Design, Alive));
		bool bAllEnemyOwned = Alive > 0;
		for (const AActor* Harvester : Test.Refinery.IsValid() ? Test.Refinery->GetHarvesters() : TArray<AActor*>())
		{
			bAllEnemyOwned &= FRTSPlayers::GetOwningPlayerOfActor(Harvester) == EnemyPlayer;
		}
		Check(bAllEnemyOwned, TEXT("harvesters_owned_by_ai"), Test.Design);
	}
	if (not bAllTrained)
	{
		Finish();
		return;
	}
	// Forced trip: harvester 0 brings radixite (crane bay), harvester 1 metal (crane or overflow).
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		Test.DeliveredBaselineRadixite = GetDelivered(Test, ERTSResourceType::Resource_Radixite);
		Test.DeliveredBaselineMetal = GetDelivered(Test, ERTSResourceType::Resource_Metal);
		const TArray<AActor*> Harvesters = Test.Refinery->GetHarvesters();
		for (int32 Index = 0; Index < Harvesters.Num() && Index < 2; ++Index)
		{
			UHarvester* Harvester = Harvesters[Index]->FindComponentByClass<UHarvester>();
			ICommands* Commands = Cast<ICommands>(Harvesters[Index]);
			const ERTSResourceType Resource = Index == 0
				                                  ? ERTSResourceType::Resource_Radixite
				                                  : ERTSResourceType::Resource_Metal;
			const int32 Amount = Index == 0 ? ForcedRadixiteCargo : ForcedMetalCargo;
			if (not IsValid(Harvester) || Commands == nullptr)
			{
				Check(false, TEXT("harvester_ready_for_forced_trip"), Test.Design);
				continue;
			}
			Harvester->MapTest_SetCargo(Resource, Amount);
			const ECommandQueueError Error = Commands->ReturnCargo(true);
			Check(Error == ECommandQueueError::NoError, TEXT("forced_return_ordered"),
			      FString::Printf(TEXT("%s %s=%d"), *Test.Design, *Global_GetResourceTypeAsString(Resource), Amount));
			(Index == 0 ? Test.ForcedRadixite : Test.ForcedMetal) = Amount;
		}
	}
	EnterPhase(ESovietRefineryTestPhase::ForcedUnload);
}

void USovietRefineryTestSubsystem::TickForcedUnload()
{
	using namespace SovietRefineryTestPrivate;
	bool bAllDone = true;
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		const int32 Radixite = GetDelivered(Test, ERTSResourceType::Resource_Radixite) - Test.DeliveredBaselineRadixite;
		const int32 Metal = GetDelivered(Test, ERTSResourceType::Resource_Metal) - Test.DeliveredBaselineMetal;
		Test.bForcedDone = Radixite >= Test.ForcedRadixite && Metal >= Test.ForcedMetal && Test.CraneReleases >= 2;
		bAllDone &= Test.bForcedDone;
	}
	if (not bAllDone && GetPhaseSeconds() < ForcedTimeoutSeconds)
	{
		return;
	}
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		const int32 Radixite = GetDelivered(Test, ERTSResourceType::Resource_Radixite) - Test.DeliveredBaselineRadixite;
		const int32 Metal = GetDelivered(Test, ERTSResourceType::Resource_Metal) - Test.DeliveredBaselineMetal;
		// Natural deliveries may add on top while the forced trips run; never less than the forced cargo.
		Check(Radixite >= Test.ForcedRadixite, TEXT("forced_radixite_delivered"),
		      FString::Printf(TEXT("%s delivered=%d forced=%d"), *Test.Design, Radixite, Test.ForcedRadixite));
		Check(Metal >= Test.ForcedMetal, TEXT("forced_metal_delivered"),
		      FString::Printf(TEXT("%s delivered=%d forced=%d"), *Test.Design, Metal, Test.ForcedMetal));
		Check(Test.CraneReleases >= 2, TEXT("crane_made_trips"),
		      FString::Printf(TEXT("%s releases=%d"), *Test.Design, Test.CraneReleases));
		Check(Test.MaxJawOffPick2DCm <= MaxJawOffPick2DCm, TEXT("crane_grabs_in_hopper"),
		      FString::Printf(TEXT("%s max_off_cm=%.0f"), *Test.Design, Test.MaxJawOffPick2DCm));
		Test.DeliveredBaselineRadixite = GetDelivered(Test, ERTSResourceType::Resource_Radixite);
		Test.DeliveredBaselineMetal = GetDelivered(Test, ERTSResourceType::Resource_Metal);
	}
	EnterPhase(ESovietRefineryTestPhase::NaturalHarvest);
}

void USovietRefineryTestSubsystem::TickNaturalHarvest()
{
	using namespace SovietRefineryTestPrivate;
	// A harvester unloads at the nearest refinery of its side, which need not be the one that trained it: every
	// refinery must receive something on its own, and the side as a whole must deliver both resources.
	int32 SideRadixite = 0;
	int32 SideMetal = 0;
	bool bEveryRefineryGotSome = true;
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		const int32 Radixite = GetDelivered(Test, ERTSResourceType::Resource_Radixite) - Test.DeliveredBaselineRadixite;
		const int32 Metal = GetDelivered(Test, ERTSResourceType::Resource_Metal) - Test.DeliveredBaselineMetal;
		SideRadixite += Radixite;
		SideMetal += Metal;
		if (not Test.bNaturalDone && Radixite + Metal > 0)
		{
			Test.bNaturalDone = true;
			UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST natural delivery design=%s t=%.0f r=%d m=%d"),
			       *Test.Design, GetPhaseSeconds(), Radixite, Metal);
		}
		bEveryRefineryGotSome &= Test.bNaturalDone;
	}
	const bool bDone = bEveryRefineryGotSome && SideRadixite > 0 && SideMetal > 0;
	if (not bDone && GetPhaseSeconds() < NaturalTimeoutSeconds)
	{
		return;
	}
	Check(SideRadixite > 0, TEXT("natural_radixite_delivered"),
	      FString::Printf(TEXT("side_total=%d t=%.0f"), SideRadixite, GetPhaseSeconds()));
	Check(SideMetal > 0, TEXT("natural_metal_delivered"),
	      FString::Printf(TEXT("side_total=%d t=%.0f"), SideMetal, GetPhaseSeconds()));
	for (FSovietRefineryUnderTest& Test : M_Refineries)
	{
		Check(Test.bNaturalDone, TEXT("refinery_receives_natural_deliveries"), Test.Design);
		USovietRefineryComponent* Refinery = Test.Refinery.Get();
		if (IsValid(Refinery))
		{
			Refinery->MapTest_AddStock(ERTSResourceType::Resource_Radixite, 30);
			Refinery->MapTest_AddStock(ERTSResourceType::Resource_Metal, 30);
			Refinery->MapTest_RunConversionNow();
			Check(Refinery->GetTotalConverted(ERTSResourceType::Resource_Radixite) > 0
			      && Refinery->GetTotalConverted(ERTSResourceType::Resource_Metal) > 0, TEXT("stock_converts"),
			      FString::Printf(TEXT("%s converted_r=%d converted_m=%d training=%d building=%d"), *Test.Design,
			                      Refinery->GetTotalConverted(ERTSResourceType::Resource_Radixite),
			                      Refinery->GetTotalConverted(ERTSResourceType::Resource_Metal),
			                      Refinery->GetTotalTrainingPointsGranted(), Refinery->GetTotalBuildingPointsGranted()));
		}
		CaptureCrane(Test, TEXT("end"));
	}
	const int32 PlayerRadixite = GetPlayerResource(ERTSResourceType::Resource_Radixite);
	const int32 PlayerMetal = GetPlayerResource(ERTSResourceType::Resource_Metal);
	Check(PlayerRadixite == M_PlayerRadixiteBefore && PlayerMetal == M_PlayerMetalBefore,
	      TEXT("player_resources_untouched"),
	      FString::Printf(TEXT("radixite %d->%d metal %d->%d"), M_PlayerRadixiteBefore, PlayerRadixite,
	                      M_PlayerMetalBefore, PlayerMetal));
	Finish();
}

void USovietRefineryTestSubsystem::TrackCrane(FSovietRefineryUnderTest& Test)
{
	ASkeletalCraneUnloadStation* Station = Test.Station.Get();
	if (not IsValid(Station))
	{
		return;
	}
	const EUnloadTrolleyPhase Phase = Station->GetTrolleyPhase(0);
	const uint8 PhaseValue = static_cast<uint8>(Phase);
	if (PhaseValue == Test.LastPhase)
	{
		return;
	}
	UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST crane design=%s phase=%d t=%.1f stock_r=%d stock_m=%d"),
	       *Test.Design, PhaseValue, GetPhaseSeconds(),
	       Test.Refinery.IsValid() ? Test.Refinery->GetStock(ERTSResourceType::Resource_Radixite) : -1,
	       Test.Refinery.IsValid() ? Test.Refinery->GetStock(ERTSResourceType::Resource_Metal) : -1);
	if (static_cast<EUnloadTrolleyPhase>(Test.LastPhase) == EUnloadTrolleyPhase::Releasing)
	{
		++Test.CraneReleases;
	}
	if (Phase == EUnloadTrolleyPhase::Grabbing)
	{
		// The grab is at its lowest: measure it against the docked hopper.
		const UHarvester* Docked = Station->GetBayHarvester(0);
		if (IsValid(Docked))
		{
			const float Off = FVector::Dist2D(Station->GetCraneGrabWorldLocation(), Docked->GetCargoWorldLocation());
			Test.MaxJawOffPick2DCm = FMath::Max(Test.MaxJawOffPick2DCm, Off);
		}
	}
	if (Phase == EUnloadTrolleyPhase::Grabbing && not Test.bCapturedLowering)
	{
		Test.bCapturedLowering = true;
		CaptureCrane(Test, TEXT("grab_in_hopper"));
	}
	if (Phase == EUnloadTrolleyPhase::LoweringToDrop && not Test.bCapturedCarry)
	{
		Test.bCapturedCarry = true;
		CaptureCrane(Test, TEXT("over_drop"));
	}
	if (Phase == EUnloadTrolleyPhase::HoistingFromDrop && not Test.bCapturedRelease)
	{
		Test.bCapturedRelease = true;
		CaptureCrane(Test, TEXT("released"));
	}
	Test.LastPhase = PhaseValue;
}

void USovietRefineryTestSubsystem::TrackHarvesters(FSovietRefineryUnderTest& Test)
{
	const USovietRefineryComponent* Refinery = Test.Refinery.Get();
	if (not IsValid(Refinery))
	{
		return;
	}
	for (AActor* Actor : Refinery->GetHarvesters())
	{
		const UHarvester* Harvester = Actor->FindComponentByClass<UHarvester>();
		ICommands* Commands = Cast<ICommands>(Actor);
		if (not IsValid(Harvester) || Commands == nullptr)
		{
			continue;
		}
		const EHarvesterAIAction Status = Harvester->MapTest_GetHarvestStatus();
		uint8& Last = Test.HarvesterStatus.FindOrAdd(Actor->GetName(), MAX_uint8);
		if (static_cast<uint8>(Status) == Last)
		{
			continue;
		}
		Last = static_cast<uint8>(Status);
		int32 Cargo = 0;
		ERTSResourceType CargoType = ERTSResourceType::Resource_None;
		(void)Harvester->GetMaxCapacityForTargetResource(Cargo, CargoType);
		const UResourceComponent* Target = Harvester->GetTargetResource().Get();
		UE_LOG(LogSovietRefineryTest, Display,
		       TEXT("RTS_SOVREF_TEST harvester design=%s unit=%s status=%s command=%d cargo=%d %s target=%s auto=%d t=%.1f"),
		       *Test.Design, *Actor->GetName(), *GetStringHarvesterAIStatus(Status),
		       static_cast<int32>(Commands->GetActiveCommandID()), Cargo, *Global_GetResourceTypeAsString(CargoType),
		       IsValid(Target) ? *GetNameSafe(Target->GetOwner()) : TEXT("None"),
		       Harvester->GetIsHarvestingAutonomously() ? 1 : 0, GetPhaseSeconds());
	}
}

int32 USovietRefineryTestSubsystem::GetDelivered(const FSovietRefineryUnderTest& Test,
                                                 const ERTSResourceType Resource) const
{
	const USovietRefineryComponent* Refinery = Test.Refinery.Get();
	return IsValid(Refinery) ? Refinery->GetStock(Resource) + Refinery->GetTotalConverted(Resource) : 0;
}

void USovietRefineryTestSubsystem::CaptureCrane(const FSovietRefineryUnderTest& Test, const FString& Label) const
{
	const ASkeletalCraneUnloadStation* Station = Test.Station.Get();
	if (not M_CaptureRig.GetIsEnabled() || not IsValid(Station))
	{
		return;
	}
	// The camera is the human's: the AI's refinery and harvesters sit in its fog. Pictures only: take them out of it.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		UFowComp* Fow = IsValid(Actor) ? Actor->FindComponentByClass<UFowComp>() : nullptr;
		if (IsValid(Fow) && FRTSPlayers::GetOwningPlayerOfActor(Actor) == SovietRefineryTestPrivate::EnemyPlayer
			&& FVector::Dist2D(Actor->GetActorLocation(), Station->GetActorLocation()) < 9000.f)
		{
			Fow->StopFowParticipation();
			Actor->SetActorHiddenInGame(false);
		}
	}
	FTransform Frame = Station->GetActorTransform();
	Frame.SetScale3D(FVector::OneVector);
	const FVector Dock = Station->GetDockPickPointLocal();
	const FVector Crane = Station->GetCraneLocationLocal();
	const FVector Drop = Station->GetDropPointLocal(ERTSResourceType::Resource_Radixite);
	// Building origin, crane, dock and drop: the whole refinery with the docked Breaker.
	const FVector Centre = (Dock + Crane + Drop + FVector::ZeroVector) / 4.f + FVector(0.f, 0.f, 300.f);
	const TArray<FB4HowitzerCameraView> Views = {
		{TEXT("rts"), Centre + FVector(-2500.f, 6500.f, 5200.f), Centre, 55.f},
		{TEXT("dock"), Dock + FVector(-1800.f, 1800.f, 1300.f), FVector(Dock.X, Dock.Y, 300.f), 55.f},
		{TEXT("top"), Centre + FVector(0.f, 0.f, 7500.f), Centre + FVector(1.f, 0.f, 0.f), 70.f}
	};
	M_CaptureRig.CaptureViews(*GetWorld(), Frame, Views, Test.Design + TEXT("_") + Label);
}

int32 USovietRefineryTestSubsystem::GetPlayerResource(const ERTSResourceType Resource) const
{
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	const UPlayerResourceManager* Manager = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	return IsValid(Manager) ? Manager->GetResourceAmount(Resource) : -1;
}

void USovietRefineryTestSubsystem::StartGameAndUnpause() const
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
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

void USovietRefineryTestSubsystem::EnterPhase(const ESovietRefineryTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST phase=%d"), static_cast<int32>(NewPhase));
}

float USovietRefineryTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void USovietRefineryTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void USovietRefineryTestSubsystem::Finish()
{
	M_Phase = ESovietRefineryTestPhase::Inactive;
	UE_LOG(LogSovietRefineryTest, Display, TEXT("RTS_SOVREF_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	bM_IsExitPending = true;
	M_ExitFramesLeft = SovietRefineryTestPrivate::FramesToWriteLastCapture;
}

void USovietRefineryTestSubsystem::TickExitPending()
{
	if (--M_ExitFramesLeft > 0)
	{
		return;
	}
	bM_IsExitPending = false;
	FPlatformMisc::RequestExit(false, TEXT("SovietRefineryValidate"));
}

#else

bool USovietRefineryTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void USovietRefineryTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId USovietRefineryTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovietRefineryTestSubsystem, STATGROUP_Tickables);
}

#endif
