// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftBaseTakeOffValidate: an aircraft parked in an airbase bay takes off, flies out, returns to its bay and
// takes off again (UAircraftTestSubsystem). Guards the vertical take-off against the terrain following of airborne
// aircraft: the take-off must still end exactly at the take-off height, and the hand-over must not jolt the height.

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftBaseTakeOffTest, Log, All);

namespace AircraftBaseTakeOffTestPrivate
{
	const TCHAR* const AircraftClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Ger_Aircraft_BF109.Ger_Aircraft_BF109_C");
	const TCHAR* const AirbaseTruckClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
		"Ger_Airbase_Truck.Ger_Airbase_Truck_C");
	constexpr uint8 HumanPlayer = 1;
	constexpr float AirbaseSpawnLift = 100.f;
	// Headless runs never draw the truck, so its unpack montage is fast-forwarded after this long.
	constexpr float AirbaseFastForwardSeconds = 5.f;
	constexpr float AirbaseTimeoutSeconds = 30.f;
	constexpr float SettleSeconds = 2.f;
	constexpr float TakeOffTimeoutSeconds = 60.f;
	constexpr float ReturnTimeoutSeconds = 120.f;
	// The move order away from the airbase.
	constexpr float MoveOutDistance = 9000.f;
	constexpr float FlyOutSeconds = 8.f;
	// The take-off ends by putting the aircraft on its apex.
	constexpr float ApexTolerance = 5.f;
	// Over the flat ground of a unit-test map terrain following must not move the aircraft at all; with relief it
	// may, but never faster than its capped vertical speed.
	constexpr float FlatGroundHeightTolerance = 60.f;
	constexpr float VerticalSpeedTolerance = 1.25f;
	// Where the aircraft must stand again after landing.
	constexpr float LandedXYTolerance = 50.f;
	constexpr float LandedZTolerance = 15.f;
	constexpr float StatusIntervalSeconds = 2.f;
	constexpr float LandscapeTraceHalfHeight = 200000.f;
}

void UAircraftTestSubsystem::StartAircraftBaseTakeOff()
{
	using namespace AircraftBaseTakeOffTestPrivate;
	FAircraftBaseTakeOffUnderTest& Entry = M_BaseTakeOff;
	EnterPhase(EAircraftTestPhase::AircraftBaseTakeOffRunning);
	Entry.Airbase = Cast<ANomadicVehicle>(SpawnUnitForPlayer(
		AirbaseTruckClassPath, FTransform(M_Anchor + FVector(0.f, 0.f, AirbaseSpawnLift)), HumanPlayer));
	Check(Entry.Airbase.IsValid(), TEXT("base_takeoff_airbase_spawned"), AirbaseTruckClassPath);
	if (not Entry.Airbase.IsValid())
	{
		Finish();
		return;
	}
	ANomadicVehicle* Airbase = Entry.Airbase.Get();
	const ECommandQueueError Error = Airbase->CreateBuildingAtLocation(Airbase->GetActorLocation(),
	                                                                   Airbase->GetActorRotation(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("base_takeoff_airbase_unpack_ordered"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterBaseTakeOffStep(Entry, EAircraftBaseTakeOffStep::WaitingForAirbase);
}

void UAircraftTestSubsystem::TickAircraftBaseTakeOff()
{
	FAircraftBaseTakeOffUnderTest& Entry = M_BaseTakeOff;
	if (Entry.Step != EAircraftBaseTakeOffStep::WaitingForAirbase && not Entry.Aircraft.IsValid())
	{
		Check(false, TEXT("base_takeoff_aircraft_alive"), TEXT("destroyed during the test"));
		Finish();
		return;
	}
	switch (Entry.Step)
	{
	case EAircraftBaseTakeOffStep::WaitingForAirbase:
		TickBaseTakeOffWaitingForAirbase(Entry);
		return;
	case EAircraftBaseTakeOffStep::Settling:
		TickBaseTakeOffSettling(Entry);
		return;
	case EAircraftBaseTakeOffStep::TakingOff:
		TickBaseTakeOffTakingOff(Entry, TEXT("base_takeoff_first"), EAircraftBaseTakeOffStep::FlyingOut);
		return;
	case EAircraftBaseTakeOffStep::FlyingOut:
		TickBaseTakeOffFlyingOut(Entry);
		return;
	case EAircraftBaseTakeOffStep::ReturningToBase:
		TickBaseTakeOffReturning(Entry);
		return;
	case EAircraftBaseTakeOffStep::SecondTakeOff:
		TickBaseTakeOffTakingOff(Entry, TEXT("base_takeoff_second"), EAircraftBaseTakeOffStep::Done);
		return;
	case EAircraftBaseTakeOffStep::Done:
		Finish();
		return;
	}
}

void UAircraftTestSubsystem::TickBaseTakeOffWaitingForAirbase(FAircraftBaseTakeOffUnderTest& Entry)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	ANomadicVehicle* Airbase = Entry.Airbase.Get();
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (not IsValid(Airbase) || Waited > AirbaseTimeoutSeconds)
	{
		Check(false, TEXT("base_takeoff_airbase_unpacked"), FString::Printf(TEXT("after=%.1fs"), Waited));
		Finish();
		return;
	}
	if (Waited >= AirbaseFastForwardSeconds)
	{
		(void)Airbase->ShippingTest_ForceFinishConversionToBuilding();
	}
	UAircraftOwnerComp* Bays = Airbase->GetAircraftOwnerComp();
	if (Airbase->GetNomadicStatus() != ENomadStatus::Building || not IsValid(Bays)
		|| not Bays->IsAirbaseUnPackedAsBuilding())
	{
		return;
	}
	Check(true, TEXT("base_takeoff_airbase_unpacked"), FString::Printf(TEXT("after=%.1fs"), Waited));
	// Enemy bombers on the test map can destroy the airbase, which takes a parked aircraft with it: keep it standing.
	constexpr float AirbaseTestHealth = 1000000.f;
	if (UHealthComponent* AirbaseHealth = Entry.Airbase.IsValid()
		                                       ? Entry.Airbase->FindComponentByClass<UHealthComponent>()
		                                       : nullptr)
	{
		AirbaseHealth->SetMaxHealth(AirbaseTestHealth);
		AirbaseHealth->SetCurrentHealth(AirbaseTestHealth);
	}
	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(
		AircraftClassPath, FTransform(Airbase->GetActorRotation(), Airbase->GetActorLocation()), HumanPlayer));
	Check(Entry.Aircraft.IsValid(), TEXT("base_takeoff_aircraft_spawned"), AircraftClassPath);
	if (not Entry.Aircraft.IsValid())
	{
		Finish();
		return;
	}
	// The way a trained aircraft appears: assigned to a free bay and put down on its landing socket.
	const bool bParked = Bays->OnAircraftTrained(Entry.Aircraft.Get(), EAircraftSubtype::Aircraft_Bf109);
	const int32 BayIndex = Bays->GetAssignedSocketIndex(Entry.Aircraft.Get());
	const bool bHasBay = bParked && Bays->GetSocketWorldLocation(BayIndex, Entry.BayLocation);
	Check(bHasBay, TEXT("base_takeoff_parked_in_a_bay"), FString::Printf(TEXT("bay=%d at=%s"), BayIndex,
	                                                                    *Entry.BayLocation.ToCompactString()));
	if (not bHasBay)
	{
		Finish();
		return;
	}
	EnterBaseTakeOffStep(Entry, EAircraftBaseTakeOffStep::Settling);
}

void UAircraftTestSubsystem::TickBaseTakeOffSettling(FAircraftBaseTakeOffUnderTest& Entry)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	if (GetNow() - Entry.StepStartSeconds < SettleSeconds)
	{
		return;
	}
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	Check(Aircraft.GetLandedState() == EAircraftLandingState::Landed, TEXT("base_takeoff_starts_landed"),
	      FString::Printf(TEXT("state=%d"), static_cast<int32>(Aircraft.GetLandedState())));
	Entry.ParkedZ = Aircraft.GetActorLocation().Z;
	if (not OrderBaseTakeOffMoveOut(Entry, TEXT("base_takeoff_first")))
	{
		Finish();
		return;
	}
	EnterBaseTakeOffStep(Entry, EAircraftBaseTakeOffStep::TakingOff);
}

bool UAircraftTestSubsystem::OrderBaseTakeOffMoveOut(FAircraftBaseTakeOffUnderTest& Entry, const FString& Label)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	// Toward the map centre, so the whole flight stays over landscape that the clearance check can measure.
	FVector MoveOutDirection = (FVector::ZeroVector - Entry.BayLocation).GetSafeNormal2D();
	if (MoveOutDirection.IsNearlyZero())
	{
		MoveOutDirection = Aircraft.GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector MoveOutLocation = Entry.BayLocation + MoveOutDirection * MoveOutDistance;
	const ECommandQueueError Error = Aircraft.MoveToLocation(MoveOutLocation, true, FRotator::ZeroRotator);
	Check(Error == ECommandQueueError::NoError, Label + TEXT("_move_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	Entry.bSawVerticalTakeOff = false;
	return Error == ECommandQueueError::NoError;
}

void UAircraftTestSubsystem::TickBaseTakeOffTakingOff(FAircraftBaseTakeOffUnderTest& Entry, const FString& Label,
                                                      const EAircraftBaseTakeOffStep NextStep)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const EAircraftLandingState State = Aircraft.GetLandedState();
	Entry.bSawVerticalTakeOff |= State == EAircraftLandingState::VerticalTakeOff;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (State != EAircraftLandingState::Airborne)
	{
		if (Waited < TakeOffTimeoutSeconds)
		{
			return;
		}
		Check(false, Label + TEXT("_airborne"), FString::Printf(TEXT("state=%d after=%.1fs"),
		                                                        static_cast<int32>(State), Waited));
		Finish();
		return;
	}
	const float ApexZ = Entry.ParkedZ + Aircraft.GetTakeOffHeight();
	const float AirborneZ = Aircraft.GetActorLocation().Z;
	Check(true, Label + TEXT("_airborne"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Check(Entry.bSawVerticalTakeOff, Label + TEXT("_vertical_take_off_state"));
	Check(FMath::IsNearlyEqual(AirborneZ, ApexZ, ApexTolerance), Label + TEXT("_reached_take_off_height"),
	      FString::Printf(TEXT("z=%.1f apex=%.1f parked=%.1f take_off_height=%.1f"), AirborneZ, ApexZ, Entry.ParkedZ,
	                      Aircraft.GetTakeOffHeight()));
	Entry.LastZ = AirborneZ;
	Entry.LargestZStepPerSecond = 0.f;
	Entry.HeightAboveGroundAtAirborne = GetHeightAboveLandscape(Aircraft);
	EnterBaseTakeOffStep(Entry, NextStep);
}

void UAircraftTestSubsystem::TickBaseTakeOffFlyingOut(FAircraftBaseTakeOffUnderTest& Entry)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	const UWorld* World = GetWorld();
	const float DeltaSeconds = IsValid(World) ? World->GetDeltaSeconds() : 0.f;
	const float Z = Aircraft.GetActorLocation().Z;
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Entry.LargestZStepPerSecond = FMath::Max(Entry.LargestZStepPerSecond, FMath::Abs(Z - Entry.LastZ) / DeltaSeconds);
	}
	Entry.LastZ = Z;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (GetNow() >= M_NextStatusSeconds)
	{
		M_NextStatusSeconds = GetNow() + StatusIntervalSeconds;
		const FAircraftTerrainFollow& TerrainFollow = Aircraft.GetTerrainFollow();
		UE_LOG(LogRTSAircraftBaseTakeOffTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST BASE_TAKEOFF flying t=%.1f z=%.1f above_ground=%.1f state=%d clearance=%.1f "
			       "has_clearance=%d vertical_speed=%.1f"),
		       Waited, Z, GetHeightAboveLandscape(Aircraft), static_cast<int32>(Aircraft.GetLandedState()),
		       TerrainFollow.GetClearance(), TerrainFollow.GetHasClearance() ? 1 : 0,
		       TerrainFollow.GetVerticalSpeed());
	}
	if (Waited < FlyOutSeconds)
	{
		return;
	}
	const FAircraftTerrainFollowSettings Settings;
	Check(Aircraft.GetLandedState() == EAircraftLandingState::Airborne, TEXT("base_takeoff_stays_airborne"));
	Check(Aircraft.GetTerrainFollow().GetHasClearance(), TEXT("base_takeoff_clearance_captured"),
	      FString::Printf(TEXT("clearance=%.1f"), Aircraft.GetTerrainFollow().GetClearance()));
	Check(Entry.LargestZStepPerSecond <= Settings.MaxVerticalSpeed * VerticalSpeedTolerance,
	      TEXT("base_takeoff_no_height_jolt_after_take_off"),
	      FString::Printf(TEXT("largest=%.1f cm/s cap=%.1f"), Entry.LargestZStepPerSecond, Settings.MaxVerticalSpeed));
	const float HeightAboveGround = GetHeightAboveLandscape(Aircraft);
	// A take-off height below the minimum cruise clearance climbs to it once airborne; otherwise it is kept.
	const float WantedHeightAboveGround = FMath::Max(Entry.HeightAboveGroundAtAirborne, Settings.MinCruiseClearance);
	Check(FMath::Abs(HeightAboveGround - WantedHeightAboveGround) <= FlatGroundHeightTolerance,
	      TEXT("base_takeoff_keeps_clearance_above_ground"),
	      FString::Printf(TEXT("above_ground now=%.1f at_airborne=%.1f wanted=%.1f"), HeightAboveGround,
	                      Entry.HeightAboveGroundAtAirborne, WantedHeightAboveGround));
	const ECommandQueueError Error = Aircraft.ReturnToBase(true);
	Check(Error == ECommandQueueError::NoError, TEXT("base_takeoff_return_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterBaseTakeOffStep(Entry, EAircraftBaseTakeOffStep::ReturningToBase);
}

void UAircraftTestSubsystem::TickBaseTakeOffReturning(FAircraftBaseTakeOffUnderTest& Entry)
{
	using namespace AircraftBaseTakeOffTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (Aircraft.GetLandedState() != EAircraftLandingState::Landed)
	{
		if (Waited < ReturnTimeoutSeconds)
		{
			return;
		}
		Check(false, TEXT("base_takeoff_landed_back"), FString::Printf(TEXT("state=%d after=%.1fs"),
		                                                               static_cast<int32>(Aircraft.GetLandedState()),
		                                                               Waited));
		Finish();
		return;
	}
	const FVector Location = Aircraft.GetActorLocation();
	Check(true, TEXT("base_takeoff_landed_back"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Check(FVector::Dist2D(Location, Entry.BayLocation) <= LandedXYTolerance, TEXT("base_takeoff_landed_in_its_bay"),
	      FString::Printf(TEXT("at=%s bay=%s"), *Location.ToCompactString(), *Entry.BayLocation.ToCompactString()));
	Check(FMath::IsNearlyEqual(Location.Z, Entry.ParkedZ, LandedZTolerance), TEXT("base_takeoff_landed_on_bay_floor"),
	      FString::Printf(TEXT("z=%.1f parked=%.1f"), Location.Z, Entry.ParkedZ));
	if (not OrderBaseTakeOffMoveOut(Entry, TEXT("base_takeoff_second")))
	{
		Finish();
		return;
	}
	EnterBaseTakeOffStep(Entry, EAircraftBaseTakeOffStep::SecondTakeOff);
}

float UAircraftTestSubsystem::GetHeightAboveLandscape(const AAircraftMaster& Aircraft) const
{
	using namespace AircraftBaseTakeOffTestPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return -1.f;
	}
	const FVector Location = Aircraft.GetActorLocation();
	const FVector Start(Location.X, Location.Y, Location.Z + LandscapeTraceHalfHeight);
	const FVector End(Location.X, Location.Y, Location.Z - LandscapeTraceHalfHeight);
	FCollisionQueryParams Params(FName(TEXT("BaseTakeOffTestGround")), false, &Aircraft);
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, Start, End, COLLISION_TRACE_LANDSCAPE, Params))
	{
		return -1.f;
	}
	return Location.Z - GroundHit.ImpactPoint.Z;
}

void UAircraftTestSubsystem::EnterBaseTakeOffStep(FAircraftBaseTakeOffUnderTest& Entry,
                                                  const EAircraftBaseTakeOffStep NewStep)
{
	Entry.Step = NewStep;
	Entry.StepStartSeconds = GetNow();
	UE_LOG(LogRTSAircraftBaseTakeOffTest, Display, TEXT("RTS_AIRCRAFT_TEST BASE_TAKEOFF_STEP %d at=%.1f"),
	       static_cast<int32>(NewStep), Entry.StepStartSeconds);
}

#endif
