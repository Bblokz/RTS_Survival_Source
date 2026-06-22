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
}

void UGA_AircraftDrop::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const TSoftClassPtr<AAircraftMaster> AircraftClassToLoad = M_AircraftDropSettings.AircraftClass;
	if (AircraftClassToLoad.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftDrop has no aircraft class configured."));
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
		return;
	}

	int32 AircraftIndex = 0;
	for (const FAircraftDropTankWithOffset& TankWithOffset : M_AircraftDropSettings.TanksWithOffsets)
	{
		if (TankWithOffset.TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		SpawnTankDropAircraft(AircraftClass, TargetLocation, TankWithOffset, AircraftIndex);
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

		SpawnSquadDropAircraft(AircraftClass, TargetLocation, SquadsForAircraft, AircraftIndex);
		++AircraftIndex;
	}
}

void UGA_AircraftDrop::SpawnTankDropAircraft(
	UClass* AircraftClass,
	const FVector& TargetLocation,
	const FAircraftDropTankWithOffset& TankWithOffset,
	const int32 AircraftIndex) const
{
	FAircraftDropRequest DropRequest = BuildBaseDropRequest(
		TargetLocation,
		EAircraftDropPayloadType::Tank,
		AircraftIndex);
	DropRequest.TankAttachZOffset = TankWithOffset.TankAttachZOffset;

	const FVector SpawnLocation = BuildAircraftSpawnLocation(TargetLocation, AircraftIndex);
	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this,
		AircraftClass,
		SpawnLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

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
	const FVector& TargetLocation,
	const TArray<ESquadSubtype>& Squads,
	const int32 AircraftIndex) const
{
	FAircraftDropRequest DropRequest = BuildBaseDropRequest(
		TargetLocation,
		EAircraftDropPayloadType::Infantry,
		AircraftIndex);
	DropRequest.SquadSubtypes = Squads;

	const FVector SpawnLocation = BuildAircraftSpawnLocation(TargetLocation, AircraftIndex);
	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this,
		AircraftClass,
		SpawnLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

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
