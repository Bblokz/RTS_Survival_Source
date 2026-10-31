// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_RadixiteNuke.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"

void UGA_RadixiteNuke::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetAbilityEndedBroadcast();
	FVector GroundZero;
	GlobalAbilityHelpers::TryFindGroundLocation(this, TargetLocation, GroundZero);

	TWeakObjectPtr<UGA_RadixiteNuke> WeakThis(this);
	M_LoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		GetAssetsToLoad(),
		FStreamableDelegate::CreateLambda([WeakThis, GroundZero]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded(GroundZero);
			}
		})));
	Super::ExecuteAbilityAtLocation(GroundZero);
}

void UGA_RadixiteNuke::BeginDestroy()
{
	M_LoadHandles.Reset();
	Super::BeginDestroy();
}

void UGA_RadixiteNuke::OnAssetsLoaded(const FVector& GroundZero)
{
	UWorld* World = GetWorld();
	ARadixiteNuke* Nuke = IsValid(World)
		                      ? World->SpawnActor<ARadixiteNuke>(ARadixiteNuke::StaticClass(), GroundZero,
		                                                         FRotator::ZeroRotator)
		                      : nullptr;
	if (not IsValid(Nuke))
	{
		BroadcastAbilityEnded();
		return;
	}
	const FGlobalAbilityAreaAimSettings& AreaAim = M_AimSettings.AreaAim;
	Nuke->StartNuke(GroundZero, AreaAim.AreaRadius, AreaAim.InnerRadius, M_NukeSettings, M_NukeVisuals,
	                static_cast<uint8>(GetOwningPlayer()));
	GiveOwnerVisionOfBlast(GroundZero);
	BroadcastAbilityEnded();
}

void UGA_RadixiteNuke::GiveOwnerVisionOfBlast(const FVector& GroundZero) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	AGlobalAbilityVisionBubble* Bubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
		AGlobalAbilityVisionBubble::StaticClass(), GroundZero, FRotator::ZeroRotator);
	if (not IsValid(Bubble))
	{
		return;
	}
	// The player who fired it should see the result, also when it lands deep in the fog.
	Bubble->StartVision(M_AimSettings.AreaAim.AreaRadius,
	                    M_NukeSettings.WarningSeconds + M_NukeSettings.RadiationSeconds,
	                    static_cast<uint8>(GetOwningPlayer()));
}

TArray<FSoftObjectPath> UGA_RadixiteNuke::GetAssetsToLoad() const
{
	return {
		M_NukeVisuals.WarheadMesh.ToSoftObjectPath(),
		M_NukeVisuals.WarheadTrailEffect.ToSoftObjectPath(),
		M_NukeVisuals.DetonationEffect.ToSoftObjectPath(),
		M_NukeVisuals.RadiationEffect.ToSoftObjectPath(),
		M_NukeVisuals.WarningAreaMaterial.ToSoftObjectPath(),
		M_NukeVisuals.ShockwaveMaterial.ToSoftObjectPath(),
		M_NukeVisuals.ScarMaterial.ToSoftObjectPath(),
		M_NukeVisuals.WarningSound.ToSoftObjectPath(),
		M_NukeVisuals.DetonationSound.ToSoftObjectPath()
	};
}
