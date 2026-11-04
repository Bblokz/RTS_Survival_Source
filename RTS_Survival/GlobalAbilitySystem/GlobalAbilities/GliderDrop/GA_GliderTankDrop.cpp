// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_GliderTankDrop.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UGA_GliderTankDrop::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	const FVector Direction = GetDirectionForPointExecution(TargetLocation);
	ExecuteAbilityAlongLine(TargetLocation - Direction * M_AimSettings.AreaAim.MaxStripLength, TargetLocation);
}

void UGA_GliderTankDrop::ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd)
{
	ResetAbilityEndedBroadcast();
	StartDelivery(LineStart, LineEnd);
	Super::ExecuteAbilityAtLocation(LineEnd);
}

void UGA_GliderTankDrop::GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
                                            TArray<FVector>& OutDropPoints) const
{
	// The skid ends at the second click; that is where the tank comes out.
	OutDropPoints.Add(GetGroundLocation(LineEnd));
}

void UGA_GliderTankDrop::BeginDestroy()
{
	M_LoadHandles.Reset();
	Super::BeginDestroy();
}

void UGA_GliderTankDrop::StartDelivery(const FVector& Touchdown, const FVector& Stop)
{
	const FVector Run(Stop.X - Touchdown.X, Stop.Y - Touchdown.Y, 0.f);
	if (not GetIsValidGlobalAbilityManager() || Run.IsNearlyZero() || M_GliderDropSettings.DeliveryClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_GliderTankDrop has no manager, landing run or delivery class."));
		BroadcastDropFinishedIfNeeded();
		return;
	}
	if (M_GliderDropSettings.TankSubtype == ETankSubtype::Tank_None)
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_GliderTankDrop has no tank subtype configured."));
		BroadcastDropFinishedIfNeeded();
		return;
	}

	++M_ActiveDeliveries;
	const FVector GroundTouchdown = GetGroundLocation(Touchdown);
	const FVector GroundStop = GetGroundLocation(Stop);
	TWeakObjectPtr<UGA_GliderTankDrop> WeakThis(this);
	M_LoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		M_GliderDropSettings.DeliveryClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis, GroundTouchdown, GroundStop]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnDeliveryClassLoaded(GroundTouchdown, GroundStop);
			}
		})));
}

void UGA_GliderTankDrop::OnDeliveryClassLoaded(const FVector Touchdown, const FVector Stop)
{
	UClass* DeliveryClass = M_GliderDropSettings.DeliveryClass.Get();
	UWorld* World = GetWorld();
	if (not IsValid(DeliveryClass) || not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_GliderTankDrop failed to load its delivery class."));
		M_ActiveDeliveries = FMath::Max(0, M_ActiveDeliveries - 1);
		BroadcastDropFinishedIfNeeded();
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGliderTankDelivery* Delivery = World->SpawnActor<AGliderTankDelivery>(
		DeliveryClass, Touchdown, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Delivery))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_GliderTankDrop failed to spawn its delivery."));
		M_ActiveDeliveries = FMath::Max(0, M_ActiveDeliveries - 1);
		BroadcastDropFinishedIfNeeded();
		return;
	}

	FGliderDeliveryOrder Order;
	Order.TouchdownLocation = Touchdown;
	Order.StopLocation = Stop;
	Order.ApproachDistance = GetApproachDistance(Touchdown, (Stop - Touchdown).GetSafeNormal2D());
	Order.OwningPlayer = GetOwningPlayer();
	Order.TankOption = FTrainingOption(EAllUnitType::UNType_Tank,
	                                   static_cast<uint8>(M_GliderDropSettings.TankSubtype));
	Order.TankClassOverride = M_GliderDropSettings.TankClassOverride;
	Delivery->OnTankDelivered.AddUObject(this, &UGA_GliderTankDrop::OnTankDelivered);
	Delivery->StartDelivery(Order);
}

FVector UGA_GliderTankDrop::GetDirectionForPointExecution(const FVector& TargetLocation) const
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return FVector::ForwardVector;
	}
	const FVector SpawnLocation = GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, TargetLocation);
	const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? FVector::ForwardVector : Direction;
}

FVector UGA_GliderTankDrop::GetGroundLocation(const FVector& Location) const
{
	FVector GroundLocation;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Location, GroundLocation);
	return GroundLocation;
}

float UGA_GliderTankDrop::GetApproachDistance(const FVector& Touchdown, const FVector& Direction) const
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return M_GliderDropSettings.MaxApproachDistance;
	}
	// The manager clamps the approach to the map edge; only its horizontal distance is used.
	const FVector Start = GetGlobalAbilityManager()->GetStraightFlightSpawnLocation(
		Touchdown, Direction, M_GliderDropSettings.MaxApproachDistance);
	return FVector::Dist2D(Start, Touchdown);
}

void UGA_GliderTankDrop::OnTankDelivered(AGliderTankDelivery* Delivery, ATankMaster* Tank)
{
	if (IsValid(Delivery))
	{
		Delivery->OnTankDelivered.RemoveAll(this);
	}
	M_ActiveDeliveries = FMath::Max(0, M_ActiveDeliveries - 1);
	BroadcastDropFinishedIfNeeded();
}

void UGA_GliderTankDrop::BroadcastDropFinishedIfNeeded()
{
	if (M_ActiveDeliveries > 0)
	{
		return;
	}
	M_LoadHandles.Reset();
	BroadcastAbilityEnded();
}
