// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_BuildingParachuteDrop.h"

#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace BuildingParachuteDropConstants
{
	// Components that affected navigation before the fall get it back on landing; marked with this tag meanwhile.
	const FName AirborneNavigationTag(TEXT("RTS_BuildingDrop_NavigationOff"));
}

void UGA_BuildingParachuteDrop::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetAbilityEndedBroadcast();
	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}
	if (M_BuildingDropSettings.AircraftClass.IsNull() || M_BuildingDropSettings.Payloads.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_BuildingParachuteDrop has no aircraft class or no payloads."));
		BroadcastAbilityEnded();
		return;
	}

	FBuildingDropRun NewRun;
	NewRun.RunId = M_NextRunId++;
	NewRun.TargetLocation = TargetLocation;
	const int32 RunId = NewRun.RunId;
	M_DropRuns.Add(MoveTemp(NewRun));

	// The buildings load with the aircraft so they can be built the moment they leave it.
	TArray<FSoftObjectPath> AssetsToLoad;
	AssetsToLoad.Add(M_BuildingDropSettings.AircraftClass.ToSoftObjectPath());
	for (const FBuildingParachuteDropPayload& Payload : M_BuildingDropSettings.Payloads)
	{
		if (not Payload.BuildingClass.IsNull())
		{
			AssetsToLoad.AddUnique(Payload.BuildingClass.ToSoftObjectPath());
		}
	}
	TWeakObjectPtr<UGA_BuildingParachuteDrop> WeakThis(this);
	M_LoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateLambda([WeakThis, RunId]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded(RunId);
			}
		})));
	Super::ExecuteAbilityAtLocation(TargetLocation);
}

void UGA_BuildingParachuteDrop::BeginDestroy()
{
	M_LoadHandles.Reset();
	if (UWorld* World = GetWorld())
	{
		for (FBuildingDropRun& Run : M_DropRuns)
		{
			World->GetTimerManager().ClearTimer(Run.DropTimerHandle);
		}
	}
	M_DropRuns.Reset();
	Super::BeginDestroy();
}

UGA_BuildingParachuteDrop::FBuildingDropRun* UGA_BuildingParachuteDrop::FindRun(const int32 RunId)
{
	return M_DropRuns.FindByPredicate([RunId](const FBuildingDropRun& Run)
	{
		return Run.RunId == RunId;
	});
}

void UGA_BuildingParachuteDrop::OnAssetsLoaded(const int32 RunId)
{
	UClass* AircraftClass = M_BuildingDropSettings.AircraftClass.Get();
	FBuildingDropRun* Run = FindRun(RunId);
	if (not Run || not IsValid(AircraftClass) || not GetIsValidGlobalAbilityManager())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_BuildingParachuteDrop failed to load a valid aircraft class."));
		FinishRunIfDone(RunId);
		return;
	}

	FVector StartLocation = GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, Run->TargetLocation);
	StartLocation.Z = FMath::Max(StartLocation.Z,
	                             Run->TargetLocation.Z + M_BuildingDropSettings.MinFlightHeightAboveTarget);
	Run->PassOverLocation = FVector(Run->TargetLocation.X, Run->TargetLocation.Y, StartLocation.Z);
	const float ExitDistance = M_BuildingDropSettings.ExitDistanceAfterTarget > 0.0f
		                           ? M_BuildingDropSettings.ExitDistanceAfterTarget
		                           : FVector::Dist2D(StartLocation, Run->PassOverLocation);
	const FVector EndLocation = GlobalAbilityHelpers::BuildAircraftRunEndLocationFromDirection(
		Run->PassOverLocation, StartLocation, Run->PassOverLocation, ExitDistance);
	Run->RunDirection = (EndLocation - StartLocation).GetSafeNormal2D();

	AAircraftMaster* Aircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, AircraftClass, StartLocation);
	if (not IsValid(Aircraft))
	{
		FinishRunIfDone(RunId);
		return;
	}
	Run->Aircraft = Aircraft;
	Aircraft->SetActorRotation(Run->RunDirection.Rotation());
	Aircraft->SetUnitSelectable(false);
	Aircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_BuildingParachuteDrop::OnSpawnedAircraftDestroyed);
	Aircraft->OnStraightRunPassedPoint.AddWeakLambda(this, [this, RunId](AAircraftMaster*)
	{
		OnAircraftOverTarget(RunId);
	});

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<UGA_BuildingParachuteDrop> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(Aircraft);
	const FVector PassOverLocation = Run->PassOverLocation;
	// Same as the other aircraft abilities: the aircraft finishes its BeginPlay setup before it gets an order.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakThis, WeakAircraft, PassOverLocation, EndLocation]()
		{
			if (not WeakThis.IsValid() || not WeakAircraft.IsValid())
			{
				return;
			}
			WeakAircraft->OrderStraightFlightRun(PassOverLocation, EndLocation);
			WeakAircraft->SetStraightRunHoldAtEnd(true);
		}));
}

void UGA_BuildingParachuteDrop::OnAircraftOverTarget(const int32 RunId)
{
	FBuildingDropRun* Run = FindRun(RunId);
	UWorld* World = GetWorld();
	if (not Run || Run->bM_HasReachedTarget || not IsValid(World))
	{
		return;
	}
	Run->bM_HasReachedTarget = true;
	OnDropTimer(RunId);
	Run = FindRun(RunId);
	if (not Run)
	{
		return;
	}
	TWeakObjectPtr<UGA_BuildingParachuteDrop> WeakThis(this);
	World->GetTimerManager().SetTimer(
		Run->DropTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis, RunId]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnDropTimer(RunId);
			}
		}),
		M_BuildingDropSettings.DropInterval,
		true);
}

void UGA_BuildingParachuteDrop::OnDropTimer(const int32 RunId)
{
	FBuildingDropRun* Run = FindRun(RunId);
	if (not Run)
	{
		return;
	}
	if (M_BuildingDropSettings.Payloads.IsValidIndex(Run->NextPayloadIndex))
	{
		const FBuildingParachuteDropPayload Payload = M_BuildingDropSettings.Payloads[Run->NextPayloadIndex];
		++Run->NextPayloadIndex;
		DropPayload(*Run, Payload);
		Run = FindRun(RunId);
	}
	if (not Run)
	{
		return;
	}
	ReleaseAircraftIfAllDropped(*Run);
	FinishRunIfDone(RunId);
}

void UGA_BuildingParachuteDrop::DropPayload(FBuildingDropRun& Run, const FBuildingParachuteDropPayload& Payload)
{
	UClass* BuildingClass = Payload.BuildingClass.LoadSynchronous();
	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, Run.RunDirection).GetSafeNormal();
	const FVector DesiredLanding = Run.TargetLocation + Run.RunDirection * Payload.LandingOffset.X +
		RightDirection * Payload.LandingOffset.Y;
	FVector GroundLocation;
	if (not IsValid(BuildingClass) || not GlobalAbilityHelpers::TryFindGroundLocation(this, DesiredLanding, GroundLocation))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_BuildingParachuteDrop skipped a payload: no building class or no "
			"landscape under its landing spot."));
		return;
	}
	const FRotator LandingRotation(0.0f, Run.RunDirection.Rotation().Yaw + Payload.YawOffset, 0.0f);
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	const FVector DropFrom = IsValid(Aircraft) ? Aircraft->GetActorLocation() : Run.PassOverLocation;
	const FVector StartLocation = DropFrom - FVector(0.0f, 0.0f, M_BuildingDropSettings.DropBelowAircraftOffset);

	ABuildingExpansion* Expansion = SpawnBuiltExpansion(BuildingClass, StartLocation, LandingRotation);
	if (not IsValid(Expansion))
	{
		return;
	}
	M_DroppedBuildings.Add(Expansion);
	++Run.LoadsInAir;
	UE_LOG(LogTemp, Display, TEXT("RTS_BUILDING_DROP run=%d payload=%s start=%s landing=%s canopies=%d"),
	       Run.RunId, *Expansion->GetName(), *StartLocation.ToString(), *GroundLocation.ToString(),
	       Payload.Descent.CanopyCount);

	UCargoParachuteDescentComponent* DescentComponent = NewObject<UCargoParachuteDescentComponent>(Expansion);
	DescentComponent->RegisterComponent();
	const int32 RunId = Run.RunId;
	DescentComponent->OnDescentFinished.BindWeakLambda(this, [this, RunId](AActor* Load, const bool bLanded)
	{
		OnPayloadLanded(RunId, Load, bLanded);
	});
	// May finish synchronously and re-enter through OnPayloadLanded: Run is not used after.
	DescentComponent->StartDescent(
		Payload.Descent,
		M_BuildingDropSettings.CanopyMesh,
		StartLocation,
		FTransform(LandingRotation, GroundLocation));
}

ABuildingExpansion* UGA_BuildingParachuteDrop::SpawnBuiltExpansion(
	UClass* BuildingClass,
	const FVector& Location,
	const FRotator& Rotation) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not BuildingClass->IsChildOf(ABuildingExpansion::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_BuildingParachuteDrop: payload class is not a building expansion."));
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingExpansion* Expansion = World->SpawnActor<ABuildingExpansion>(
		BuildingClass, Location, Rotation, SpawnParameters);
	if (not IsValid(Expansion))
	{
		return nullptr;
	}
	if (URTSComponent* RTSComponent = Expansion->GetRTSComponent())
	{
		const uint8 Owner = static_cast<uint8>(GetOwningPlayer());
		if (RTSComponent->GetOwningPlayer() != Owner)
		{
			RTSComponent->SetOwningPlayerRuntime(Owner);
		}
	}
	// The standalone path the HQ's field defenses use; finished at once, so it leaves the aircraft complete.
	Expansion->StartFieldDefenseConstruction(Location, Rotation);
	Expansion->FinishFieldDefenseConstruction();
	SetExpansionAirborne(*Expansion, true);
	return Expansion;
}

void UGA_BuildingParachuteDrop::SetExpansionAirborne(ABuildingExpansion& Expansion, const bool bAirborne) const
{
	using namespace BuildingParachuteDropConstants;
	Expansion.SetActorEnableCollision(not bAirborne);
	TArray<AActor*> Parts;
	Expansion.GetAttachedActors(Parts, true, true);
	Parts.Add(&Expansion);
	for (AActor* Part : Parts)
	{
		for (UActorComponent* Component : Part->GetComponents())
		{
			if (not IsValid(Component))
			{
				continue;
			}
			// A falling building must not carve the navmesh along its way down.
			if (bAirborne && Component->CanEverAffectNavigation())
			{
				Component->ComponentTags.AddUnique(AirborneNavigationTag);
				Component->SetCanEverAffectNavigation(false);
				continue;
			}
			if (not bAirborne && Component->ComponentTags.Contains(AirborneNavigationTag))
			{
				Component->ComponentTags.Remove(AirborneNavigationTag);
				Component->SetCanEverAffectNavigation(true);
			}
		}
	}
	if (bAirborne)
	{
		return;
	}
	// Same as a finished construction: turrets engage on their own again.
	for (ACPPTurretsMaster* Turret : Expansion.GetTurrets())
	{
		if (IsValid(Turret))
		{
			Turret->SetAutoEngageTargets(false);
		}
	}
}

void UGA_BuildingParachuteDrop::OnPayloadLanded(const int32 RunId, AActor* Load, const bool bLanded)
{
	ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(Load);
	if (bLanded && IsValid(Expansion))
	{
		SetExpansionAirborne(*Expansion, false);
		UE_LOG(LogTemp, Display, TEXT("RTS_BUILDING_DROP landed=%s at=%s"), *Expansion->GetName(),
		       *Expansion->GetActorLocation().ToString());
	}
	if (FBuildingDropRun* Run = FindRun(RunId))
	{
		Run->LoadsInAir = FMath::Max(0, Run->LoadsInAir - 1);
	}
	FinishRunIfDone(RunId);
}

void UGA_BuildingParachuteDrop::ReleaseAircraftIfAllDropped(const FBuildingDropRun& Run) const
{
	AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(Aircraft) || not Run.bM_HasReachedTarget ||
		Run.NextPayloadIndex < M_BuildingDropSettings.Payloads.Num())
	{
		return;
	}
	Aircraft->SetStraightRunHoldAtEnd(false);
}

void UGA_BuildingParachuteDrop::FinishRunIfDone(const int32 RunId)
{
	const int32 RunIndex = M_DropRuns.IndexOfByPredicate([RunId](const FBuildingDropRun& Run)
	{
		return Run.RunId == RunId;
	});
	if (M_DropRuns.IsValidIndex(RunIndex))
	{
		const FBuildingDropRun& Run = M_DropRuns[RunIndex];
		const bool bAllDropped = Run.NextPayloadIndex >= M_BuildingDropSettings.Payloads.Num();
		// A run whose aircraft is gone before the target drops nothing more.
		const bool bNothingMoreToDrop = bAllDropped || (not Run.Aircraft.IsValid() && not Run.bM_HasReachedTarget);
		if (Run.Aircraft.IsValid() || Run.LoadsInAir > 0 || not bNothingMoreToDrop)
		{
			return;
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_DropRuns[RunIndex].DropTimerHandle);
		}
		M_DropRuns.RemoveAt(RunIndex);
	}
	if (M_DropRuns.IsEmpty())
	{
		BroadcastAbilityEnded();
	}
}

void UGA_BuildingParachuteDrop::OnSpawnedAircraftDestroyed(AActor* DestroyedActor)
{
	TArray<int32> RunIds;
	for (FBuildingDropRun& Run : M_DropRuns)
	{
		if (Run.Aircraft.Get() == DestroyedActor)
		{
			// The weak pointer still resolves during OnDestroyed.
			Run.Aircraft.Reset();
			RunIds.Add(Run.RunId);
		}
	}
	for (const int32 RunId : RunIds)
	{
		FinishRunIfDone(RunId);
	}
}
