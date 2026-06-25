// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AircraftDrop.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace
{
	constexpr float AircraftDropSpacing = 450.0f;
	constexpr int32 AircraftFormationWidth = 2;
	constexpr int32 MaxAircraftFormationProjectionAttempts = 64;
	constexpr float AircraftDropProjectionScale = 7.0f;
	constexpr float AircraftExecuteLocationDuplicateToleranceSquared = 1.0f;
}

void UGA_AircraftDrop::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetAbilityEndedBroadcast();

	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	M_ActiveAircraftCount = 0;

	const TSoftClassPtr<AAircraftMaster> AircraftClassToLoad = M_AircraftDropSettings.AircraftClass;
	if (AircraftClassToLoad.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftDrop has no aircraft class configured."));
		BroadcastAbilityEnded();
		return;
	}

	TWeakObjectPtr<UGA_AircraftDrop> WeakThis(this);
	M_AircraftClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AircraftClassToLoad.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis, AircraftClassToLoad, TargetLocation]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->OnAircraftClassLoaded(AircraftClassToLoad, TargetLocation);
		}));
	Super::ExecuteAbilityAtLocation(TargetLocation);
	
}

void UGA_AircraftDrop::BeginDestroy()
{
	M_AircraftClassLoadHandle.Reset();
	Super::BeginDestroy();
}

void UGA_AircraftDrop::OnAircraftClassLoaded(
	const TSoftClassPtr<AAircraftMaster> AircraftClassToLoad,
	const FVector TargetLocation)
{
	M_AircraftClassLoadHandle.Reset();
	UClass* AircraftClass = AircraftClassToLoad.Get();
	if (not IsValid(AircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftDrop failed to async load a valid aircraft class."));
		BroadcastDropFinishedIfNeeded();
		return;
	}

	int32 AircraftIndex = 0;
	TArray<FVector> UsedExecuteLocations;
	for (const FAircraftDropTankWithOffset& TankWithOffset : M_AircraftDropSettings.TanksWithOffsets)
	{
		if (TankWithOffset.TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		const FVector ExecuteLocation = BuildAircraftExecuteLocation(TargetLocation, AircraftIndex, UsedExecuteLocations);
		SpawnTankDropAircraft(AircraftClass, ExecuteLocation, TankWithOffset, AircraftIndex);
		++AircraftIndex;
	}

	const int32 InfantryPerAircraft = FMath::Max(1, M_AircraftDropSettings.AmountInfantryPerAircraft);
	for (int32 SquadIndex = 0;
		SquadIndex < M_AircraftDropSettings.SquadSubtypes.Num();
		SquadIndex += InfantryPerAircraft)
	{
		TArray<ESquadSubtype> SquadsForAircraft;
		const int32 LastSquadIndex = FMath::Min(
			SquadIndex + InfantryPerAircraft,
			M_AircraftDropSettings.SquadSubtypes.Num());
		for (int32 InnerSquadIndex = SquadIndex; InnerSquadIndex < LastSquadIndex; ++InnerSquadIndex)
		{
			if (M_AircraftDropSettings.SquadSubtypes[InnerSquadIndex] != ESquadSubtype::Squad_None)
			{
				SquadsForAircraft.Add(M_AircraftDropSettings.SquadSubtypes[InnerSquadIndex]);
			}
		}

		if (SquadsForAircraft.IsEmpty())
		{
			continue;
		}

		const FVector ExecuteLocation = BuildAircraftExecuteLocation(TargetLocation, AircraftIndex, UsedExecuteLocations);
		SpawnSquadDropAircraft(AircraftClass, ExecuteLocation, SquadsForAircraft, AircraftIndex);
		++AircraftIndex;
	}

	BroadcastDropFinishedIfNeeded();
}

void UGA_AircraftDrop::SpawnTankDropAircraft(
	UClass* AircraftClass,
	const FVector& ExecuteLocation,
	const FAircraftDropTankWithOffset& TankWithOffset,
	const int32 AircraftIndex)
{
	FAircraftDropRequest DropRequest = BuildBaseDropRequest(
		ExecuteLocation,
		EAircraftDropPayloadType::Tank,
		AircraftIndex);
	DropRequest.TankAttachZOffset = TankWithOffset.TankAttachZOffset;

	const FVector SpawnLocation = BuildAircraftSpawnLocation(ExecuteLocation, AircraftIndex);
	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this,
		AircraftClass,
		SpawnLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

	++M_ActiveAircraftCount;
	SpawnedAircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_AircraftDrop::OnSpawnedAircraftDestroyed);
	SpawnedAircraft->SetUnitSelectable(false);

	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftDrop failed to find ARTSAsyncSpawner for tank drop."));
		SpawnedAircraft->Destroy();
		return;
	}

	const FTrainingOption TankTrainingOption(
		EAllUnitType::UNType_Tank,
		static_cast<uint8>(TankWithOffset.TankSubtype));
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(SpawnedAircraft);
	AsyncSpawner->AsyncSpawnOptionAtLocation(
		TankTrainingOption,
		SpawnLocation + FVector(0.0f, 0.0f, TankWithOffset.TankAttachZOffset),
		const_cast<UGA_AircraftDrop*>(this),
		AircraftIndex,
		[WeakAircraft, DropRequest](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (not WeakAircraft.IsValid())
			{
				return;
			}

			ATankMaster* SpawnedTank = Cast<ATankMaster>(SpawnedActor);
			if (not IsValid(SpawnedTank))
			{
				WeakAircraft->Destroy();
				return;
			}

			FAircraftDropRequest RequestWithTank = DropRequest;
			RequestWithTank.AttachedTank = SpawnedTank;
			WeakAircraft->OrderAircraftDrop(RequestWithTank);
		},
		FRotator::ZeroRotator);
}

void UGA_AircraftDrop::SpawnSquadDropAircraft(
	UClass* AircraftClass,
	const FVector& ExecuteLocation,
	const TArray<ESquadSubtype>& Squads,
	const int32 AircraftIndex)
{
	FAircraftDropRequest DropRequest = BuildBaseDropRequest(
		ExecuteLocation,
		EAircraftDropPayloadType::Infantry,
		AircraftIndex);
	DropRequest.SquadSubtypes = Squads;

	const FVector SpawnLocation = BuildAircraftSpawnLocation(ExecuteLocation, AircraftIndex);
	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this,
		AircraftClass,
		SpawnLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}
	++M_ActiveAircraftCount;
	SpawnedAircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_AircraftDrop::OnSpawnedAircraftDestroyed);
	SpawnedAircraft->SetUnitSelectable(false);
	SpawnedAircraft->OrderAircraftDrop(DropRequest);
}

FAircraftDropRequest UGA_AircraftDrop::BuildBaseDropRequest(
	const FVector& TargetLocation,
	const EAircraftDropPayloadType PayloadType,
	const int32 AircraftIndex) const
{
	FAircraftDropRequest DropRequest;
	DropRequest.State = EAircraftDropRequestState::MovingToDropLocation;
	DropRequest.PayloadType = PayloadType;
	DropRequest.ExecuteLocation = TargetLocation;
	DropRequest.RetreatLocation = GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
	DropRequest.RetreatLocation.Y += AircraftDropSpacing * static_cast<float>(AircraftIndex);
	DropRequest.HowLongStayLanded = PayloadType == EAircraftDropPayloadType::Tank
		? M_AircraftDropSettings.HowLongStayLandedTankDrop
		: M_AircraftDropSettings.HowLongStayLanded;
	DropRequest.RadiusSpawnSquads = M_AircraftDropSettings.RadiusSpawnSquads;
	DropRequest.DropOffSound = M_AircraftDropSettings.DropOffSound;
	DropRequest.DropOffSoundAttenuation = M_AircraftDropSettings.DropOffSoundAttenuation;
	DropRequest.DropOffSoundConcurrency = M_AircraftDropSettings.DropOffSoundConcurrency;
	return DropRequest;
}

FVector UGA_AircraftDrop::BuildAircraftSpawnLocation(const FVector& TargetLocation, const int32 AircraftIndex) const
{
	FVector SpawnLocation = GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, TargetLocation);
	SpawnLocation.Y += AircraftDropSpacing * static_cast<float>(AircraftIndex);
	return SpawnLocation;
}

FVector UGA_AircraftDrop::BuildAircraftExecuteLocation(
	const FVector& TargetLocation,
	const int32 AircraftIndex,
	TArray<FVector>& UsedExecuteLocations) const
{
	if (AircraftIndex == 0)
	{
		UsedExecuteLocations.Add(TargetLocation);
		return TargetLocation;
	}

	const int32 FirstFormationIndex = AircraftIndex - 1;
	for (int32 AttemptIndex = 0; AttemptIndex < MaxAircraftFormationProjectionAttempts; ++AttemptIndex)
	{
		const FVector CandidateLocation = BuildAircraftRectangleFormationLocation(
			TargetLocation,
			FirstFormationIndex + AttemptIndex);
		bool bProjectionSucceeded = false;
		const FVector ProjectedLocation = RTSFunctionLibrary::GetLocationProjected(
			this,
			CandidateLocation,
			true,
			bProjectionSucceeded,
			AircraftDropProjectionScale);

		if (not bProjectionSucceeded || GetIsExecuteLocationAlreadyUsed(ProjectedLocation, UsedExecuteLocations))
		{
			continue;
		}

		UsedExecuteLocations.Add(ProjectedLocation);
		return ProjectedLocation;
	}

	RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftDrop failed to project a unique aircraft drop location."));
	UsedExecuteLocations.Add(TargetLocation);
	return TargetLocation;
}

FVector UGA_AircraftDrop::BuildAircraftRectangleFormationLocation(
	const FVector& TargetLocation,
	const int32 FormationIndex) const
{
	const float BetweenAircraftOffset = M_AircraftDropSettings.BetweenAircraftRectangleOffset;
	const int32 RowIndex = FormationIndex / AircraftFormationWidth + 1;
	const int32 ColumnIndex = FormationIndex % AircraftFormationWidth;
	const float ColumnSide = ColumnIndex == 0 ? -1.0f : 1.0f;

	FVector FormationLocation = TargetLocation;
	FormationLocation.X += static_cast<float>(RowIndex) * BetweenAircraftOffset;
	FormationLocation.Y += ColumnSide * BetweenAircraftOffset * 0.5f;
	return FormationLocation;
}

bool UGA_AircraftDrop::GetIsExecuteLocationAlreadyUsed(
	const FVector& ExecuteLocation,
	const TArray<FVector>& UsedExecuteLocations) const
{
	for (const FVector& UsedExecuteLocation : UsedExecuteLocations)
	{
		if (FVector::DistSquared2D(ExecuteLocation, UsedExecuteLocation) <= AircraftExecuteLocationDuplicateToleranceSquared)
		{
			return true;
		}
	}

	return false;
}

void UGA_AircraftDrop::OnSpawnedAircraftDestroyed(AActor* DestroyedActor)
{
	M_ActiveAircraftCount = FMath::Max(0, M_ActiveAircraftCount - 1);
	BroadcastDropFinishedIfNeeded();
}

void UGA_AircraftDrop::BroadcastDropFinishedIfNeeded()
{
	if (M_AircraftClassLoadHandle.IsValid())
	{
		return;
	}

	if (M_ActiveAircraftCount > 0)
	{
		return;
	}

	BroadcastAbilityEnded();
}
