// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_FlareCurtain.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

void UGA_FlareCurtain::OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
                                             const float NewDistance)
{
	const int32 FlareCount = FMath::Max(M_AimSettings.AreaAim.ShaderMarkerCount, 1);
	const float Spacing = FlareCount > 1 ? Run.StripLength / (FlareCount - 1) : 0.0f;
	for (int32 FlareIndex = 0; FlareIndex < FlareCount; ++FlareIndex)
	{
		const float FlareDistance = Spacing * FlareIndex;
		if (FlareDistance > PreviousDistance && FlareDistance <= NewDistance)
		{
			DropFlare(Run, Run.LineStart + Run.Direction * FlareDistance);
		}
	}
}

void UGA_FlareCurtain::DropFlare(const FStripFlightRun& Run, const FVector& LocationOnStrip) const
{
	UWorld* World = GetWorld();
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(World) || not IsValid(Aircraft))
	{
		return;
	}
	FVector GroundLocation;
	GlobalAbilityHelpers::TryFindGroundLocation(this, LocationOnStrip, GroundLocation);

	FFlareProjectileSettings FlareSettings;
	FlareSettings.ApexHeight = M_FlareCurtainSettings.IgnitionHeight;
	FlareSettings.HangTimeSeconds = M_FlareCurtainSettings.HangTimeSeconds;
	FlareSettings.VisionRadius = M_AimSettings.AreaAim.AreaRadius;
	FlareSettings.AscentSpeed = M_FlareCurtainSettings.FallSpeed;
	FlareSettings.Visuals = M_FlareCurtainSettings.Visuals;

	const FVector ReleaseLocation = Aircraft->GetActorLocation();
	AFlareProjectile* Flare = World->SpawnActor<AFlareProjectile>(
		AFlareProjectile::StaticClass(), ReleaseLocation, FRotator::ZeroRotator);
	if (not IsValid(Flare))
	{
		return;
	}
	Flare->DropFlare(ReleaseLocation, GroundLocation, FlareSettings, static_cast<uint8>(GetOwningPlayer()));
}
