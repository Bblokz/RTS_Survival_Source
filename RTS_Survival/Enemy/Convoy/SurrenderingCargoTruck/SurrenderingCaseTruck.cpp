// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurrenderingCaseTruck.h"

#include "Engine/World.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ASurrenderingCaseTruck::ASurrenderingCaseTruck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

ASurrenderingCaseTruck* ASurrenderingCaseTruck::GetActiveCaseTruck(const UObject* WorldContextObject)
{
	const USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(WorldContextObject);
	return Subsystem != nullptr ? Subsystem->GetActiveCaseTruck() : nullptr;
}

AActor* ASurrenderingCaseTruck::SpawnSealedCacheAt(const UObject* WorldContextObject,
                                                   const TSubclassOf<AScavengeableObject> SealedCacheClass,
                                                   const FTransform& SpawnTransform, const float ScavengeSeconds)
{
	USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(WorldContextObject);
	if (Subsystem == nullptr)
	{
		RTSFunctionLibrary::ReportError("SpawnSealedCacheAt: no world (invalid world context).");
		return nullptr;
	}
	return Subsystem->SpawnSealedCache(SealedCacheClass, SpawnTransform, ScavengeSeconds);
}

void ASurrenderingCaseTruck::BeginPlay()
{
	Super::BeginPlay();
	if (CaseTruckSettings.SealedCache.SealedCacheClass == nullptr)
	{
		RTSFunctionLibrary::ReportError("Case truck " + GetName() + " has no SealedCacheClass: destroying it would "
			"lose the cases. Set Case Truck > Sealed Cache in its Blueprint.");
	}
	if (USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(this))
	{
		Subsystem->RegisterCaseTruck(this);
	}
}

void ASurrenderingCaseTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Removed without dying or delivering: left with the convoy, or taken off the map (abandoned stuck run, script).
	const bool bRemovedFromPlay = EndPlayReason == EEndPlayReason::Destroyed;
	if (bRemovedFromPlay and M_CasesState == ECaseTruckCasesState::OnTruck)
	{
		if (GetIsLeavingWithEscapedConvoy())
		{
			MarkCasesEscaped();
		}
		else
		{
			// The mission stays winnable: the cases wait where the truck was.
			DropCasesInSealedCache(GetSealedCacheTransform());
		}
	}
	if (USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(this))
	{
		Subsystem->UnregisterCaseTruck(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASurrenderingCaseTruck::UnitDies(const ERTSDeathType DeathType)
{
	const bool bWasAlive = IsUnitAlive();
	// Read before the death: the Blueprint may move or remove the truck for its wreck.
	const FTransform SealedCacheTransform = GetSealedCacheTransform();
	// Shot, scuttled, killed mid-countdown or after capture: the convoy skips its cargo cache for this class.
	Super::UnitDies(DeathType);
	if (bWasAlive)
	{
		DropCasesInSealedCache(SealedCacheTransform);
	}
}

float ASurrenderingCaseTruck::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent,
                                         AController* EventInstigator, AActor* DamageCauser)
{
	const float Multiplier = FMath::Clamp(CaseTruckSettings.DamageTakenMultiplier, 0.f, 1.f);
	return Super::TakeDamage(DamageAmount * Multiplier, DamageEvent, EventInstigator, DamageCauser);
}

void ASurrenderingCaseTruck::HandleCapturedCargoDelivered()
{
	if (M_CasesState != ECaseTruckCasesState::OnTruck)
	{
		return;
	}
	M_CasesState = ECaseTruckCasesState::Delivered;
	UE_LOG(LogConvoy, Log, TEXT("Case truck %s delivered the cases."), *GetName());
	OnCasesDelivered.Broadcast(this);
	BP_OnCasesDelivered();
	if (USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(this))
	{
		Subsystem->NotifyCasesDelivered(this);
	}
}

bool ASurrenderingCaseTruck::GetIsLeavingWithEscapedConvoy() const
{
	// The convoy sets its outcome before it removes the units it takes off the map at the exit.
	const AConvoy* Convoy = GetLastKnownConvoy();
	return IsValid(Convoy) and Convoy->GetOutcome() == EConvoyOutcome::Escaped and not GetHasSurrendered();
}

FTransform ASurrenderingCaseTruck::GetSealedCacheTransform() const
{
	FVector CacheLocation = GetActorLocation() + GetActorRightVector() * CaseTruckSettings.SealedCacheSideOffset;
	if (not UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), CacheLocation, CacheLocation))
	{
		// Off the navmesh beside the wreck: the truck's own spot was reachable.
		CacheLocation = GetActorLocation();
	}
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), CacheLocation);
}

void ASurrenderingCaseTruck::MarkCasesEscaped()
{
	M_CasesState = ECaseTruckCasesState::Escaped;
	UE_LOG(LogConvoy, Log, TEXT("Case truck %s escaped with its convoy; the cases are gone with it."), *GetName());
	OnCasesEscaped.Broadcast(this);
	BP_OnCasesEscaped();
	if (USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(this))
	{
		Subsystem->NotifyCasesEscaped(this, CaseTruckSettings.SealedCache);
	}
}

void ASurrenderingCaseTruck::DropCasesInSealedCache(const FTransform& SealedCacheTransform)
{
	if (M_CasesState != ECaseTruckCasesState::OnTruck)
	{
		return;
	}
	M_CasesState = ECaseTruckCasesState::InSealedCache;
	UE_LOG(LogConvoy, Log, TEXT("Case truck %s is gone; the cases wait in a sealed cache at %s."), *GetName(),
	       *SealedCacheTransform.GetLocation().ToCompactString());
	OnCasesDropped.Broadcast(this, SealedCacheTransform);
	BP_OnCasesDropped(SealedCacheTransform);
	USurrenderingTruckSubsystem* Subsystem = USurrenderingTruckSubsystem::Get(this);
	if (Subsystem == nullptr)
	{
		return;
	}
	Subsystem->SpawnSealedCacheWithSettings(CaseTruckSettings.SealedCache, SealedCacheTransform,
	                                        CaseTruckSettings.SealedCacheSpawnDelaySeconds);
}
