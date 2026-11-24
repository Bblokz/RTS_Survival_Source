// Copyright (C) Bas Blokzijl - All rights reserved.

// Edge layout for the chosen start (far-side airlift origin, rim slide) and the heavy airlift of
// USurvivalWaveDirectorComponent. Kept apart from the ground-wave scheduling in SurvivalWaveDirectorComponent.cpp.

#include "SurvivalWaveDirectorComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/Formation/FormationData.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalEdgeSpawnZone.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveMarkerTracker.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AircraftDropTypes.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalWaveAirliftConstants
{
	constexpr int32 EnemyPlayerIndex = 2;
	constexpr int32 DropSearchAnglesPerRing = 8;
	constexpr float DropProjectionScale = 3.f;
	// The transports leave past their spawn point, out over the rim.
	constexpr float RetreatBeyondSpawnDistance = 5000.f;
	// A slid zone never comes closer to the start than this multiple of the minimum edge distance.
	constexpr float SlideMinDistanceToStartMultiplier = 1.5f;
}

// ---------------------------------------------------------------- Edge layout

void USurvivalWaveDirectorComponent::ConfigureEdgesForStart(const FVector& StartLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings or not GetIsValidDirector())
	{
		return;
	}
	TArray<ASurvivalEdgeSpawnZone*> Zones;
	FBox2D ZoneBounds(ForceInit);
	for (const TWeakObjectPtr<ASurvivalEdgeSpawnZone>& EachWeakZone : M_Director->GetEdgeSpawnZones())
	{
		ASurvivalEdgeSpawnZone* Zone = EachWeakZone.Get();
		if (not IsValid(Zone) or not Zone->GetIsEnabled())
		{
			continue;
		}
		Zones.Add(Zone);
		ZoneBounds += FVector2D(Zone->GetPlacedSpawnCenter());
	}
	if (Zones.IsEmpty())
	{
		return;
	}
	M_MapCentre = FVector(ZoneBounds.GetCenter(), StartLocation.Z);

	M_AirliftOriginZone = WaveSettings->OppositeAirlift.bEnabled ? FindOppositeZone(Zones, StartLocation) : nullptr;
	if (M_AirliftOriginZone.IsValid())
	{
		RequestAirliftAircraftClass();
	}
	int32 SlidZones = 0;
	bool bSlideEnabled = WaveSettings->EdgeSlide.bEnabled;
#if RTS_WITH_MAP_TESTS
	// A/B test runs: -SurvivalNoEdgeSlide keeps every zone where it was placed.
	bSlideEnabled = bSlideEnabled and not FParse::Param(FCommandLine::Get(), TEXT("SurvivalNoEdgeSlide"));
#endif
	for (ASurvivalEdgeSpawnZone* EachZone : Zones)
	{
		if (EachZone == M_AirliftOriginZone.Get() or not bSlideEnabled)
		{
			continue;
		}
		SlideZoneTowardStart(*EachZone, StartLocation);
		SlidZones += EachZone->GetHasRuntimeSpawnCenter() ? 1 : 0;
		UE_LOG(LogTemp, Log, TEXT("Survival edge %s: %s -> %s (%.0f -> %.0f from the start)."), *EachZone->GetName(),
		       *EachZone->GetPlacedSpawnCenter().ToCompactString(), *EachZone->GetSpawnCenter().ToCompactString(),
		       FVector::Dist2D(EachZone->GetPlacedSpawnCenter(), StartLocation),
		       FVector::Dist2D(EachZone->GetSpawnCenter(), StartLocation));
	}
	UE_LOG(LogTemp, Log, TEXT("Survival edges for start %s: airlift origin %s, %d of %d zones slid toward the start, "
		       "map centre %s."),
	       *StartLocation.ToCompactString(),
	       M_AirliftOriginZone.IsValid() ? *M_AirliftOriginZone->GetName() : TEXT("none"), SlidZones,
	       Zones.Num() - (M_AirliftOriginZone.IsValid() ? 1 : 0), *M_MapCentre.ToCompactString());
}

ASurvivalEdgeSpawnZone* USurvivalWaveDirectorComponent::FindOppositeZone(const TArray<ASurvivalEdgeSpawnZone*>& Zones,
                                                                         const FVector& StartLocation) const
{
	if (Zones.Num() < 2)
	{
		return nullptr;
	}
	const FVector StartDirection = (StartLocation - M_MapCentre).GetSafeNormal2D();
	ASurvivalEdgeSpawnZone* OppositeZone = nullptr;
	float LowestDot = TNumericLimits<float>::Max();
	for (ASurvivalEdgeSpawnZone* EachZone : Zones)
	{
		const FVector ZoneDirection = (EachZone->GetPlacedSpawnCenter() - M_MapCentre).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(StartDirection, ZoneDirection);
		if (Dot < LowestDot)
		{
			LowestDot = Dot;
			OppositeZone = EachZone;
		}
	}
	return OppositeZone;
}

void USurvivalWaveDirectorComponent::SlideZoneTowardStart(ASurvivalEdgeSpawnZone& Zone,
                                                          const FVector& StartLocation) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return;
	}
	const FSurvivalEdgeSlideSettings& Slide = WaveSettings->EdgeSlide;
	// A zone on an X rim (inward mostly along X) slides along Y, and the other way round.
	const FVector Inward = Zone.GetInwardDirection();
	const bool bSlideAlongY = FMath::Abs(Inward.X) >= FMath::Abs(Inward.Y);
	const FVector Placed = Zone.GetPlacedSpawnCenter();
	const float PlacedAlong = bSlideAlongY ? Placed.Y : Placed.X;
	const float StartAlong = bSlideAlongY ? StartLocation.Y : StartLocation.X;
	const int32 Attempts = FMath::Max(1, Slide.SlideAttempts);
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const float Fraction = Slide.SlideFraction * (1.f - static_cast<float>(Attempt) / Attempts);
		const float NewAlong = FMath::Lerp(PlacedAlong, StartAlong, Fraction);
		FVector Candidate = Placed;
		if (bSlideAlongY)
		{
			Candidate.Y = NewAlong;
		}
		else
		{
			Candidate.X = NewAlong;
		}
		if (not GetIsSlidCenterAllowed(Candidate, StartLocation))
		{
			continue;
		}
		if (Zone.TrySetRuntimeSpawnCenter(Candidate))
		{
			return;
		}
	}
}

bool USurvivalWaveDirectorComponent::GetIsSlidCenterAllowed(const FVector& Candidate,
                                                            const FVector& StartLocation) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings or not M_Director.IsValid())
	{
		return false;
	}
	using SurvivalWaveAirliftConstants::SlideMinDistanceToStartMultiplier;
	float MinDistanceToStart = WaveSettings->MinEdgeDistanceFromBase * SlideMinDistanceToStartMultiplier;
	// Never inside the player's own start content (resource fields, wrecks, cacti): waves enter beyond it.
	const ASurvivalStartLocation* ChosenStart = M_Director->GetChosenStartLocation();
	const USurvivalModeSettings* ModeSettings = M_Director->GetSettings();
	if (IsValid(ChosenStart) and IsValid(ModeSettings))
	{
		MinDistanceToStart = FMath::Max(MinDistanceToStart,
		                                ChosenStart->GetContentOuterRadius(ModeSettings->StartContent)
		                                + WaveSettings->EdgeSlide.OwnStartContentClearance);
	}
	if (FVector::Dist2D(Candidate, StartLocation) < MinDistanceToStart)
	{
		return false;
	}
	// The other starts hold Soviet bases: waves must not spawn inside them.
	for (const TWeakObjectPtr<ASurvivalStartLocation>& EachStart : M_Director->GetStartLocations())
	{
		const ASurvivalStartLocation* OtherStart = EachStart.Get();
		if (not IsValid(OtherStart) or OtherStart == M_Director->GetChosenStartLocation())
		{
			continue;
		}
		const float Clearance = OtherStart->GetBuildAreaRadius() + WaveSettings->EdgeSlide.OtherStartClearance;
		if (FVector::Dist2D(Candidate, OtherStart->GetDeployCenter()) < Clearance)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------- Airlift: setup

void USurvivalWaveDirectorComponent::RequestAirliftAircraftClass()
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings or IsValid(M_AirliftAircraftClass) or M_AirliftAircraftLoadHandle.IsValid())
	{
		return;
	}
	const TSoftClassPtr<AAircraftMaster>& AircraftClass = WaveSettings->OppositeAirlift.AircraftClass;
	if (AircraftClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("Survival heavy airlift has no aircraft class; the far side stays quiet."
			"\nAt USurvivalWaveDirectorComponent::RequestAirliftAircraftClass");
		return;
	}
	M_AirliftAircraftLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AircraftClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &USurvivalWaveDirectorComponent::OnAirliftAircraftClassLoaded));
}

void USurvivalWaveDirectorComponent::OnAirliftAircraftClassLoaded()
{
	M_AirliftAircraftLoadHandle.Reset();
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return;
	}
	M_AirliftAircraftClass = WaveSettings->OppositeAirlift.AircraftClass.Get();
	if (not IsValid(M_AirliftAircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival heavy airlift aircraft class failed to load: ")
			+ WaveSettings->OppositeAirlift.AircraftClass.ToString());
	}
}

bool USurvivalWaveDirectorComponent::GetIsAirliftReady() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	return WaveSettings and WaveSettings->OppositeAirlift.bEnabled and M_AirliftOriginZone.IsValid()
		and IsValid(M_AirliftAircraftClass);
}

// ---------------------------------------------------------------- Airlift: launch

void USurvivalWaveDirectorComponent::TryLaunchAirlift(FSurvivalWavePlan& InOutPlan)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not GetIsAirliftReady() or not WaveSettings)
	{
		return;
	}
	const FSurvivalHeavyAirliftSettings& Airlift = WaveSettings->OppositeAirlift;
	const double Now = GetWorldTime();
	if (Now - M_LastAirliftTime < Airlift.MinSecondsBetweenAirlifts)
	{
		return;
	}
	TArray<FSurvivalPlannedWaveUnit> Units;
	PickAirliftUnits(InOutPlan.Minute, GetAirliftTankCount(InOutPlan.Minute), Units);
	if (Units.IsEmpty())
	{
		return;
	}
	FVector DropCenter;
	if (not FindAirliftDropCenter(InOutPlan.BaseLocation, DropCenter))
	{
		return;
	}

	const FVector FinalWaypoint = GetUniqueFinalWaypoint(InOutPlan.BaseLocation);
	FSurvivalAirliftRun& Run = M_AirliftRuns.AddDefaulted_GetRef();
	Run.RunId = M_NextAirliftRunId++;
	Run.DropCenter = DropCenter;
	Run.FinalWaypoint = FinalWaypoint;
	Run.AimedBaseLocation = InOutPlan.BaseLocation;
	Run.LaunchTime = Now;
	Run.Slots.SetNum(Units.Num());
	const TArray<FVector> DropLocations = BuildAirliftDropLocations(DropCenter, Units.Num());

	const ASurvivalEdgeSpawnZone* OriginZone = M_AirliftOriginZone.Get();
	const FVector Origin = OriginZone->GetPlacedSpawnCenter();
	const FVector FlightDirection = (DropCenter - Origin).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, FlightDirection);
	const float FlightZ = FMath::Max(Origin.Z, DropCenter.Z) + Airlift.FlightHeight;
	TArray<FVector> AircraftSpawnLocations;
	const int32 RunId = Run.RunId;
	int32 Requested = 0;
	for (int32 SlotIndex = 0; SlotIndex < Units.Num(); ++SlotIndex)
	{
		const float SideOffset = (SlotIndex - (Units.Num() - 1) * 0.5f) * Airlift.AircraftLateralSpacing;
		FVector SpawnLocation = Origin + Side * SideOffset;
		SpawnLocation.Z = FlightZ;
		const FVector RetreatLocation = SpawnLocation
			- FlightDirection * SurvivalWaveAirliftConstants::RetreatBeyondSpawnDistance;
		AircraftSpawnLocations.Add(SpawnLocation);
		// Re-fetch: LaunchAirliftSlot never adds runs, but keep the reference use local to this iteration.
		FSurvivalAirliftRun* LiveRun = FindAirliftRun(RunId);
		if (not LiveRun)
		{
			break;
		}
		LiveRun->Slots[SlotIndex].DropLocation = DropLocations[SlotIndex];
		Requested += LaunchAirliftSlot(*LiveRun, SlotIndex, Units[SlotIndex], SpawnLocation, RetreatLocation) ? 1 : 0;
	}
	if (Requested <= 0)
	{
		M_AirliftRuns.RemoveAll([RunId](const FSurvivalAirliftRun& EachRun) { return EachRun.RunId == RunId; });
		return;
	}

	M_LastAirliftTime = Now;
	InOutPlan.AirliftTanks = Requested;
	InOutPlan.AirliftDropCenter = DropCenter;

	// The tanks spawn under their transports: attribute them to this round there.
	FRoundSpawnMarker Marker;
	Marker.SpawnPoints = AircraftSpawnLocations;
	Marker.Expected = Requested;
	Marker.LaunchTime = Now;
	M_RoundSpawnMarkers.Add(Marker);

	if (not IsValid(M_WaveMarkerTracker))
	{
		M_WaveMarkerTracker = NewObject<USurvivalWaveMarkerTracker>(this);
		M_WaveMarkerTracker->InitTracker(GetEnemyController(), WaveSettings->FormationBindTimeoutSeconds);
	}
	const FName GroupKey(*FString::Printf(TEXT("%d_Airlift%d"), M_WavesLaunched + 1, RunId));
	M_WaveMarkerTracker->OnGroupLaunched(GroupKey, FinalWaypoint, DropLocations, InOutPlan.BaseLocation);
	UE_LOG(LogTemp, Log, TEXT("Survival heavy airlift %d: %d tanks from %s to %s."), RunId, Requested,
	       *OriginZone->GetName(), *DropCenter.ToCompactString());
}

int32 USurvivalWaveDirectorComponent::GetAirliftTankCount(const float Minute) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 0;
	}
	const int32 Wanted = FMath::Max(1, FMath::RoundToInt(USurvivalWaveSettings::EvaluateCurveAtMinute(
		                                   WaveSettings->OppositeAirlift.TanksPerAirlift, Minute, 1.f)));
	// The ground group of this wave is already counted in the load; the airlift only takes what is left.
	const float FreeLoad = GetAliveEnemyLoadCap() - GetAliveEnemyLoad();
	const float TankLoad = FMath::Max(WaveSettings->PlannedTankLoad, KINDA_SMALL_NUMBER);
	return FMath::Clamp(FMath::FloorToInt(FreeLoad / TankLoad), 0, Wanted);
}

void USurvivalWaveDirectorComponent::PickAirliftUnits(const float Minute, const int32 Count,
                                                      TArray<FSurvivalPlannedWaveUnit>& OutUnits)
{
	OutUnits.Reset();
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	if (not WaveSettings or not Profile or Count <= 0)
	{
		return;
	}
	const ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		return;
	}
	FSurvivalWaveCompositionRequest Request;
	Request.SurvivalMinute = Minute;
	Request.TechPaceMultiplier = Profile->TechPaceMultiplier;
	TArray<int32> PoolIndices;
	TArray<float> Weights;
	float TotalWeight = 0.f;
	for (int32 PoolIndex = 0; PoolIndex < WaveSettings->UnitPool.Num(); ++PoolIndex)
	{
		const FSurvivalWaveUnitEntry& Entry = WaveSettings->UnitPool[PoolIndex];
		if (Entry.Kind != ESurvivalWaveUnitKind::Tank
			or not WaveSettings->OppositeAirlift.AirliftRoles.Contains(Entry.Role))
		{
			continue;
		}
		const float Weight = FSurvivalWaveComposer::GetEntryWeight(*WaveSettings, Entry, Request);
		// A transport without a tank would have to be sent home empty: only fly what the spawner can make.
		if (Weight <= 0.f or not AsyncSpawner->GetHasClassForTrainingOption(Entry.GetTrainingOption()))
		{
			continue;
		}
		PoolIndices.Add(PoolIndex);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}
	if (PoolIndices.IsEmpty())
	{
		return;
	}
	for (int32 UnitIndex = 0; UnitIndex < Count; ++UnitIndex)
	{
		float Roll = M_Random.FRandRange(0.f, TotalWeight);
		int32 Picked = PoolIndices.Num() - 1;
		for (int32 Index = 0; Index < PoolIndices.Num(); ++Index)
		{
			Roll -= Weights[Index];
			if (Roll <= 0.f)
			{
				Picked = Index;
				break;
			}
		}
		const FSurvivalWaveUnitEntry& Entry = WaveSettings->UnitPool[PoolIndices[Picked]];
		FSurvivalPlannedWaveUnit Unit;
		Unit.Option = Entry.GetTrainingOption();
		Unit.Kind = Entry.Kind;
		Unit.Role = Entry.Role;
		Unit.Cost = Entry.GetCost();
		Unit.PoolIndex = PoolIndices[Picked];
		OutUnits.Add(Unit);
	}
}

bool USurvivalWaveDirectorComponent::FindAirliftDropCenter(const FVector& BaseLocation, FVector& OutDropCenter) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const UWorld* World = GetWorld();
	if (not WaveSettings or not IsValid(World))
	{
		return false;
	}
	using namespace SurvivalWaveAirliftConstants;
	const FSurvivalHeavyAirliftSettings& Airlift = WaveSettings->OppositeAirlift;
	bool bHasFallback = false;
	FVector Fallback = FVector::ZeroVector;
	for (int32 Ring = 0; Ring <= Airlift.DropSearchRings; ++Ring)
	{
		const int32 AngleCount = Ring == 0 ? 1 : DropSearchAnglesPerRing;
		for (int32 AngleIndex = 0; AngleIndex < AngleCount; ++AngleIndex)
		{
			const float AngleRadians = 2.f * PI * AngleIndex / AngleCount;
			const FVector Candidate = M_MapCentre
				+ FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.f) * (Ring * Airlift.DropSearchRingStep);
			FVector Projected;
			if (not UConvoyRouteLibrary::ProjectToVehicleNavmesh(World, Candidate, Projected))
			{
				continue;
			}
			if (not bHasFallback)
			{
				bHasFallback = true;
				Fallback = Projected;
			}
			if (GetHasVehiclePath(Projected, BaseLocation))
			{
				OutDropCenter = Projected;
				return true;
			}
		}
	}
	OutDropCenter = Fallback;
	return bHasFallback;
}

bool USurvivalWaveDirectorComponent::GetHasVehiclePath(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	FVector Goal;
	if (not IsValid(World) or not UConvoyRouteLibrary::ProjectToVehicleNavmesh(World, To, Goal))
	{
		return false;
	}
	TArray<FVector> PathPoints;
	return UConvoyRouteLibrary::FindChainedVehiclePath(World, From, Goal, PathPoints);
}

TArray<FVector> USurvivalWaveDirectorComponent::BuildAirliftDropLocations(const FVector& DropCenter,
                                                                          const int32 Count) const
{
	TArray<FVector> Locations;
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const float Spacing = WaveSettings ? WaveSettings->OppositeAirlift.DropSpacing : 1600.f;
	using SurvivalWaveAirliftConstants::DropProjectionScale;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Centre first, then a ring around it.
		FVector Candidate = DropCenter;
		if (Index > 0)
		{
			const float AngleRadians = 2.f * PI * (Index - 1) / FMath::Max(1, Count - 1);
			Candidate += FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.f) * Spacing;
		}
		bool bProjected = false;
		const FVector Projected = RTSFunctionLibrary::GetLocationProjected(
			this, Candidate, true, bProjected, DropProjectionScale);
		Locations.Add(bProjected ? Projected : DropCenter);
	}
	return Locations;
}

bool USurvivalWaveDirectorComponent::LaunchAirliftSlot(FSurvivalAirliftRun& Run, const int32 SlotIndex,
                                                       const FSurvivalPlannedWaveUnit& Unit,
                                                       const FVector& AircraftSpawnLocation,
                                                       const FVector& RetreatLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not WaveSettings or not IsValid(AsyncSpawner) or not Run.Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	FSurvivalAirliftSlot& Slot = Run.Slots[SlotIndex];
	AAircraftMaster* Aircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this, M_AirliftAircraftClass, AircraftSpawnLocation);
	if (not IsValid(Aircraft))
	{
		Slot.bLost = true;
		return false;
	}
	const FRotator Facing = (Slot.DropLocation - AircraftSpawnLocation).GetSafeNormal2D().Rotation();
	Aircraft->SetUnitSelectable(false);
	Aircraft->SetActorRotation(Facing);
	Aircraft->OnAircraftDropPayloadReleased.AddUObject(this, &USurvivalWaveDirectorComponent::OnAirliftPayloadReleased);
	Slot.Aircraft = Aircraft;

	const FVector TankSpawnLocation = AircraftSpawnLocation
		+ FVector(0.f, 0.f, WaveSettings->OppositeAirlift.TankAttachZOffset);
	TWeakObjectPtr<USurvivalWaveDirectorComponent> WeakThis(this);
	const int32 RunId = Run.RunId;
	const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
		Unit.Option, TankSpawnLocation, this, SlotIndex,
		[WeakThis, RunId, SlotIndex, RetreatLocation](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnAirliftTankSpawned(RunId, SlotIndex, SpawnedActor, RetreatLocation);
		}, Facing);
	if (bRequested)
	{
		return true;
	}
	// The spawner already reported why; a transport without a tank would circle forever.
	FSurvivalAirliftRun* LiveRun = FindAirliftRun(RunId);
	if (LiveRun and LiveRun->Slots.IsValidIndex(SlotIndex))
	{
		LiveRun->Slots[SlotIndex].bLost = true;
	}
	if (IsValid(Aircraft))
	{
		Aircraft->Destroy();
	}
	return false;
}

void USurvivalWaveDirectorComponent::OnAirliftTankSpawned(const int32 RunId, const int32 SlotIndex,
                                                          AActor* SpawnedActor, const FVector& RetreatLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	FSurvivalAirliftRun* Run = FindAirliftRun(RunId);
	ATankMaster* Tank = Cast<ATankMaster>(SpawnedActor);
	if (not WaveSettings or not Run or not Run->Slots.IsValidIndex(SlotIndex))
	{
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		return;
	}
	FSurvivalAirliftSlot& Slot = Run->Slots[SlotIndex];
	AAircraftMaster* Aircraft = Slot.Aircraft.Get();
	if (not IsValid(Tank) or not IsValid(Aircraft))
	{
		// Shot down before its tank existed, or the spawn failed: neither may stay behind alone.
		Slot.bLost = true;
		if (IsValid(Aircraft))
		{
			Aircraft->Destroy();
		}
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		return;
	}
	Slot.Tank = Tank;
	const FSurvivalHeavyAirliftSettings& Airlift = WaveSettings->OppositeAirlift;
	FAircraftDropRequest DropRequest;
	DropRequest.State = EAircraftDropRequestState::MovingToDropLocation;
	DropRequest.PayloadType = EAircraftDropPayloadType::Tank;
	DropRequest.OwningPlayer = SurvivalWaveAirliftConstants::EnemyPlayerIndex;
	DropRequest.ExecuteLocation = Slot.DropLocation;
	DropRequest.RetreatLocation = RetreatLocation;
	DropRequest.TankAttachZOffset = Airlift.TankAttachZOffset;
	DropRequest.HowLongStayLanded = Airlift.HowLongStayLandedSeconds;
	DropRequest.AttachedTank = Tank;
	Aircraft->OrderAircraftDrop(DropRequest);
}

void USurvivalWaveDirectorComponent::OnAirliftPayloadReleased(AAircraftMaster* Aircraft, ATankMaster* ReleasedTank)
{
	if (IsValid(Aircraft))
	{
		Aircraft->OnAircraftDropPayloadReleased.RemoveAll(this);
	}
	for (FSurvivalAirliftRun& EachRun : M_AirliftRuns)
	{
		FSurvivalAirliftSlot* Slot = EachRun.Slots.FindByPredicate([Aircraft](const FSurvivalAirliftSlot& EachSlot)
		{
			return EachSlot.Aircraft.Get() == Aircraft;
		});
		if (not Slot)
		{
			continue;
		}
		if (EachRun.bFormed)
		{
			// Too late for its group: it stays with the strategic AI, which registered it on release.
			return;
		}
		Slot->bReleased = true;
		Slot->Tank = ReleasedTank;
		const double Now = GetWorldTime();
		EachRun.FirstReleaseTime = EachRun.FirstReleaseTime < 0.0 ? Now : EachRun.FirstReleaseTime;
		EachRun.LastReleaseTime = Now;
		// The drop put the tank on the strategic AI's blackboard; the airlift group owns it until it arrives.
		AEnemyController* EnemyController = GetEnemyController();
		UEnemyDirectControlComponent* DirectControl = EnemyController
			                                              ? EnemyController->GetEnemyDirectControlComponent()
			                                              : nullptr;
		if (IsValid(DirectControl) and IsValid(ReleasedTank))
		{
			DirectControl->DeregisterDirectControlUnit(ReleasedTank);
		}
		return;
	}
}

// ---------------------------------------------------------------- Airlift: forming the group

void USurvivalWaveDirectorComponent::UpdateAirliftRuns()
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (M_AirliftRuns.IsEmpty() or not WaveSettings)
	{
		return;
	}
	const double Now = GetWorldTime();
	// FormAirliftGroup never adds or removes runs; removal happens after the loop.
	for (FSurvivalAirliftRun& EachRun : M_AirliftRuns)
	{
		if (EachRun.bFormed)
		{
			continue;
		}
		UpdateAirliftSlotStates(EachRun);
		if (GetShouldFormAirliftGroup(EachRun, Now))
		{
			FormAirliftGroup(EachRun);
		}
	}
	const float Timeout = WaveSettings->OppositeAirlift.RunTimeoutSeconds;
	M_AirliftRuns.RemoveAll([Now, Timeout](const FSurvivalAirliftRun& EachRun)
	{
		const bool bAllLost = not EachRun.Slots.ContainsByPredicate([](const FSurvivalAirliftSlot& EachSlot)
		{
			return not EachSlot.bLost;
		});
		return EachRun.bFormed or bAllLost or Now - EachRun.LaunchTime > Timeout;
	});
}

void USurvivalWaveDirectorComponent::UpdateAirliftSlotStates(FSurvivalAirliftRun& Run) const
{
	for (FSurvivalAirliftSlot& EachSlot : Run.Slots)
	{
		if (EachSlot.bLost)
		{
			continue;
		}
		const bool bTankDead = EachSlot.Tank.IsValid() and not RTSFunctionLibrary::RTSIsValid(EachSlot.Tank.Get());
		const bool bTankGone = EachSlot.Tank.IsStale() or bTankDead;
		// A transport gone before the release took its tank with it (or never had one).
		const bool bAircraftGoneEarly = not EachSlot.bReleased and not EachSlot.Aircraft.IsValid();
		if (bTankGone or bAircraftGoneEarly)
		{
			EachSlot.bLost = true;
		}
	}
}

bool USurvivalWaveDirectorComponent::GetShouldFormAirliftGroup(const FSurvivalAirliftRun& Run, const double Now) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings or Run.FirstReleaseTime < 0.0)
	{
		return false;
	}
	const FSurvivalHeavyAirliftSettings& Airlift = WaveSettings->OppositeAirlift;
	const bool bAllResolved = not Run.Slots.ContainsByPredicate([](const FSurvivalAirliftSlot& EachSlot)
	{
		return not EachSlot.bReleased and not EachSlot.bLost;
	});
	if (bAllResolved)
	{
		return Now - Run.LastReleaseTime >= Airlift.FormationDelaySeconds;
	}
	return Now - Run.FirstReleaseTime >= Airlift.MaxWaitAfterFirstDropSeconds;
}

void USurvivalWaveDirectorComponent::FormAirliftGroup(FSurvivalAirliftRun& Run)
{
	Run.bFormed = true;
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	if (not WaveSettings or not EnemyController or not GetIsValidDirector())
	{
		return;
	}
	TArray<ATankMaster*> Tanks;
	FVector LocationSum = FVector::ZeroVector;
	for (const FSurvivalAirliftSlot& EachSlot : Run.Slots)
	{
		ATankMaster* Tank = EachSlot.Tank.Get();
		if (not EachSlot.bReleased or EachSlot.bLost or not RTSFunctionLibrary::RTSIsValid(Tank))
		{
			continue;
		}
		Tanks.Add(Tank);
		LocationSum += Tank->GetActorLocation();
	}
	if (Tanks.IsEmpty())
	{
		return;
	}
	const FVector Centroid = LocationSum / Tanks.Num();
	const FVector BaseLocation = M_Director->GetCurrentPlayerBaseLocation();
	const bool bBaseMoved = FVector::Dist2D(BaseLocation, Run.AimedBaseLocation) >= WaveSettings->RetargetDistance;
	// Keep the launch-time point (the minimap marker waits for it) unless the base moved away meanwhile.
	const FVector FinalWaypoint = bBaseMoved ? GetUniqueFinalWaypoint(BaseLocation) : Run.FinalWaypoint;
	const TArray<FVector> Waypoints = BuildWaypoints(Centroid, FinalWaypoint, false);
	const FVector PreviousWaypoint = Waypoints.Num() > 1 ? Waypoints[Waypoints.Num() - 2] : Centroid;
	const FRotator FinalDirection(0.f, (FinalWaypoint - PreviousWaypoint).GetSafeNormal2D().Rotation().Yaw, 0.f);
	EnemyController->MoveAttackMoveFormationToLocation(
		{},
		Tanks,
		Waypoints,
		FinalDirection,
		GetFormationWidth(Tanks.Num()),
		WaveSettings->FormationOffsetMultiplier,
		MakeAttackMoveSettings(),
		Centroid);

	FSurvivalTrackedWaveGroup Group;
	Group.WaveNumber = M_RoundNumber;
	Group.ExpectedFinalWaypoint = FinalWaypoint;
	Group.AimedBaseLocation = bBaseMoved ? BaseLocation : Run.AimedBaseLocation;
	// Formed now: the "never seen" timeout counts from here, not from the take-off.
	Group.LaunchTime = GetWorldTime();
	Group.PlannedUnitCount = Tanks.Num();
	M_TrackedGroups.Add(Group);
	UE_LOG(LogTemp, Log, TEXT("Survival heavy airlift %d formed: %d tanks attack-move from %s."), Run.RunId,
	       Tanks.Num(), *Centroid.ToCompactString());
}

FSurvivalAirliftRun* USurvivalWaveDirectorComponent::FindAirliftRun(const int32 RunId)
{
	return M_AirliftRuns.FindByPredicate([RunId](const FSurvivalAirliftRun& EachRun)
	{
		return EachRun.RunId == RunId;
	});
}

// ---------------------------------------------------------------- Stalled groups

bool USurvivalWaveDirectorComponent::UpdateGroupProgress(FSurvivalTrackedWaveGroup& Group,
                                                         const FFormationData& Formation, const double Now) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return false;
	}
	if (Formation.CurrentWaypointIndex != Group.LastWaypointIndex)
	{
		Group.LastWaypointIndex = Formation.CurrentWaypointIndex;
		Group.LastProgressTime = Now;
		return false;
	}
	return Now - Group.LastProgressTime >= WaveSettings->StalledGroupSeconds;
}

bool USurvivalWaveDirectorComponent::RecoverStalledGroup(FSurvivalTrackedWaveGroup& Group,
                                                         const FFormationData& Formation,
                                                         const FVector& BaseLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	UEnemyFormationController* FormationController = EnemyController
		                                                 ? EnemyController->GetEnemyFormationController()
		                                                 : nullptr;
	if (not WaveSettings or not IsValid(FormationController))
	{
		return false;
	}
	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	TArray<AActor*> Stragglers;
	FVector LocationSum = FVector::ZeroVector;
	for (const FFormationUnitData& EachUnit : Formation.FormationUnits)
	{
		AActor* UnitActor = EachUnit.IsValidFormationUnit() ? EachUnit.Unit->GetOwnerActor() : nullptr;
		if (not RTSFunctionLibrary::RTSIsValid(UnitActor))
		{
			continue;
		}
		if (not EachUnit.bHasReachedNextDestination)
		{
			Stragglers.Add(UnitActor);
			continue;
		}
		LocationSum += UnitActor->GetActorLocation();
		if (ATankMaster* Tank = Cast<ATankMaster>(UnitActor))
		{
			Tanks.Add(Tank);
		}
		else if (ASquadController* Squad = Cast<ASquadController>(UnitActor))
		{
			Squads.Add(Squad);
		}
	}
	// Nobody arrived (the whole group is held up): re-path everyone from where they stand.
	if (Squads.IsEmpty() and Tanks.IsEmpty())
	{
		Group.LastProgressTime = GetWorldTime();
		return RetargetGroup(Group, Formation, BaseLocation);
	}
	const FVector Centroid = LocationSum / (Squads.Num() + Tanks.Num());
	const FVector FinalWaypoint = GetUniqueFinalWaypoint(BaseLocation);
	const TArray<FVector> Waypoints = BuildWaypoints(Centroid, FinalWaypoint, false);
	const FVector PreviousWaypoint = Waypoints.Num() > 1 ? Waypoints[Waypoints.Num() - 2] : Centroid;
	const FRotator FinalDirection(0.f, (FinalWaypoint - PreviousWaypoint).GetSafeNormal2D().Rotation().Yaw, 0.f);

	// Remove and re-create back to back, as a retarget does; the stragglers leave the wave for the strategic AI.
	FormationController->RemoveActiveFormationsByID({Formation.FormationID});
	EnemyController->MoveAttackMoveFormationToLocation(
		Squads, Tanks, Waypoints, FinalDirection, GetFormationWidth(Squads.Num() + Tanks.Num()),
		WaveSettings->FormationOffsetMultiplier, MakeAttackMoveSettings(), Centroid);
	UEnemyDirectControlComponent* DirectControl = EnemyController->GetEnemyDirectControlComponent();
	for (AActor* EachStraggler : Stragglers)
	{
		if (IsValid(DirectControl))
		{
			DirectControl->RegisterDirectControlUnit(EachStraggler);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Survival wave group stalled at waypoint %d/%d for %.0f s: %d units continue, %d "
		       "stragglers go to the strategic AI."),
	       Formation.CurrentWaypointIndex, Formation.FormationWaypoints.Num(), GetWorldTime() - Group.LastProgressTime,
	       Squads.Num() + Tanks.Num(), Stragglers.Num());
	Group.ExpectedFinalWaypoint = FinalWaypoint;
	Group.AimedBaseLocation = BaseLocation;
	Group.LastRetargetTime = GetWorldTime();
	Group.FormationID = INDEX_NONE;
	Group.LastWaypointIndex = INDEX_NONE;
	Group.LastProgressTime = GetWorldTime();
	return true;
}

void USurvivalWaveDirectorComponent::LogWaveProbeIfRequested()
{
#if RTS_WITH_MAP_TESTS
	constexpr double ProbeIntervalSeconds = 10.0;
	static const bool bProbeRequested = FParse::Param(FCommandLine::Get(), TEXT("SurvivalWaveProbe"));
	const double Now = GetWorldTime();
	if (not bProbeRequested or Now < M_NextWaveProbeTime or not GetIsValidDirector())
	{
		return;
	}
	M_NextWaveProbeTime = Now + ProbeIntervalSeconds;
	const FVector BaseLocation = M_Director->GetCurrentPlayerBaseLocation();
	int32 AttackMoves = 0;
	int32 PlainMoves = 0;
	int32 Attacking = 0;
	int32 Other = 0;
	int32 AttackingFarFromBase = 0;
	constexpr float FarFromBaseDistance = 9000.f;
	TArray<TWeakObjectPtr<AActor>> WaveUnits = M_AllWaveTanks;
	WaveUnits.Append(M_AllWaveSquads);
	for (const TWeakObjectPtr<AActor>& EachUnit : WaveUnits)
	{
		ICommands* Commands = Cast<ICommands>(EachUnit.Get());
		UCommandData* CommandData = Commands ? Commands->GetIsValidCommandData() : nullptr;
		if (not CommandData)
		{
			continue;
		}
		const EAbilityID ActiveCommand = Commands->GetActiveCommandID();
		if (ActiveCommand == EAbilityID::IdAttack)
		{
			++Attacking;
			AttackingFarFromBase += FVector::Dist2D(EachUnit->GetActorLocation(), BaseLocation) > FarFromBaseDistance
				                        ? 1
				                        : 0;
			continue;
		}
		if (ActiveCommand == EAbilityID::IdMove)
		{
			CommandData->GetIsActiveCommandAttackMove() ? ++AttackMoves : ++PlainMoves;
			continue;
		}
		++Other;
	}
	int32 PendingAirliftTanks = 0;
	for (const FSurvivalAirliftRun& EachRun : M_AirliftRuns)
	{
		PendingAirliftTanks += EachRun.Slots.Num();
	}
	UE_LOG(LogTemp, Warning, TEXT("SURVIVALWAVEPROBE t=%.0f minute=%.1f units=%d attackmove=%d plainmove=%d "
		       "attacking=%d (far from base %d) other=%d airliftruns=%d airlifttanks=%d tracked=%d"),
	       Now, GetSurvivalMinute(), WaveUnits.Num(), AttackMoves, PlainMoves, Attacking, AttackingFarFromBase, Other,
	       M_AirliftRuns.Num(), PendingAirliftTanks, M_TrackedGroups.Num());
	AEnemyController* EnemyController = GetEnemyController();
	const UEnemyFormationController* FormationController = EnemyController
		                                                       ? EnemyController->GetEnemyFormationController()
		                                                       : nullptr;
	if (not IsValid(FormationController))
	{
		return;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	for (const FSurvivalTrackedWaveGroup& EachGroup : M_TrackedGroups)
	{
		const int32 FormationIndex = FindFormationIndexForGroup(EachGroup, Formations);
		if (FormationIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("SURVIVALWAVEPROBE group final=%s no formation (seen %d, age %.0f s)"),
			       *EachGroup.ExpectedFinalWaypoint.ToCompactString(), EachGroup.bFormationSeen ? 1 : 0,
			       Now - EachGroup.LaunchTime);
			continue;
		}
		const FFormationData& Formation = Formations[FormationIndex];
		TArray<ASquadController*> Squads;
		TArray<ATankMaster*> Tanks;
		FVector Centroid = FVector::ZeroVector;
		GetFormationUnits(Formation, Squads, Tanks, Centroid);
		int32 Reached = 0;
		int32 Idle = 0;
		for (const FFormationUnitData& EachUnit : Formation.FormationUnits)
		{
			if (not EachUnit.IsValidFormationUnit())
			{
				continue;
			}
			Reached += EachUnit.bHasReachedNextDestination ? 1 : 0;
			Idle += not EachUnit.bHasReachedNextDestination and EachUnit.Unit->GetActiveCommandID() == EAbilityID::IdIdle
				        ? 1
				        : 0;
		}
		UE_LOG(LogTemp, Warning, TEXT("SURVIVALWAVEPROBE group id=%d waypoint=%d/%d units=%d reached=%d idle=%d "
			       "centroid=%s base=%.0f age=%.0f"),
		       Formation.FormationID, Formation.CurrentWaypointIndex, Formation.FormationWaypoints.Num(),
		       Squads.Num() + Tanks.Num(), Reached, Idle, *Centroid.ToCompactString(),
		       FVector::Dist2D(Centroid, BaseLocation), Now - EachGroup.LaunchTime);
	}
#endif
}

void USurvivalWaveDirectorComponent::UpdateDecoyTest()
{
#if RTS_WITH_MAP_TESTS
	static const bool bDecoyRequested = FParse::Param(FCommandLine::Get(), TEXT("SurvivalDecoyTest"));
	constexpr int32 MaxDecoys = 3;
	constexpr float MinDistanceFromBase = 20000.f;
	constexpr float DecoyAheadDistance = 3000.f;
	constexpr float NearDecoyDistance = 7000.f;
	constexpr double StoppedCheckSeconds = 20.0;
	constexpr double DecoyTimeoutSeconds = 60.0;
	constexpr int32 MinGroupUnits = 3;
	if (not bDecoyRequested or not GetIsValidDirector())
	{
		return;
	}
	const double Now = GetWorldTime();
	const FVector BaseLocation = M_Director->GetCurrentPlayerBaseLocation();
	TArray<TWeakObjectPtr<AActor>> WaveUnits = M_AllWaveTanks;
	WaveUnits.Append(M_AllWaveSquads);
	if (bM_DecoyRequested)
	{
		const bool bAlive = M_DecoyTank.IsValid() and RTSFunctionLibrary::RTSIsValid(M_DecoyTank.Get());
		int32 NearAttacking = 0;
		int32 NearNow = 0;
		for (const TWeakObjectPtr<AActor>& EachUnit : WaveUnits)
		{
			ICommands* Commands = Cast<ICommands>(EachUnit.Get());
			if (Commands and FVector::Dist2D(EachUnit->GetActorLocation(), M_DecoyLocation) <= NearDecoyDistance)
			{
				++NearNow;
				NearAttacking += Commands->GetActiveCommandID() == EAbilityID::IdAttack ? 1 : 0;
			}
		}
		if (bAlive)
		{
			++M_DecoyStats.AliveSamples;
			M_DecoyStats.SumAttacking += NearAttacking;
			M_DecoyStats.SumNear += NearNow;
		}
		M_DecoyStats.MaxAttacking = FMath::Max(M_DecoyStats.MaxAttacking, NearAttacking);
		if (M_DecoyStats.bSpawned and not bAlive and M_DecoyStats.KilledAfterSeconds < 0.0)
		{
			M_DecoyStats.KilledAfterSeconds = Now - M_DecoyStats.SpawnTime;
		}
		M_DecoyStats.bSpawned = M_DecoyStats.bSpawned or bAlive;
		if (M_DecoyStats.StillNearAt20s < 0 and Now - M_DecoyStats.SpawnTime >= StoppedCheckSeconds)
		{
			M_DecoyStats.StillNearAt20s = 0;
			for (const TWeakObjectPtr<AActor>& EachUnit : M_DecoyStats.NearAtSpawn)
			{
				M_DecoyStats.StillNearAt20s += EachUnit.IsValid()
				                               and FVector::Dist2D(EachUnit->GetActorLocation(), M_DecoyLocation)
				                               <= NearDecoyDistance
					                               ? 1
					                               : 0;
			}
		}
		const bool bDone = M_DecoyStats.StillNearAt20s >= 0 and (not bAlive or Now - M_DecoyStats.SpawnTime >= DecoyTimeoutSeconds);
		if (not bDone)
		{
			return;
		}
		const float Samples = FMath::Max(1, M_DecoyStats.AliveSamples);
		UE_LOG(LogTemp, Warning, TEXT("SURVIVALDECOY result %d: wave units near at spawn=%d, max attacking it=%d, "
			       "killed after %.0f s (-1 = survived %.0f s), still near after 20 s=%d of %d, while it lived (%d s) "
			       "on average %.1f of %.1f near wave units were attacking"),
		       M_DecoyCount, M_DecoyStats.NearAtSpawn.Num(), M_DecoyStats.MaxAttacking,
		       M_DecoyStats.KilledAfterSeconds, DecoyTimeoutSeconds, M_DecoyStats.StillNearAt20s,
		       M_DecoyStats.NearAtSpawn.Num(), M_DecoyStats.AliveSamples, M_DecoyStats.SumAttacking / Samples,
		       M_DecoyStats.SumNear / Samples);
		if (M_DecoyTank.IsValid())
		{
			M_DecoyTank->Destroy();
		}
		bM_DecoyRequested = false;
		return;
	}
	if (M_DecoyCount >= MaxDecoys)
	{
		return;
	}
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	AEnemyController* EnemyController = GetEnemyController();
	const UEnemyFormationController* FormationController = EnemyController
		                                                       ? EnemyController->GetEnemyFormationController()
		                                                       : nullptr;
	if (not IsValid(AsyncSpawner) or not IsValid(FormationController))
	{
		return;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	for (const FFormationData& EachFormation : Formations)
	{
		TArray<ASquadController*> Squads;
		TArray<ATankMaster*> Tanks;
		FVector Centroid = FVector::ZeroVector;
		GetFormationUnits(EachFormation, Squads, Tanks, Centroid);
		const bool bIsWaveFormation = M_TrackedGroups.ContainsByPredicate([&EachFormation](const FSurvivalTrackedWaveGroup& Group)
		{
			return Group.FormationID == EachFormation.FormationID;
		});
		const bool bAlreadyUsed = M_DecoyFormationIDs.Contains(EachFormation.FormationID);
		if (not bIsWaveFormation or bAlreadyUsed or Squads.Num() + Tanks.Num() < MinGroupUnits
			or FVector::Dist2D(Centroid, BaseLocation) < MinDistanceFromBase
			or not EachFormation.FormationWaypoints.IsValidIndex(EachFormation.CurrentWaypointIndex))
		{
			continue;
		}
		const FVector Ahead = (EachFormation.FormationWaypoints[EachFormation.CurrentWaypointIndex] - Centroid).GetSafeNormal2D();
		bool bProjected = false;
		const FVector DecoyLocation = RTSFunctionLibrary::GetLocationProjected(
			this, Centroid + Ahead * DecoyAheadDistance, true, bProjected, 3.f);
		if (not bProjected)
		{
			continue;
		}
		M_DecoyLocation = DecoyLocation;
		M_DecoyFormationIDs.Add(EachFormation.FormationID);
		M_DecoyStats = FSurvivalDecoyStats();
		M_DecoyStats.SpawnTime = Now;
		for (const TWeakObjectPtr<AActor>& EachUnit : WaveUnits)
		{
			if (EachUnit.IsValid() and FVector::Dist2D(EachUnit->GetActorLocation(), M_DecoyLocation) <= NearDecoyDistance)
			{
				M_DecoyStats.NearAtSpawn.Add(EachUnit);
			}
		}
		++M_DecoyCount;
		bM_DecoyRequested = true;
		M_DecoyTank.Reset();
		TWeakObjectPtr<USurvivalWaveDirectorComponent> WeakThis(this);
		AsyncSpawner->AsyncSpawnOptionAtLocation(
			FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Maus)),
			M_DecoyLocation + FVector(0.f, 0.f, 150.f), this, 0,
			[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->M_DecoyTank = SpawnedActor;
				}
			}, (-Ahead).Rotation());
		UE_LOG(LogTemp, Warning, TEXT("SURVIVALDECOY %d: player Maus at %s, %.0f ahead of formation %d (%d units, "
			       "%.0f from base)"), M_DecoyCount, *M_DecoyLocation.ToCompactString(), DecoyAheadDistance,
		       EachFormation.FormationID, Squads.Num() + Tanks.Num(), FVector::Dist2D(Centroid, BaseLocation));
		return;
	}
#endif
}

float USurvivalWaveDirectorComponent::GetPendingAirliftLoad() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 0.f;
	}
	int32 PendingTanks = 0;
	for (const FSurvivalAirliftRun& EachRun : M_AirliftRuns)
	{
		for (const FSurvivalAirliftSlot& EachSlot : EachRun.Slots)
		{
			// Spawned tanks are counted as wave tanks already.
			PendingTanks += not EachSlot.bLost and not EachSlot.Tank.IsValid() ? 1 : 0;
		}
	}
	return PendingTanks * WaveSettings->PlannedTankLoad;
}
