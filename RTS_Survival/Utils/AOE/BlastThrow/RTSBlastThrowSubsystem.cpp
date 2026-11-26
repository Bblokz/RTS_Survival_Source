// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSBlastThrowSubsystem.h"

#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleDamageEvent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace RTSBlastThrowConstants
{
	// The events carry the epicentre they were dealt from, unchanged; this only absorbs float noise.
	constexpr float EpicenterToleranceCm = 50.f;
}

void URTSBlastThrowSubsystem::RegisterBlastThrow(const FRTSBlastThrow& Throw, const float LifetimeSeconds)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const float NowSeconds = World->GetTimeSeconds();
	RemoveExpired(NowSeconds);
	FRTSBlastThrow& Added = M_Throws.Add_GetRef(Throw);
	Added.ExpireAtSeconds = NowSeconds + LifetimeSeconds;
}

const FRTSBlastThrow* URTSBlastThrowSubsystem::FindForDamageEvent(const UObject* WorldContext,
                                                                  const FDamageEvent& DamageEvent,
                                                                  FVector& OutEpicenter)
{
	const UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	const URTSBlastThrowSubsystem* Subsystem = IsValid(World) ? World->GetSubsystem<URTSBlastThrowSubsystem>() : nullptr;
	if (not IsValid(Subsystem) || Subsystem->M_Throws.IsEmpty())
	{
		return nullptr;
	}
	if (DamageEvent.IsOfType(FRTSBlastDamageEvent::ClassID))
	{
		OutEpicenter = static_cast<const FRTSBlastDamageEvent&>(DamageEvent).Epicenter;
	}
	else if (DamageEvent.IsOfType(FVehicleModuleDamageEvent::ClassID))
	{
		// Splash on armoured vehicles arrives as a module event that names the explosion location.
		OutEpicenter = static_cast<const FVehicleModuleDamageEvent&>(DamageEvent).ExplosionLocation;
	}
	else
	{
		return nullptr;
	}
	const FRTSBlastThrow* Throw = Subsystem->FindNear(OutEpicenter);
	if (Throw == nullptr || World->GetTimeSeconds() > Throw->ExpireAtSeconds)
	{
		return nullptr;
	}
	return Throw;
}

const FRTSBlastThrow* URTSBlastThrowSubsystem::FindNear(const FVector& Epicenter) const
{
	for (const FRTSBlastThrow& Throw : M_Throws)
	{
		if (FVector::DistSquared(Throw.Epicenter, Epicenter)
			<= FMath::Square(RTSBlastThrowConstants::EpicenterToleranceCm))
		{
			return &Throw;
		}
	}
	return nullptr;
}

void URTSBlastThrowSubsystem::RemoveExpired(const float NowSeconds)
{
	M_Throws.RemoveAll([NowSeconds](const FRTSBlastThrow& Throw)
	{
		return NowSeconds > Throw.ExpireAtSeconds;
	});
}
