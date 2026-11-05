// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_Sonnengewehr.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SonnengewehrAbilityConstants
{
	// The player who fired it should see the burnt strip, also deep in the fog.
	constexpr float VisionAfterSweepSeconds = 6.f;
	constexpr float MinVisionRadiusCm = 1200.f;
	constexpr float VisionRadiusPerHalfWidth = 3.f;
	// Neighbouring vision bubbles overlap so the strip has no blind gaps.
	constexpr float VisionSpacingFraction = 1.5f;
}

void UGA_Sonnengewehr::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	const FVector Direction = GetDirectionForPointExecution(TargetLocation);
	const FVector HalfLine = Direction * (M_AimSettings.AreaAim.MaxStripLength * 0.5f);
	ExecuteAbilityAlongLine(TargetLocation - HalfLine, TargetLocation + HalfLine);
}

void UGA_Sonnengewehr::ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd)
{
	ResetAbilityEndedBroadcast();
	FVector StripStart = LineStart;
	FVector StripEnd = LineEnd;
	ClampStripToAim(StripStart, StripEnd);
	if (FVector::Dist2D(StripStart, StripEnd) <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Sonnengewehr got a strip without length."));
		BroadcastAbilityEnded();
		return;
	}
	GlobalAbilityHelpers::TryFindGroundLocation(this, StripStart, StripStart);
	GlobalAbilityHelpers::TryFindGroundLocation(this, StripEnd, StripEnd);

	++M_StrikesInFlight;
	TWeakObjectPtr<UGA_Sonnengewehr> WeakThis(this);
	M_LoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		GetAssetsToLoad(),
		FStreamableDelegate::CreateLambda([WeakThis, StripStart, StripEnd]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded(StripStart, StripEnd);
			}
		})));
	// The base execution places the marker and plays the order sound; it does not route back into this class.
	Super::ExecuteAbilityAtLocation((StripStart + StripEnd) * 0.5f);
}

void UGA_Sonnengewehr::BeginDestroy()
{
	M_LoadHandles.Reset();
	Super::BeginDestroy();
}

void UGA_Sonnengewehr::ClampStripToAim(FVector& InOutLineStart, FVector& InOutLineEnd) const
{
	const FVector Line(InOutLineEnd.X - InOutLineStart.X, InOutLineEnd.Y - InOutLineStart.Y, 0.f);
	const float MaxLength = M_AimSettings.AreaAim.MaxStripLength;
	if (Line.Size() <= MaxLength)
	{
		return;
	}
	// Missions can pass any line; the beam keeps its start and stops at the longest sweep it has.
	InOutLineEnd = InOutLineStart + Line.GetSafeNormal() * MaxLength;
}

FVector UGA_Sonnengewehr::GetDirectionForPointExecution(const FVector& TargetLocation) const
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return FVector::ForwardVector;
	}
	// Same direction the strip abilities with aircraft use: from where the owner's aircraft would come in.
	const FVector SpawnLocation = GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, TargetLocation);
	const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? FVector::ForwardVector : Direction;
}

void UGA_Sonnengewehr::OnAssetsLoaded(const FVector& StripStart, const FVector& StripEnd)
{
	UWorld* World = GetWorld();
	ASonnengewehrBeam* Beam = IsValid(World)
		                          ? World->SpawnActor<ASonnengewehrBeam>(ASonnengewehrBeam::StaticClass(), StripStart,
		                                                                 FRotator::ZeroRotator)
		                          : nullptr;
	if (not IsValid(Beam))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Sonnengewehr could not spawn its beam."));
		OnStrikeOver();
		return;
	}
	Beam->OnSweepFinished.AddUObject(this, &UGA_Sonnengewehr::OnBeamSweepFinished);
	Beam->StartStrike(StripStart, StripEnd, M_AimSettings.AreaAim.AreaRadius, M_SonnengewehrSettings,
	                  M_SonnengewehrVisuals, static_cast<uint8>(GetOwningPlayer()));
	GiveOwnerVisionOfStrip(StripStart, StripEnd);
	OnBeamSpawned.Broadcast(this, Beam);
}

void UGA_Sonnengewehr::OnBeamSweepFinished(ASonnengewehrBeam* Beam)
{
	if (IsValid(Beam))
	{
		Beam->OnSweepFinished.RemoveAll(this);
	}
	OnStrikeOver();
}

void UGA_Sonnengewehr::OnStrikeOver()
{
	M_StrikesInFlight = FMath::Max(0, M_StrikesInFlight - 1);
	if (M_StrikesInFlight == 0)
	{
		BroadcastAbilityEnded();
	}
}

void UGA_Sonnengewehr::GiveOwnerVisionOfStrip(const FVector& StripStart, const FVector& StripEnd) const
{
	using namespace SonnengewehrAbilityConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FSonnengewehrSettings& Settings = M_SonnengewehrSettings;
	const float Seconds = Settings.WarningSeconds + Settings.IgnitionSeconds + Settings.SweepSeconds
		+ Settings.FadeSeconds + VisionAfterSweepSeconds;
	const float Radius = FMath::Max(MinVisionRadiusCm, M_AimSettings.AreaAim.AreaRadius * VisionRadiusPerHalfWidth);
	const float Length = FVector::Dist2D(StripStart, StripEnd);
	const int32 Segments = FMath::Max(1, FMath::CeilToInt(Length / (Radius * VisionSpacingFraction)));
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const FVector Location = FMath::Lerp(StripStart, StripEnd, static_cast<float>(Index) / Segments);
		AGlobalAbilityVisionBubble* Bubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
			AGlobalAbilityVisionBubble::StaticClass(), Location, FRotator::ZeroRotator);
		if (IsValid(Bubble))
		{
			Bubble->StartVision(Radius, Seconds, static_cast<uint8>(GetOwningPlayer()));
		}
	}
}

TArray<FSoftObjectPath> UGA_Sonnengewehr::GetAssetsToLoad() const
{
	const FSonnengewehrVisuals& Visuals = M_SonnengewehrVisuals;
	return {
		Visuals.BeamMaterial.ToSoftObjectPath(),
		Visuals.ImpactMaterial.ToSoftObjectPath(),
		Visuals.ScorchMaterial.ToSoftObjectPath(),
		Visuals.WarningAreaMaterial.ToSoftObjectPath(),
		Visuals.WarningSound.ToSoftObjectPath(),
		Visuals.BeamSound.ToSoftObjectPath()
	};
}
