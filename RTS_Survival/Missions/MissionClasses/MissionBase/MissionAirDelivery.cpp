// Copyright (C) Bas Blokzijl - All rights reserved.

// The air delivery part of UMissionBase: nomadic trucks flown in and built on the spot.

#include "MissionBase.h"

#include "RTS_Survival/Missions/NomadicAirDelivery/NomadicAirDeliverySubsystem.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UMissionBase::DeliverPlayerNomadicByCargoPlane(const ENomadicSubtype NomadicSubtype, const FVector BuildLocation,
                                                    const FRotator BuildRotation, const int32 ID)
{
	FNomadicAirDeliveryOrder Order;
	Order.Method = ENomadicAirDeliveryMethod::CargoPlaneRopes;
	Order.NomadicSubtype = NomadicSubtype;
	Order.BuildLocation = BuildLocation;
	Order.BuildRotation = BuildRotation;
	Order.Settings = AirDeliverySettings;
	AirDelivery_Start(Order, ID);
}

void UMissionBase::DeliverPlayerHQByMammutGlider(const FVector BuildLocation, const FRotator BuildRotation,
                                                 const FVector GliderTouchdownLocation, const int32 ID)
{
	FNomadicAirDeliveryOrder Order;
	Order.Method = ENomadicAirDeliveryMethod::MammutGlider;
	Order.NomadicSubtype = ENomadicSubtype::Nomadic_GerHq;
	Order.BuildLocation = BuildLocation;
	Order.BuildRotation = BuildRotation;
	Order.GliderTouchdownLocation = GliderTouchdownLocation;
	Order.Settings = AirDeliverySettings;
	AirDelivery_Start(Order, ID);
}

void UMissionBase::AirDelivery_Start(const FNomadicAirDeliveryOrder& Order, const int32 ID)
{
	UWorld* World = GetWorld();
	UNomadicAirDeliverySubsystem* DeliverySubsystem = IsValid(World)
		                                                  ? World->GetSubsystem<UNomadicAirDeliverySubsystem>()
		                                                  : nullptr;
	if (not IsValid(DeliverySubsystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("Mission found no UNomadicAirDeliverySubsystem for an air delivery."));
		return;
	}

	const bool bIsHQDelivery = Order.NomadicSubtype == ENomadicSubtype::Nomadic_GerHq
		|| Order.NomadicSubtype == ENomadicSubtype::Nomadic_RusHq;
	const ENomadicSubtype NomadicSubtype = Order.NomadicSubtype;
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	const FOnNomadicAirDeliveryLanded OnLanded = FOnNomadicAirDeliveryLanded::CreateLambda(
		[WeakThis, NomadicSubtype, bIsHQDelivery, ID](ANomadicVehicle* NomadicVehicle)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AirDelivery_OnLanded(NomadicVehicle, NomadicSubtype, bIsHQDelivery, ID);
			}
		});
	if (bIsHQDelivery)
	{
		// Counted before the start: a delivery that fails at once reports back inside StartDelivery.
		++M_PendingAirDeliveredHQCount;
	}
	const int32 DeliveryId = DeliverySubsystem->StartDelivery(Order, OnLanded);
	if (DeliveryId == INDEX_NONE && bIsHQDelivery)
	{
		M_PendingAirDeliveredHQCount = FMath::Max(0, M_PendingAirDeliveredHQCount - 1);
	}
}

void UMissionBase::AirDelivery_OnLanded(ANomadicVehicle* NomadicVehicle, const ENomadicSubtype NomadicSubtype,
                                        const bool bIsHQDelivery, const int32 ID)
{
	if (bIsHQDelivery)
	{
		M_PendingAirDeliveredHQCount = FMath::Max(0, M_PendingAirDeliveredHQCount - 1);
	}
	// The next mission starts before the truck unfolds, so its callbacks on the HQ conversion are registered in time.
	if (bM_IsNextMissionWaitingForAirDeliveredHQ && M_PendingAirDeliveredHQCount == 0)
	{
		bM_IsNextMissionWaitingForAirDeliveredHQ = false;
		if (not IsValid(NomadicVehicle))
		{
			RTSFunctionLibrary::ReportError(TEXT("The air-delivered HQ failed; the next mission starts without it."));
		}
		ActivateNextMissionIfConfigured();
	}
	BP_OnAirDeliveredNomadicLanded(NomadicVehicle, NomadicSubtype, ID);
}
