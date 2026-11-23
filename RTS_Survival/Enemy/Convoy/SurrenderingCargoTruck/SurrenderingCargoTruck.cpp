// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurrenderingCargoTruck.h"

#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace SurrenderingCargoTruckConstants
{
	// The convoy registers its trucks right after they spawned; a truck not in a convoy by then never joins one.
	constexpr double ConvoySearchTimeoutSeconds = 60.0;
	// The blast deals this many times the truck's health so no armour or resistance keeps it alive.
	constexpr float ScuttleDamageHealthMultiplier = 100.f;
	constexpr float MinScuttleDamage = 1.f;
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
	// Only the timers: the countdown widget and the hold go with the actor.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScuttleState.ScuttleTimerHandle);
		World->GetTimerManager().ClearTimer(M_ScuttleState.RefreshTimerHandle);
	}
	// The supply truck component reports a run that ends with the level as lost; that is not a lost captured truck.
	if (M_SupplyRunEndedHandle.IsValid() and GetIsValidSupplyTruckComponent())
	{
		M_SupplyTruckComponent->OnSupplyTruckRunEnded.Remove(M_SupplyRunEndedHandle);
	}
	M_SupplyRunEndedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASurrenderingCargoTruck::UnitDies(const ERTSDeathType DeathType)
{
	// Shot dead before it surrendered (also mid-countdown): an ordinary destroyed convoy truck.
	const bool bDiesAsConvoyTruck = M_SurrenderState == ESurrenderingTruckState::CanSurrender
		or M_SurrenderState == ESurrenderingTruckState::NoConvoy
		or M_SurrenderState == ESurrenderingTruckState::ScuttleCountdown;
	if (bDiesAsConvoyTruck)
	{
		M_SurrenderState = ESurrenderingTruckState::Destroyed;
	}
	EndScuttleCountdown();
	StopTimers();
	// Broadcasts OnUnitDies: the convoy counts a destroyed truck and drops its cache; a captured run ends as lost.
	Super::UnitDies(DeathType);
}

float ASurrenderingCargoTruck::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent,
                                          AController* EventInstigator, AActor* DamageCauser)
{
	const float Result = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (DamageAmount > 0.f)
	{
		// Alerts the crew of a convoy without escort.
		M_ScuttleState.bWasDamaged = true;
	}
	return Result;
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
	return M_SurrenderState == ESurrenderingTruckState::Surrendered
		or M_SurrenderState == ESurrenderingTruckState::DrivingToDropOff
		or M_SurrenderState == ESurrenderingTruckState::Delivered
		or M_SurrenderState == ESurrenderingTruckState::Lost;
}

float ASurrenderingCargoTruck::GetScuttleSecondsLeft() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr or M_SurrenderState != ESurrenderingTruckState::ScuttleCountdown)
	{
		return -1.f;
	}
	return World->GetTimerManager().GetTimerRemaining(M_ScuttleState.ScuttleTimerHandle);
}

bool ASurrenderingCargoTruck::SurrenderNow()
{
	const bool bCanStillSurrender = M_SurrenderState == ESurrenderingTruckState::CanSurrender
		or M_SurrenderState == ESurrenderingTruckState::NoConvoy
		or M_SurrenderState == ESurrenderingTruckState::ScuttleCountdown;
	if (not bCanStillSurrender or not GetIsTruckAlive())
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
	switch (M_SurrenderState)
	{
	case ESurrenderingTruckState::CanSurrender:
		CheckSurrender_InConvoy();
		break;
	case ESurrenderingTruckState::ScuttleCountdown:
		CheckSurrender_DuringCountdown();
		break;
	default:
		StopTimers();
		break;
	}
}

void ASurrenderingCargoTruck::CheckSurrender_InConvoy()
{
	if (not SurrenderSettings.bCanSurrender or not GetIsTruckAlive())
	{
		return;
	}
	const AConvoy* Convoy = GetConvoyOfTruck();
	if (Convoy == nullptr)
	{
		return;
	}
	M_ScuttleState.MostAliveEscortsSeen = FMath::Max(M_ScuttleState.MostAliveEscortsSeen, Convoy->GetAliveEscortCount());
	if (not GetIsConvoyReadyForSurrender(*Convoy) or not GetIsSurrenderTriggerMet(*Convoy))
	{
		return;
	}
	// A dead escort is the alarm; a convoy that never had one needs the captor close or a hit on the truck.
	if (M_ScuttleState.MostAliveEscortsSeen == 0 and not GetIsUnescortedTruckAlerted())
	{
		return;
	}
	if (GetCanBeCapturedNow())
	{
		Surrender();
		return;
	}
	if (ScuttleSettings.bScuttleWhenNotCaptured)
	{
		StartScuttleCountdown();
	}
}

void ASurrenderingCargoTruck::CheckSurrender_DuringCountdown()
{
	if (not SurrenderSettings.bCanSurrender or not GetIsTruckAlive())
	{
		return;
	}
	if (GetCanBeCapturedNow())
	{
		Surrender();
	}
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

bool ASurrenderingCargoTruck::GetIsUnescortedTruckAlerted() const
{
	constexpr bool bIncludeVehicles = true;
	return M_ScuttleState.bWasDamaged
		or GetIsCaptorUnitWithin(ScuttleSettings.UnescortedAlertRadius, bIncludeVehicles);
}

bool ASurrenderingCargoTruck::GetCanBeCapturedNow() const
{
	if (not GetIsHealthEnoughToSurrender())
	{
		return false;
	}
	if (not SurrenderSettings.bRequireCaptorNearby)
	{
		return true;
	}
	const bool bVehiclesCapture = SurrenderSettings.Captors == ESurrenderingTruckCaptors::SquadsAndVehicles;
	return GetIsCaptorUnitWithin(SurrenderSettings.CaptorSearchRadius, bVehiclesCapture);
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

bool ASurrenderingCargoTruck::GetIsCaptorUnitWithin(const float Radius, const bool bIncludeVehicles) const
{
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return false;
	}
	const uint8 CaptorMask = FRTSPlayers::GetPlayerBit(SurrenderSettings.CapturingPlayer);
	const FVector TruckLocation = GetActorLocation();
	const float RadiusSquared = FMath::Square(Radius);
	const TArray<ASquadUnit*> CaptorSoldiers = UnitManager->GetSquadUnitsOfPlayers(CaptorMask);
	for (const ASquadUnit* Soldier : CaptorSoldiers)
	{
		if (IsValid(Soldier) and FVector::DistSquared2D(Soldier->GetActorLocation(), TruckLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	if (not bIncludeVehicles)
	{
		return false;
	}
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
	return false;
}

// ------------------------------------------------------------------
// Scuttle countdown
// ------------------------------------------------------------------

void ASurrenderingCargoTruck::StartScuttleCountdown()
{
	UWorld* World = GetWorld();
	if (World == nullptr or M_SurrenderState != ESurrenderingTruckState::CanSurrender)
	{
		return;
	}
	M_SurrenderState = ESurrenderingTruckState::ScuttleCountdown;
	const float CountdownSeconds = FMath::Max(ScuttleSettings.CountdownSeconds, KINDA_SMALL_NUMBER);
	TWeakObjectPtr<ASurrenderingCargoTruck> WeakThis(this);
	World->GetTimerManager().SetTimer(M_ScuttleState.ScuttleTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Scuttle();
		}
	}), CountdownSeconds, false);
	if (ScuttleSettings.bStopDuringCountdown)
	{
		HoldInPlace();
	}
	ShowCountdownWidget();
	UE_LOG(LogConvoy, Log, TEXT("Surrendering truck %s: escort gone, scuttle countdown %.1f s."), *GetName(),
	       CountdownSeconds);
	OnScuttleCountdownStarted.Broadcast(this, CountdownSeconds);
	BP_OnScuttleCountdownStarted(CountdownSeconds);
}

void ASurrenderingCargoTruck::HoldInPlace()
{
	// The convoy keeps its own restriction and orders; ours (the minimum wins) keeps the truck standing.
	constexpr float NoTravel = 0.f;
	constexpr float NoTurning = 0.f;
	SetMobilityRestriction(this, NoTravel, NoTurning);
	StopCommand();
	M_ScuttleState.bIsHeldInPlace = true;
}

void ASurrenderingCargoTruck::ReleaseHoldInPlace()
{
	if (not M_ScuttleState.bIsHeldInPlace)
	{
		return;
	}
	M_ScuttleState.bIsHeldInPlace = false;
	ClearMobilityRestriction(this);
}

void ASurrenderingCargoTruck::ShowCountdownWidget()
{
	UClass* WidgetClass = ScuttleSettings.CountdownWidgetClass.Get();
	if (WidgetClass == nullptr or IsValid(M_ScuttleState.CountdownWidgetComponent))
	{
		return;
	}
	// Screen space: always faces the camera at a fixed pixel size, and is not drawn while the fog hides the truck.
	UWidgetComponent* CountdownComponent = NewObject<UWidgetComponent>(this, TEXT("ScuttleCountdownWidget"));
	CountdownComponent->SetWidgetSpace(EWidgetSpace::Screen);
	CountdownComponent->SetDrawAtDesiredSize(true);
	CountdownComponent->SetPivot(FVector2D(0.5f, 1.f));
	CountdownComponent->SetWidgetClass(WidgetClass);
	CountdownComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CountdownComponent->SetGenerateOverlapEvents(false);
	CountdownComponent->SetupAttachment(GetRootComponent());
	CountdownComponent->SetRelativeLocation(ScuttleSettings.CountdownWidgetOffset);
	CountdownComponent->RegisterComponent();
	M_ScuttleState.CountdownWidgetComponent = CountdownComponent;
	RefreshCountdownWidget();
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<ASurrenderingCargoTruck> WeakThis(this);
	World->GetTimerManager().SetTimer(M_ScuttleState.RefreshTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RefreshCountdownWidget();
		}
	}), FMath::Max(ScuttleSettings.CountdownRefreshSeconds, KINDA_SMALL_NUMBER), true);
}

void ASurrenderingCargoTruck::RefreshCountdownWidget() const
{
	if (not IsValid(M_ScuttleState.CountdownWidgetComponent))
	{
		return;
	}
	UW_SurrenderingTruckCountdown* CountdownWidget = Cast<UW_SurrenderingTruckCountdown>(
		M_ScuttleState.CountdownWidgetComponent->GetUserWidgetObject());
	if (CountdownWidget == nullptr)
	{
		return;
	}
	const float SecondsLeft = FMath::Max(0.f, GetScuttleSecondsLeft());
	const float FractionLeft = SecondsLeft / FMath::Max(ScuttleSettings.CountdownSeconds, KINDA_SMALL_NUMBER);
	CountdownWidget->SetCountdown(SecondsLeft, FractionLeft, ScuttleSettings.CountdownTextFormat);
}

void ASurrenderingCargoTruck::RemoveCountdownWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScuttleState.RefreshTimerHandle);
	}
	if (IsValid(M_ScuttleState.CountdownWidgetComponent))
	{
		M_ScuttleState.CountdownWidgetComponent->DestroyComponent();
	}
	M_ScuttleState.CountdownWidgetComponent = nullptr;
}

void ASurrenderingCargoTruck::EndScuttleCountdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScuttleState.ScuttleTimerHandle);
	}
	RemoveCountdownWidget();
	ReleaseHoldInPlace();
}

void ASurrenderingCargoTruck::Scuttle()
{
	using namespace SurrenderingCargoTruckConstants;
	if (M_SurrenderState != ESurrenderingTruckState::ScuttleCountdown or not GetIsTruckAlive())
	{
		return;
	}
	// Set first: the death below runs UnitDies, which must not see a countdown any more.
	M_SurrenderState = ESurrenderingTruckState::Scuttled;
	EndScuttleCountdown();
	StopTimers();
	UE_LOG(LogConvoy, Log, TEXT("Surrendering truck %s: nobody took it, the crew blows it up."), *GetName());
	OnScuttled.Broadcast(this);
	BP_OnScuttled();
	PlayScuttleExplosion();
	// A listener may already have removed the truck.
	if (not GetIsTruckAlive())
	{
		return;
	}
	// The normal damage path: the convoy sees a destroyed truck and drops its cargo cache.
	const UHealthComponent* TruckHealth = GetHealthComponent();
	const float CurrentHealth = IsValid(TruckHealth) ? TruckHealth->GetCurrentHealth() : MinScuttleDamage;
	FDamageEvent ScuttleDamageEvent;
	ScuttleDamageEvent.DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(ScuttleSettings.ScuttleDamageType);
	TakeDamage(FMath::Max(CurrentHealth, MinScuttleDamage) * ScuttleDamageHealthMultiplier, ScuttleDamageEvent,
	           nullptr, nullptr);
	if (GetIsTruckAlive())
	{
		// Nothing should survive the blast; die the normal way all the same.
		TriggerDestroyActor(FRTSWeaponHelpers::TranslateDamageIntoDeathType(ScuttleSettings.ScuttleDamageType));
	}
}

void ASurrenderingCargoTruck::PlayScuttleExplosion() const
{
	// What the fog of war hides makes no visible blast either.
	if (IsHidden())
	{
		return;
	}
	const FVector BlastLocation = GetActorLocation();
	if (IsValid(ScuttleSettings.ScuttleExplosionEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, ScuttleSettings.ScuttleExplosionEffect, BlastLocation, GetActorRotation(),
			FVector(ScuttleSettings.ScuttleExplosionEffectScale));
	}
	if (IsValid(ScuttleSettings.ScuttleExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ScuttleSettings.ScuttleExplosionSound, BlastLocation);
	}
}

// ------------------------------------------------------------------
// Surrender
// ------------------------------------------------------------------

void ASurrenderingCargoTruck::Surrender()
{
	// Set first: everything below may broadcast, and nothing may surrender this truck twice.
	M_SurrenderState = ESurrenderingTruckState::Surrendered;
	StopTimers();
	// Stops a running countdown: no blast, no widget, and the truck may drive again.
	EndScuttleCountdown();
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
