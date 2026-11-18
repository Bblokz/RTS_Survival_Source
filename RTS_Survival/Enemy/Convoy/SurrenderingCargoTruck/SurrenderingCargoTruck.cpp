// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurrenderingCargoTruck.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyTruckComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurrenderingCargoTruckConstants
{
	// The convoy registers its trucks right after they spawned; a truck not in a convoy by then never joins one.
	constexpr double ConvoySearchTimeoutSeconds = 60.0;
}

ASurrenderingCargoTruck::ASurrenderingCargoTruck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The cargo struct defaults to a full supply-point load; a truck carries nothing until it surrenders.
	M_CapturedCargo.Reset();
}

void ASurrenderingCargoTruck::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_StartSurrenderChecks();
}

void ASurrenderingCargoTruck::BeginPlay_StartSurrenderChecks()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	M_ConvoySearchStartSeconds = World->GetTimeSeconds();
	TWeakObjectPtr<ASurrenderingCargoTruck> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SurrenderCheckTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CheckSurrender();
		}
	}), FMath::Max(SurrenderSettings.CheckIntervalSeconds, KINDA_SMALL_NUMBER), true);
}

void ASurrenderingCargoTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTimers();
	// The supply truck component reports a run that ends with the level as lost; that is not a lost captured truck.
	if (M_SupplyRunEndedHandle.IsValid() and GetIsValidSupplyTruckComponent())
	{
		M_SupplyTruckComponent->OnSupplyTruckRunEnded.Remove(M_SupplyRunEndedHandle);
	}
	M_SupplyRunEndedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASurrenderingCargoTruck::StopTimers()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_SurrenderCheckTimerHandle);
	World->GetTimerManager().ClearTimer(M_DriveToDropOffTimerHandle);
}

void ASurrenderingCargoTruck::SetDropOffTarget(AActor* DropOffTarget)
{
	M_DropOffOverride = DropOffTarget;
}

void ASurrenderingCargoTruck::SetCanSurrender(const bool bNewCanSurrender)
{
	SurrenderSettings.bCanSurrender = bNewCanSurrender;
}

bool ASurrenderingCargoTruck::GetHasSurrendered() const
{
	return M_SurrenderState != ESurrenderingTruckState::CanSurrender
		and M_SurrenderState != ESurrenderingTruckState::NoConvoy;
}

bool ASurrenderingCargoTruck::SurrenderNow()
{
	if (GetHasSurrendered() or not GetIsTruckAlive())
	{
		return false;
	}
	Surrender();
	return true;
}

bool ASurrenderingCargoTruck::GetIsTruckAlive()
{
	return IsValid(this) and not IsActorBeingDestroyed() and IsUnitAlive();
}

// ------------------------------------------------------------------
// Surrender checks
// ------------------------------------------------------------------

void ASurrenderingCargoTruck::CheckSurrender()
{
	if (M_SurrenderState != ESurrenderingTruckState::CanSurrender)
	{
		StopTimers();
		return;
	}
	if (not SurrenderSettings.bCanSurrender or not GetIsTruckAlive())
	{
		return;
	}
	const AConvoy* Convoy = GetConvoyOfTruck();
	if (Convoy == nullptr or not GetIsConvoyReadyForSurrender(*Convoy) or not GetIsSurrenderTriggerMet(*Convoy))
	{
		return;
	}
	if (not GetIsHealthEnoughToSurrender())
	{
		return;
	}
	if (SurrenderSettings.bRequireCaptorNearby and not GetIsCaptorNearby())
	{
		return;
	}
	Surrender();
}

AConvoy* ASurrenderingCargoTruck::GetConvoyOfTruck()
{
	if (bM_HasFoundConvoy)
	{
		AConvoy* Convoy = M_Convoy.Get();
		if (IsValid(Convoy) and Convoy->GetIsColumnActive() and Convoy->GetIsActiveMember(this))
		{
			return Convoy;
		}
		// Released alive at the exit or by EndConvoy: no escort to lose any more.
		M_SurrenderState = ESurrenderingTruckState::NoConvoy;
		StopTimers();
		return nullptr;
	}
	AConvoy* FoundConvoy = AConvoy::FindConvoyOfActiveMember(this);
	if (IsValid(FoundConvoy))
	{
		M_Convoy = FoundConvoy;
		bM_HasFoundConvoy = true;
		return FoundConvoy;
	}
	const UWorld* World = GetWorld();
	const bool bSearchTimedOut = World != nullptr and M_ConvoySearchStartSeconds >= 0.0
		and World->GetTimeSeconds() - M_ConvoySearchStartSeconds > SurrenderingCargoTruckConstants::ConvoySearchTimeoutSeconds;
	if (bSearchTimedOut)
	{
		UE_LOG(LogConvoy, Log, TEXT("Surrendering truck %s is in no convoy; it only surrenders through SurrenderNow."),
		       *GetName());
		M_SurrenderState = ESurrenderingTruckState::NoConvoy;
		StopTimers();
	}
	return nullptr;
}

bool ASurrenderingCargoTruck::GetIsConvoyReadyForSurrender(const AConvoy& Convoy) const
{
	// While the column spawns or forms, escorts that have not reported yet would look dead.
	const EConvoyState ConvoyState = Convoy.GetConvoyState();
	return Convoy.GetIsColumnActive()
		and (ConvoyState == EConvoyState::Cruise or ConvoyState == EConvoyState::Regroup
			or ConvoyState == EConvoyState::UnderAttack);
}

bool ASurrenderingCargoTruck::GetIsSurrenderTriggerMet(const AConvoy& Convoy) const
{
	switch (SurrenderSettings.Trigger)
	{
	case ESurrenderingTruckTrigger::NoEscortNearby:
		return not Convoy.GetIsAnyEscortAliveNear(GetActorLocation(), SurrenderSettings.EscortProtectionRadius);
	case ESurrenderingTruckTrigger::AllEscortsDestroyed:
	default:
		return Convoy.GetAliveEscortCount() == 0;
	}
}

bool ASurrenderingCargoTruck::GetIsHealthEnoughToSurrender() const
{
	if (SurrenderSettings.MinHealthFractionToSurrender <= 0.f)
	{
		return true;
	}
	const UHealthComponent* TruckHealth = GetHealthComponent();
	if (not IsValid(TruckHealth) or TruckHealth->GetMaxHealth() <= 0.f)
	{
		return true;
	}
	return TruckHealth->GetHealthPercentage() >= SurrenderSettings.MinHealthFractionToSurrender;
}

bool ASurrenderingCargoTruck::GetIsCaptorNearby() const
{
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return false;
	}
	const uint8 CaptorMask = FRTSPlayers::GetPlayerBit(SurrenderSettings.CapturingPlayer);
	const FVector TruckLocation = GetActorLocation();
	const float RadiusSquared = FMath::Square(SurrenderSettings.CaptorSearchRadius);
	const TArray<ATankMaster*> CaptorTanks = UnitManager->GetTanksOfPlayers(CaptorMask);
	for (const ATankMaster* Tank : CaptorTanks)
	{
		// A truck that surrendered earlier drives for the captor but captures nothing.
		const bool bIsCaptor = IsValid(Tank) and not Tank->IsA<ASurrenderingCargoTruck>();
		if (bIsCaptor and FVector::DistSquared2D(Tank->GetActorLocation(), TruckLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	const TArray<ASquadUnit*> CaptorSoldiers = UnitManager->GetSquadUnitsOfPlayers(CaptorMask);
	for (const ASquadUnit* Soldier : CaptorSoldiers)
	{
		if (IsValid(Soldier) and FVector::DistSquared2D(Soldier->GetActorLocation(), TruckLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------
// Surrender
// ------------------------------------------------------------------

void ASurrenderingCargoTruck::Surrender()
{
	// Set first: everything below may broadcast, and nothing may surrender this truck twice.
	M_SurrenderState = ESurrenderingTruckState::Surrendered;
	StopTimers();
	if (not M_Convoy.IsValid())
	{
		// SurrenderNow before the first check found the convoy.
		M_Convoy = AConvoy::FindConvoyOfActiveMember(this);
	}
	// Read before leaving: the cargo is the convoy's per-truck share.
	M_CapturedCargo = BuildCapturedCargo();
	Surrender_LeaveConvoy();
	// A listener of the convoy's capture event may have removed the truck.
	if (not GetIsTruckAlive())
	{
		M_SurrenderState = ESurrenderingTruckState::Lost;
		OnCapturedTruckLost.Broadcast(this);
		return;
	}
	// The convoy's last order would keep it driving with the column.
	StopCommand();
	Surrender_ChangeOwnerToCaptor();
	if (not Surrender_SetupSupplyTruck())
	{
		M_SurrenderState = ESurrenderingTruckState::Lost;
		return;
	}
	UE_LOG(LogConvoy, Log, TEXT("Surrendering truck %s surrendered to player %d (cargo radixite=%d metal=%d parts=%d)"),
	       *GetName(), SurrenderSettings.CapturingPlayer, M_CapturedCargo.Radixite, M_CapturedCargo.Metal,
	       M_CapturedCargo.VehicleParts);
	OnSurrendered.Broadcast(this);
	BP_OnSurrendered(SurrenderSettings.CapturingPlayer);
	if (M_SurrenderState == ESurrenderingTruckState::Surrendered)
	{
		ScheduleDriveToDropOff();
	}
}

void ASurrenderingCargoTruck::Surrender_LeaveConvoy()
{
	AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		// A truck without a convoy (placed by hand, or its convoy already ended).
		return;
	}
	if (not Convoy->ReleaseCapturedTruck(this))
	{
		UE_LOG(LogConvoy, Warning, TEXT("Surrendering truck %s could not leave convoy %d as captured."), *GetName(),
		       Convoy->GetConvoyId());
	}
	M_Convoy.Reset();
}

void ASurrenderingCargoTruck::Surrender_ChangeOwnerToCaptor()
{
	URTSComponent* TruckRTSComponent = GetRTSComponent();
	if (not IsValid(TruckRTSComponent))
	{
		RTSFunctionLibrary::ReportError("Surrendering truck has no RTS component to change owner: " + GetName());
		return;
	}
	if (not FRTSPlayers::IsValidPlayer(SurrenderSettings.CapturingPlayer))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Surrendering truck %s has an invalid CapturingPlayer %d; it keeps its owner."), *GetName(),
			SurrenderSettings.CapturingPlayer));
		return;
	}
	// Moves the unit registries, tactical AI, fog of war, collision side and weapon owners to the captor.
	TruckRTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(SurrenderSettings.CapturingPlayer));
}

bool ASurrenderingCargoTruck::Surrender_SetupSupplyTruck()
{
	M_SupplyTruckComponent = FindComponentByClass<USupplyTruckComponent>();
	if (M_SupplyTruckComponent == nullptr)
	{
		// Registering after BeginPlay begins play at once: the truck becomes uncontrollable and gets its icon.
		M_SupplyTruckComponent = NewObject<USupplyTruckComponent>(this, TEXT("CapturedSupplyTruckComponent"));
		M_SupplyTruckComponent->RegisterComponent();
	}
	if (not GetIsValidSupplyTruckComponent())
	{
		return false;
	}
	// Also when a Blueprint already had the component: the player must not steer a captured truck.
	SetUnitSelectable(false);
	M_SupplyRunEndedHandle = M_SupplyTruckComponent->OnSupplyTruckRunEnded.AddUObject(
		this, &ASurrenderingCargoTruck::OnSupplyRunEnded);
	return true;
}

void ASurrenderingCargoTruck::ScheduleDriveToDropOff()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (SurrenderSettings.SurrenderStandSeconds <= 0.f)
	{
		StartDriveToDropOff();
		return;
	}
	TWeakObjectPtr<ASurrenderingCargoTruck> WeakThis(this);
	World->GetTimerManager().SetTimer(M_DriveToDropOffTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->StartDriveToDropOff();
		}
	}), SurrenderSettings.SurrenderStandSeconds, false);
}

FSupplyTruckCargo ASurrenderingCargoTruck::BuildCapturedCargo() const
{
	if (DeliverySettings.bUseFixedCargo)
	{
		return DeliverySettings.FixedCargo;
	}
	FSupplyTruckCargo Cargo;
	Cargo.Reset();
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		RTSFunctionLibrary::ReportError("Surrendering truck " + GetName()
			+ " has no convoy to read its cargo from and no fixed cargo; it carries nothing.");
		return Cargo;
	}
	const float Multiplier = FMath::Max(0.f, DeliverySettings.CapturedCargoMultiplier);
	for (const TPair<ERTSResourceType, int32>& Pair : Convoy->GetCargoPerTruck().Resources)
	{
		const int32 Amount = FMath::RoundToInt(Pair.Value * Multiplier);
		switch (Pair.Key)
		{
		case ERTSResourceType::Resource_Radixite:
			Cargo.Radixite += Amount;
			break;
		case ERTSResourceType::Resource_Metal:
			Cargo.Metal += Amount;
			break;
		case ERTSResourceType::Resource_VehicleParts:
			Cargo.VehicleParts += Amount;
			break;
		default:
			// A supply truck only carries what the player stores; use FixedCargo for anything else.
			UE_LOG(LogConvoy, Warning, TEXT("Surrendering truck %s cannot carry %s (%d); it is left out."),
			       *GetName(), *Global_GetResourceTypeAsString(Pair.Key), Amount);
			break;
		}
	}
	return Cargo;
}

// ------------------------------------------------------------------
// Drive to the drop-off
// ------------------------------------------------------------------

void ASurrenderingCargoTruck::StartDriveToDropOff()
{
	if (M_SurrenderState != ESurrenderingTruckState::Surrendered or not GetIsTruckAlive()
		or not GetIsValidSupplyTruckComponent())
	{
		return;
	}
	M_SurrenderState = ESurrenderingTruckState::DrivingToDropOff;
	AActor* DropOff = FindDropOffTarget();
	if (not IsValid(DropOff))
	{
		RTSFunctionLibrary::ReportError("Captured truck " + GetName() + " has no drop-off (no SetDropOffTarget and no "
			"actor tagged " + DeliverySettings.DropOffTag.ToString() + "); it drives to its owner's base instead.");
		M_SupplyTruckComponent->StartDeliveryRunToBase(M_CapturedCargo);
		BP_OnDriveToDropOffStarted(nullptr);
		return;
	}
	M_SupplyTruckComponent->StartDeliveryRun(GetDeliveryTransformOfDropOff(*DropOff), M_CapturedCargo);
	BP_OnDriveToDropOffStarted(DropOff);
}

AActor* ASurrenderingCargoTruck::FindDropOffTarget() const
{
	if (M_DropOffOverride.IsValid())
	{
		return M_DropOffOverride.Get();
	}
	UWorld* World = GetWorld();
	if (World == nullptr or DeliverySettings.DropOffTag.IsNone())
	{
		return nullptr;
	}
	AActor* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	const FVector TruckLocation = GetActorLocation();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (not IsValid(Candidate) or not Candidate->ActorHasTag(DeliverySettings.DropOffTag))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(Candidate->GetActorLocation(), TruckLocation);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}
	return Nearest;
}

FTransform ASurrenderingCargoTruck::GetDeliveryTransformOfDropOff(const AActor& DropOff) const
{
	FVector DeliveryLocation = DropOff.GetActorLocation();
	if (not UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), DeliveryLocation, DeliveryLocation))
	{
		UE_LOG(LogConvoy, Warning, TEXT("Captured truck %s: drop-off %s is not on the vehicle navmesh."), *GetName(),
		       *DropOff.GetName());
	}
	return FTransform(FRotator(0.f, DropOff.GetActorRotation().Yaw, 0.f), DeliveryLocation);
}

void ASurrenderingCargoTruck::OnSupplyRunEnded(USupplyTruckComponent* SupplyTruck, const bool bDelivered)
{
	if (SupplyTruck != M_SupplyTruckComponent or M_SurrenderState == ESurrenderingTruckState::Delivered
		or M_SurrenderState == ESurrenderingTruckState::Lost)
	{
		return;
	}
	StopTimers();
	if (bDelivered)
	{
		M_SurrenderState = ESurrenderingTruckState::Delivered;
		UE_LOG(LogConvoy, Log, TEXT("Captured truck %s delivered its cargo."), *GetName());
		OnCapturedCargoDelivered.Broadcast(this, M_CapturedCargo);
		BP_OnCapturedCargoDelivered(M_CapturedCargo);
		return;
	}
	M_SurrenderState = ESurrenderingTruckState::Lost;
	UE_LOG(LogConvoy, Log, TEXT("Captured truck %s was lost on the way; its cargo is gone."), *GetName());
	OnCapturedTruckLost.Broadcast(this);
	BP_OnCapturedTruckLost();
}

bool ASurrenderingCargoTruck::GetIsValidSupplyTruckComponent() const
{
	if (IsValid(M_SupplyTruckComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SupplyTruckComponent",
	                                                      "GetIsValidSupplyTruckComponent", this);
	return false;
}
