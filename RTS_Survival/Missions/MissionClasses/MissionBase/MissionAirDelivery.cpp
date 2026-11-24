// Copyright (C) Bas Blokzijl - All rights reserved.

// The air delivery part of UMissionBase: nomadic trucks flown in and built on the spot.

#include "MissionBase.h"

#include "RTS_Survival/Missions/NomadicAirDelivery/NomadicAirDeliverySubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MissionAirDeliveryPrivate
{
	constexpr float HQExitPollSeconds = 0.25f;
	// Released HQ trucks normally drive out in about 10 s; past this the event fires anyway.
	constexpr float HQExitTimeoutSeconds = 30.f;
}

void UMissionBase::DeliverPlayerNomadicByCargoPlane(const ENomadicSubtype NomadicSubtype, const FVector BuildLocation,
                                                    const FRotator BuildRotation, const int32 ID,
                                                    const bool bBuildOnLanding)
{
	FNomadicAirDeliveryOrder Order;
	Order.Method = ENomadicAirDeliveryMethod::CargoPlaneRopes;
	Order.NomadicSubtype = NomadicSubtype;
	Order.BuildLocation = BuildLocation;
	Order.BuildRotation = BuildRotation;
	Order.bBuildOnLanding = bBuildOnLanding;
	Order.Settings = AirDeliverySettings;
	AirDelivery_Start(Order, ID);
}

void UMissionBase::DeliverPlayerHQByMammutGlider(const FVector BuildLocation, const FRotator BuildRotation,
                                                 const FVector GliderTouchdownLocation, const int32 ID,
                                                 const bool bBuildOnLanding)
{
	FNomadicAirDeliveryOrder Order;
	Order.Method = ENomadicAirDeliveryMethod::MammutGlider;
	Order.NomadicSubtype = ENomadicSubtype::Nomadic_GerHq;
	Order.BuildLocation = BuildLocation;
	Order.BuildRotation = BuildRotation;
	Order.GliderTouchdownLocation = GliderTouchdownLocation;
	Order.bBuildOnLanding = bBuildOnLanding;
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
	// A glider HQ that is not built on landing still has to drive out of the glider's nose.
	const bool bWaitForGliderExit = bIsHQDelivery && not Order.bBuildOnLanding
		&& Order.Method == ENomadicAirDeliveryMethod::MammutGlider;
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	const FOnNomadicAirDeliveryLanded OnLanded = FOnNomadicAirDeliveryLanded::CreateLambda(
		[WeakThis, NomadicSubtype, bIsHQDelivery, bWaitForGliderExit, ID](ANomadicVehicle* NomadicVehicle)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AirDelivery_OnLanded(NomadicVehicle, NomadicSubtype, bIsHQDelivery, bWaitForGliderExit,
				                               ID);
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
                                        const bool bIsHQDelivery, const bool bWaitForGliderExit,
                                        const int32 ID)
{
	if (bIsHQDelivery)
	{
		M_PendingAirDeliveredHQCount = FMath::Max(0, M_PendingAirDeliveredHQCount - 1);
		// Registered before anything else: converting the HQ truck skips the build radius only for the player HQ.
		AirDelivery_RegisterPlayerHQ(NomadicVehicle);
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
	if (bWaitForGliderExit && IsValid(NomadicVehicle))
	{
		AirDelivery_StartHQExitPoll(NomadicVehicle, ID);
	}
}

void UMissionBase::AirDelivery_RegisterPlayerHQ(ANomadicVehicle* NomadicVehicle) const
{
	if (not IsValid(NomadicVehicle))
	{
		return;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(NomadicVehicle);
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Air-delivered HQ landed but there is no player controller to own it."));
		return;
	}
	PlayerController->RegisterPlayerHQ(NomadicVehicle);
}

void UMissionBase::AirDelivery_StartHQExitPoll(ANomadicVehicle* NomadicVehicle, const int32 ID)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	AirDelivery_ClearHQExitPoll();
	M_HQLeavingGlider = NomadicVehicle;
	M_HQLeavingGliderID = ID;
	M_HQLeavingGliderSeconds = 0.f;
	World->GetTimerManager().SetTimer(M_HQLeavingGliderPollHandle, this, &UMissionBase::AirDelivery_PollHQExit,
	                                  MissionAirDeliveryPrivate::HQExitPollSeconds, true);
}

void UMissionBase::AirDelivery_PollHQExit()
{
	M_HQLeavingGliderSeconds += MissionAirDeliveryPrivate::HQExitPollSeconds;
	ANomadicVehicle* NomadicVehicle = M_HQLeavingGlider.Get();
	if (not IsValid(NomadicVehicle) || not NomadicVehicle->IsUnitAlive())
	{
		AirDelivery_ClearHQExitPoll();
		return;
	}
	// The glider queues the drive out before it hands the truck over, so idle means it is out.
	const bool bIsOut = NomadicVehicle->GetIsUnitIdle();
	if (not bIsOut && M_HQLeavingGliderSeconds < MissionAirDeliveryPrivate::HQExitTimeoutSeconds)
	{
		return;
	}
	const int32 ID = M_HQLeavingGliderID;
	AirDelivery_ClearHQExitPoll();
	BP_OnAirDeliveredHQOutOfGlider(NomadicVehicle, ID);
}

void UMissionBase::AirDelivery_ClearHQExitPoll()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_HQLeavingGliderPollHandle);
	}
	M_HQLeavingGlider.Reset();
	M_HQLeavingGliderSeconds = 0.f;
}
