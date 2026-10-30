// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalDeploymentComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalDeploymentConstants
{
	constexpr int32 PlayerOwner = 1;
	// Sideways spacing of consecutive aircraft so they do not fly through each other.
	constexpr float AircraftLateralSpacing = 700.f;
	constexpr int32 AircraftLateralColumns = 5;
	constexpr float MonitorInterval = 0.5f;
	constexpr float GroundTraceHalfHeight = 50000.f;
	// A drop that never reports back (aircraft shot down, spawn failure) must not block the grace period forever.
	constexpr float DeploymentTimeoutSeconds = 240.f;
	const TCHAR* GlobalAbilitySetPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem.DA_GlobalAbilitySystem");
}

USurvivalDeploymentComponent::USurvivalDeploymentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USurvivalDeploymentComponent::InitDeployment(ASurvivalModeDirector* Director, const bool bIsReinforcement)
{
	M_Director = Director;
	bM_IsReinforcement = bIsReinforcement;
}

void USurvivalDeploymentComponent::StartDeployment(const TArray<FSurvivalArmyPick>& Army, const FVector& DeployCenter,
                                                   const FVector& ApproachDirection)
{
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	if (bM_IsDeploying or not DeploymentSettings)
	{
		return;
	}
	bM_IsDeploying = true;
	bM_HasReportedHQLanded = false;
	M_DeployCenter = DeployCenter;
	M_ApproachDirection = ApproachDirection.GetSafeNormal2D();
	if (M_ApproachDirection.IsNearlyZero())
	{
		M_ApproachDirection = FVector::ForwardVector;
	}
	BuildDropPlan(Army);
	UE_LOG(LogTemp, Log, TEXT("Survival deployment: %d vehicle drops, %d landing squad drops, %d paratrooper drops."),
	       M_VehicleDrops.Num(), M_LandingSquadDrops.Num(), M_ParatrooperDrops.Num());

	if (DeploymentSettings->VehicleDropAircraftClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival deployment has no VehicleDropAircraftClass; vehicles are placed "
			"on the ground without aircraft."));
		OnVehicleAircraftClassLoaded();
		return;
	}
	TWeakObjectPtr<USurvivalDeploymentComponent> WeakThis(this);
	M_AircraftClassHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		DeploymentSettings->VehicleDropAircraftClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnVehicleAircraftClassLoaded();
			}
		}));
}

void USurvivalDeploymentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
		World->GetTimerManager().ClearTimer(M_MonitorTimerHandle);
	}
	M_AircraftClassHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void USurvivalDeploymentComponent::BuildDropPlan(const TArray<FSurvivalArmyPick>& Army)
{
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	M_VehicleDrops.Reset();
	M_LandingSquadDrops.Reset();
	M_ParatrooperDrops.Reset();

	TArray<ESquadSubtype> ParatrooperSquads;
	TArray<ESquadSubtype> LandingSquads;
	FVehicleDrop HQDrop;
	TArray<FVehicleDrop> OtherVehicles;
	for (const FSurvivalArmyPick& Pick : Army)
	{
		const FTrainingOption& Unit = Pick.Entry.Unit;
		if (Unit.UnitType == EAllUnitType::UNType_Squad)
		{
			const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(Unit.SubtypeValue);
			if (DeploymentSettings->SquadsDeliveredByLandingAircraft.Contains(SquadSubtype))
			{
				LandingSquads.Add(SquadSubtype);
			}
			else
			{
				ParatrooperSquads.Add(SquadSubtype);
			}
			continue;
		}
		FVehicleDrop VehicleDrop;
		VehicleDrop.Unit = Unit;
		VehicleDrop.bIsHQ = Pick.Entry.bIsHQ;
		if (VehicleDrop.bIsHQ)
		{
			HQDrop = VehicleDrop;
			continue;
		}
		OtherVehicles.Add(VehicleDrop);
	}

	// The HQ lands in the centre; the other vehicles on rings around it.
	float OuterVehicleRadius = 0.f;
	const TArray<FVector> VehicleLandings = BuildVehicleLandingLocations(OtherVehicles.Num(), OuterVehicleRadius);
	if (not HQDrop.Unit.IsNone())
	{
		HQDrop.LandingLocation = ProjectToGround(M_DeployCenter);
		M_VehicleDrops.Add(HQDrop);
	}
	for (int32 Index = 0; Index < OtherVehicles.Num(); ++Index)
	{
		OtherVehicles[Index].LandingLocation = VehicleLandings.IsValidIndex(Index)
			                                       ? VehicleLandings[Index]
			                                       : ProjectToGround(M_DeployCenter);
		M_VehicleDrops.Add(OtherVehicles[Index]);
	}

	// Infantry land on an outer ring, spread around the base so the drops do not stack.
	const float InfantryRadius = OuterVehicleRadius + DeploymentSettings->InfantryLandingOffset;
	auto GetInfantryLanding = [this, InfantryRadius](const int32 DropIndex, const int32 DropCount)
	{
		const float Angle = 2.f * PI * (static_cast<float>(DropIndex) + 0.5f) / FMath::Max(1, DropCount);
		const FVector Offset(FMath::Cos(Angle) * InfantryRadius, FMath::Sin(Angle) * InfantryRadius, 0.f);
		return ProjectToGround(M_DeployCenter + Offset);
	};

	const int32 SquadsPerParatrooperAircraft = FMath::Max(1, DeploymentSettings->SquadsPerParatrooperAircraft);
	const int32 SquadsPerLandingAircraft = FMath::Max(1, DeploymentSettings->SquadsPerLandingAircraft);
	const int32 ParatrooperDropCount = FMath::DivideAndRoundUp(ParatrooperSquads.Num(), SquadsPerParatrooperAircraft);
	const int32 LandingDropCount = FMath::DivideAndRoundUp(LandingSquads.Num(), SquadsPerLandingAircraft);
	const int32 InfantryDropCount = ParatrooperDropCount + LandingDropCount;
	int32 InfantryDropIndex = 0;
	for (int32 First = 0; First < ParatrooperSquads.Num(); First += SquadsPerParatrooperAircraft)
	{
		FParatrooperDrop Drop;
		for (int32 Index = First; Index < FMath::Min(First + SquadsPerParatrooperAircraft, ParatrooperSquads.Num());
		     ++Index)
		{
			Drop.Squads.Add(ParatrooperSquads[Index]);
		}
		Drop.LandingLocation = GetInfantryLanding(InfantryDropIndex++, InfantryDropCount);
		M_ParatrooperDrops.Add(Drop);
	}
	for (int32 First = 0; First < LandingSquads.Num(); First += SquadsPerLandingAircraft)
	{
		FLandingSquadDrop Drop;
		for (int32 Index = First; Index < FMath::Min(First + SquadsPerLandingAircraft, LandingSquads.Num()); ++Index)
		{
			Drop.Squads.Add(LandingSquads[Index]);
		}
		Drop.LandingLocation = GetInfantryLanding(InfantryDropIndex++, InfantryDropCount);
		M_LandingSquadDrops.Add(Drop);
	}
}

TArray<FVector> USurvivalDeploymentComponent::BuildVehicleLandingLocations(const int32 Amount,
                                                                           float& OutOuterRadius) const
{
	TArray<FVector> Locations;
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	const float Spacing = DeploymentSettings ? DeploymentSettings->VehicleLandingSpacing : 1100.f;
	OutOuterRadius = Spacing;
	int32 Ring = 1;
	while (Locations.Num() < Amount)
	{
		const float Radius = Spacing * Ring;
		const int32 SlotsOnRing = FMath::Max(6, FMath::FloorToInt(2.f * PI * Radius / Spacing));
		// Offset every other ring so the vehicles of neighbouring rings do not line up.
		const float AngleOffset = (Ring % 2 == 0) ? PI / SlotsOnRing : 0.f;
		for (int32 Slot = 0; Slot < SlotsOnRing and Locations.Num() < Amount; ++Slot)
		{
			const float Angle = AngleOffset + 2.f * PI * Slot / SlotsOnRing;
			Locations.Add(ProjectToGround(M_DeployCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius));
		}
		OutOuterRadius = Radius;
		++Ring;
	}
	return Locations;
}

FVector USurvivalDeploymentComponent::ProjectToGround(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return Location;
	}
	FVector Result = Location;
	if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation NavLocation;
		const FVector Extent = DeveloperSettings::GamePlay::Navigation::RTSToNavProjectionExtent * 8.f;
		if (NavSystem->ProjectPointToNavigation(Location, NavLocation, Extent))
		{
			Result = NavLocation.Location;
		}
	}
	FHitResult Hit;
	const FVector TraceStart(Result.X, Result.Y, Result.Z + SurvivalDeploymentConstants::GroundTraceHalfHeight);
	const FVector TraceEnd(Result.X, Result.Y, Result.Z - SurvivalDeploymentConstants::GroundTraceHalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivalDeployGround), false);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		Result.Z = Hit.Location.Z;
	}
	return Result;
}

void USurvivalDeploymentComponent::OnVehicleAircraftClassLoaded()
{
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	M_VehicleAircraftClass = DeploymentSettings ? DeploymentSettings->VehicleDropAircraftClass.Get() : nullptr;
	M_AircraftClassHandle.Reset();
	M_NextLaunchIndex = 0;
	M_LaunchedAircraftCount = 0;

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	LaunchNext();
	const float Interval = DeploymentSettings ? FMath::Max(0.05f, DeploymentSettings->SecondsBetweenAircraft) : 1.f;
	World->GetTimerManager().SetTimer(M_LaunchTimerHandle, this, &USurvivalDeploymentComponent::LaunchNext,
	                                  Interval, true);
	World->GetTimerManager().SetTimer(M_MonitorTimerHandle, this, &USurvivalDeploymentComponent::MonitorDeployment,
	                                  SurvivalDeploymentConstants::MonitorInterval, true);
	TWeakObjectPtr<USurvivalDeploymentComponent> WeakThis(this);
	FTimerHandle TimeoutHandle;
	World->GetTimerManager().SetTimer(TimeoutHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid() and WeakThis->bM_IsDeploying)
		{
			RTSFunctionLibrary::ReportError(TEXT("Survival deployment timed out; starting the grace period anyway."));
			WeakThis->FinishDeployment();
		}
	}), SurvivalDeploymentConstants::DeploymentTimeoutSeconds, false);
}

int32 USurvivalDeploymentComponent::GetTotalLaunchCount() const
{
	return M_VehicleDrops.Num() + M_LandingSquadDrops.Num() + M_ParatrooperDrops.Num();
}

void USurvivalDeploymentComponent::LaunchNext()
{
	const int32 LaunchIndex = M_NextLaunchIndex;
	if (LaunchIndex >= GetTotalLaunchCount())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
		}
		return;
	}
	++M_NextLaunchIndex;
	// Vehicles first (HQ at index 0), then team weapons, then paratroopers.
	if (LaunchIndex < M_VehicleDrops.Num())
	{
		LaunchVehicleDrop(M_VehicleDrops[LaunchIndex]);
		return;
	}
	const int32 LandingIndex = LaunchIndex - M_VehicleDrops.Num();
	if (LandingIndex < M_LandingSquadDrops.Num())
	{
		LaunchLandingSquadDrop(M_LandingSquadDrops[LandingIndex]);
		return;
	}
	const int32 ParatrooperIndex = LandingIndex - M_LandingSquadDrops.Num();
	if (M_ParatrooperDrops.IsValidIndex(ParatrooperIndex))
	{
		LaunchParatrooperDrop(M_ParatrooperDrops[ParatrooperIndex]);
	}
}

FVector USurvivalDeploymentComponent::GetAircraftSpawnLocation(const int32 AircraftIndex) const
{
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	const float ApproachDistance = DeploymentSettings ? DeploymentSettings->ApproachDistance : 30000.f;
	const float FlightHeight = DeploymentSettings ? DeploymentSettings->AircraftFlightHeight : 3500.f;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, M_ApproachDirection);
	const int32 Column = AircraftIndex % SurvivalDeploymentConstants::AircraftLateralColumns;
	const float SideOffset = (Column - SurvivalDeploymentConstants::AircraftLateralColumns / 2) *
		SurvivalDeploymentConstants::AircraftLateralSpacing;
	FVector SpawnLocation = M_DeployCenter + M_ApproachDirection * ApproachDistance + Side * SideOffset;
	SpawnLocation.Z = M_DeployCenter.Z + FlightHeight;
	return SpawnLocation;
}

FVector USurvivalDeploymentComponent::GetAircraftRetreatLocation(const int32 AircraftIndex) const
{
	// Leave the way they came so they never cross the next aircraft's landing.
	return GetAircraftSpawnLocation(AircraftIndex) + M_ApproachDirection * 5000.f;
}

void USurvivalDeploymentComponent::LaunchVehicleDrop(const FVehicleDrop& Drop)
{
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival deployment found no async spawner."));
		return;
	}
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	const int32 AircraftIndex = M_LaunchedAircraftCount++;
	const FVector AircraftSpawnLocation = GetAircraftSpawnLocation(AircraftIndex);

	AAircraftMaster* Aircraft = nullptr;
	if (IsValid(M_VehicleAircraftClass))
	{
		Aircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, M_VehicleAircraftClass, AircraftSpawnLocation);
		if (IsValid(Aircraft))
		{
			Aircraft->SetUnitSelectable(false);
			Aircraft->SetActorRotation((-M_ApproachDirection).Rotation());
			M_ActiveAircraft.Add(Aircraft);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Survival deployment: launching %s (aircraft %s) to %s."), *Drop.Unit.GetTrainingName(),
	       IsValid(Aircraft) ? *Aircraft->GetName() : TEXT("none"), *Drop.LandingLocation.ToString());
	const FVector VehicleSpawnLocation = IsValid(Aircraft)
		                                     ? AircraftSpawnLocation + FVector(
			                                     0.f, 0.f, DeploymentSettings->VehicleAttachZOffset)
		                                     : Drop.LandingLocation + FVector(0.f, 0.f, 150.f);
	++M_PendingVehicleSpawns;
	TWeakObjectPtr<USurvivalDeploymentComponent> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(Aircraft);
	const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
		Drop.Unit, VehicleSpawnLocation, this, AircraftIndex,
		[WeakThis, WeakAircraft, Drop, AircraftSpawnLocation](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			--WeakThis->M_PendingVehicleSpawns;
			WeakThis->OnVehicleSpawned(SpawnedActor, WeakAircraft.Get(), Drop, AircraftSpawnLocation);
		}, (-M_ApproachDirection).Rotation());
	if (not bRequested)
	{
		--M_PendingVehicleSpawns;
		UE_LOG(LogTemp, Error, TEXT("Survival deployment could not request %s."), *Drop.Unit.GetTrainingName());
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Survival deployment could not spawn %s."),
		                                                *Drop.Unit.GetTrainingName()));
		if (IsValid(Aircraft))
		{
			Aircraft->Destroy();
		}
	}
}

void USurvivalDeploymentComponent::OnVehicleSpawned(AActor* SpawnedActor, AAircraftMaster* Aircraft,
                                                    const FVehicleDrop& Drop, const FVector& AircraftSpawnLocation)
{
	ATankMaster* Vehicle = Cast<ATankMaster>(SpawnedActor);
	UE_LOG(LogTemp, Log, TEXT("Survival deployment: %s spawned as %s, aircraft %s."), *Drop.Unit.GetTrainingName(),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("nothing"),
	       IsValid(Aircraft) ? TEXT("valid") : TEXT("gone"));
	if (not IsValid(Vehicle) or not IsValid(Aircraft))
	{
		if (IsValid(Aircraft))
		{
			Aircraft->Destroy();
		}
		PlaceVehicleWithoutAircraft(SpawnedActor, Drop);
		return;
	}
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	FAircraftDropRequest DropRequest;
	DropRequest.State = EAircraftDropRequestState::MovingToDropLocation;
	DropRequest.PayloadType = EAircraftDropPayloadType::Tank;
	DropRequest.OwningPlayer = SurvivalDeploymentConstants::PlayerOwner;
	DropRequest.ExecuteLocation = Drop.LandingLocation;
	DropRequest.RetreatLocation = AircraftSpawnLocation + M_ApproachDirection * 5000.f;
	DropRequest.TankAttachZOffset = DeploymentSettings->VehicleAttachZOffset;
	DropRequest.HowLongStayLanded = DeploymentSettings->VehicleDropStayLandedSeconds;
	DropRequest.AttachedTank = Vehicle;
	Aircraft->OrderAircraftDrop(DropRequest);

	FCarriedVehicle Carried;
	Carried.Vehicle = Vehicle;
	Carried.Aircraft = Aircraft;
	Carried.bIsHQ = Drop.bIsHQ;
	M_CarriedVehicles.Add(Carried);
}

void USurvivalDeploymentComponent::PlaceVehicleWithoutAircraft(AActor* SpawnedActor, const FVehicleDrop& Drop)
{
	if (not IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Survival deployment: %s did not spawn."),
		                                                *Drop.Unit.GetTrainingName()));
		return;
	}
	SpawnedActor->SetActorLocation(Drop.LandingLocation + FVector(0.f, 0.f, 150.f), false, nullptr,
	                               ETeleportType::TeleportPhysics);
	FCarriedVehicle Placed;
	Placed.Vehicle = Cast<ATankMaster>(SpawnedActor);
	Placed.bIsHQ = Drop.bIsHQ;
	Placed.bHasLanded = true;
	M_CarriedVehicles.Add(Placed);
	if (Drop.bIsHQ)
	{
		ReportHQLanded(Placed.Vehicle.Get());
	}
}

void USurvivalDeploymentComponent::LaunchLandingSquadDrop(const FLandingSquadDrop& Drop)
{
	if (not IsValid(M_VehicleAircraftClass))
	{
		// No aircraft: spawn the squads on the ground right away.
		ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
		for (int32 Index = 0; Index < Drop.Squads.Num() and IsValid(AsyncSpawner); ++Index)
		{
			AsyncSpawner->AsyncSpawnOptionAtLocation(
				FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(Drop.Squads[Index])),
				Drop.LandingLocation + FVector(Index * 300.f, 0.f, 100.f), this, Index,
				[](const FTrainingOption&, AActor*, const int32)
				{
				}, FRotator::ZeroRotator);
		}
		return;
	}
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	const int32 AircraftIndex = M_LaunchedAircraftCount++;
	const FVector AircraftSpawnLocation = GetAircraftSpawnLocation(AircraftIndex);
	AAircraftMaster* Aircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, M_VehicleAircraftClass,
	                                                                           AircraftSpawnLocation);
	if (not IsValid(Aircraft))
	{
		return;
	}
	Aircraft->SetUnitSelectable(false);
	Aircraft->SetActorRotation((-M_ApproachDirection).Rotation());
	M_ActiveAircraft.Add(Aircraft);

	FAircraftDropRequest DropRequest;
	DropRequest.State = EAircraftDropRequestState::MovingToDropLocation;
	DropRequest.PayloadType = EAircraftDropPayloadType::Infantry;
	DropRequest.OwningPlayer = SurvivalDeploymentConstants::PlayerOwner;
	DropRequest.ExecuteLocation = Drop.LandingLocation;
	DropRequest.RetreatLocation = GetAircraftRetreatLocation(AircraftIndex);
	DropRequest.SquadSubtypes = Drop.Squads;
	DropRequest.HowLongStayLanded = DeploymentSettings->SquadDropStayLandedSeconds;
	DropRequest.RadiusSpawnSquads = 600.f;
	Aircraft->OrderAircraftDrop(DropRequest);
}

void USurvivalDeploymentComponent::LaunchParatrooperDrop(const FParatrooperDrop& Drop)
{
	const FSurvivalDeploymentSettings* DeploymentSettings = GetDeploymentSettings();
	ACPPController* PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not IsValid(PlayerController) or not DeploymentSettings)
	{
		return;
	}
	UGlobalAbilitiesManager* AbilitiesManager = PlayerController->GetGlobalAbilitiesManager();
	UGA_Paratroopers* Ability = nullptr;
	if (IsValid(DeploymentSettings->ParatrooperTemplate))
	{
		Ability = DuplicateObject<UGA_Paratroopers>(DeploymentSettings->ParatrooperTemplate, this);
	}
	else if (UGlobalAbilitySetDataAsset* AbilitySet = LoadObject<UGlobalAbilitySetDataAsset>(
		nullptr, SurvivalDeploymentConstants::GlobalAbilitySetPath))
	{
		Ability = Cast<UGA_Paratroopers>(
			AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(DeploymentSettings->ParatrooperAbilityType));
	}
	if (not IsValid(Ability) or not IsValid(AbilitiesManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival deployment has no paratrooper ability or ability manager; "
			"infantry spawn on the ground."));
		FLandingSquadDrop GroundDrop;
		GroundDrop.Squads = Drop.Squads;
		GroundDrop.LandingLocation = Drop.LandingLocation;
		UClass* AircraftClass = M_VehicleAircraftClass;
		M_VehicleAircraftClass = nullptr;
		LaunchLandingSquadDrop(GroundDrop);
		M_VehicleAircraftClass = AircraftClass;
		return;
	}
	const int32 AircraftIndex = M_LaunchedAircraftCount++;
	Ability->InitGlobalAbility(SurvivalDeploymentConstants::PlayerOwner, AbilitiesManager, PlayerController);
	Ability->SetCinematicSquadSubtypes(Drop.Squads);
	Ability->ExecuteCinematicDrop(Drop.LandingLocation, GetAircraftSpawnLocation(AircraftIndex));
	M_ParatrooperAbilities.Add(Ability);
}

void USurvivalDeploymentComponent::MonitorDeployment()
{
	for (FCarriedVehicle& Carried : M_CarriedVehicles)
	{
		if (Carried.bHasLanded)
		{
			continue;
		}
		ATankMaster* Vehicle = Carried.Vehicle.Get();
		if (not IsValid(Vehicle))
		{
			Carried.bHasLanded = true;
			continue;
		}
		// The aircraft detaches the vehicle when it has touched down (or the aircraft died).
		if (Vehicle->GetAttachParentActor() == nullptr)
		{
			Carried.bHasLanded = true;
			if (Carried.bIsHQ)
			{
				ReportHQLanded(Vehicle);
			}
		}
	}
	if (GetHasEverythingFinished())
	{
		FinishDeployment();
	}
}

void USurvivalDeploymentComponent::ReportHQLanded(ATankMaster* HQ)
{
	if (bM_IsReinforcement or bM_HasReportedHQLanded or not GetIsValidDirector())
	{
		return;
	}
	bM_HasReportedHQLanded = true;
	M_Director->OnHQLanded(Cast<ANomadicVehicle>(HQ));
}

bool USurvivalDeploymentComponent::GetHasEverythingFinished() const
{
	if (M_NextLaunchIndex < GetTotalLaunchCount() or M_PendingVehicleSpawns > 0)
	{
		return false;
	}
	for (const FCarriedVehicle& Carried : M_CarriedVehicles)
	{
		if (not Carried.bHasLanded)
		{
			return false;
		}
	}
	for (const TWeakObjectPtr<AAircraftMaster>& Aircraft : M_ActiveAircraft)
	{
		if (Aircraft.IsValid())
		{
			return false;
		}
	}
	for (const TObjectPtr<UGA_Paratroopers>& Ability : M_ParatrooperAbilities)
	{
		if (IsValid(Ability) and Ability->GetHasActiveDrop())
		{
			return false;
		}
	}
	return true;
}

void USurvivalDeploymentComponent::FinishDeployment()
{
	if (not bM_IsDeploying)
	{
		return;
	}
	bM_IsDeploying = false;
	UE_LOG(LogTemp, Log, TEXT("Survival deployment finished (HQ landed: %s)."),
	       bM_HasReportedHQLanded ? TEXT("yes") : TEXT("no"));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
		World->GetTimerManager().ClearTimer(M_MonitorTimerHandle);
	}
	if (bM_IsReinforcement)
	{
		// Reward drops are fire and forget.
		DestroyComponent();
		return;
	}
	if (not bM_HasReportedHQLanded)
	{
		// The HQ never reported (spawn failure); let the director continue so the run is not stuck.
		bM_HasReportedHQLanded = true;
		if (GetIsValidDirector())
		{
			M_Director->OnHQLanded(nullptr);
		}
	}
	if (GetIsValidDirector())
	{
		M_Director->OnDeploymentFinished();
	}
}

bool USurvivalDeploymentComponent::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Director", "GetIsValidDirector", GetOwner());
	return false;
}

const FSurvivalDeploymentSettings* USurvivalDeploymentComponent::GetDeploymentSettings() const
{
	if (not M_Director.IsValid() or not IsValid(M_Director->GetSettings()))
	{
		return nullptr;
	}
	return &M_Director->GetSettings()->Deployment;
}
