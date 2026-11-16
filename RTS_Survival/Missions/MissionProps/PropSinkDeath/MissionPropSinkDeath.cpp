// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionPropSinkDeath.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Components/PrimitiveComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionPropSinkDeathConstants
{
	constexpr float GroundTraceUp = 300.f;
	constexpr float GroundTraceDown = 800.f;
	// Decals face straight down; yaw spins them in the ground plane.
	constexpr float DecalPitch = -90.f;
	constexpr float FullTurnDegrees = 360.f;
}

void FMissionPropSinkDeath::PreloadAssets(const FMissionPropSinkDeathSettings& Settings,
                                          TArray<TObjectPtr<UObject>>& OutLoadedAssets)
{
	if (not Settings.SmokeSystem.IsNull())
	{
		if (UNiagaraSystem* const Smoke = Settings.SmokeSystem.LoadSynchronous())
		{
			OutLoadedAssets.AddUnique(Smoke);
		}
	}
	for (const FMissionPropDeathDecal& Decal : Settings.Decals)
	{
		for (const TSoftObjectPtr<UMaterialInterface>& Material : Decal.Materials)
		{
			if (Material.IsNull())
			{
				continue;
			}
			if (UMaterialInterface* const Loaded = Material.LoadSynchronous())
			{
				OutLoadedAssets.AddUnique(Loaded);
			}
		}
	}
}

bool FMissionPropSinkDeath::Start(AActor* DyingActor, const FMissionPropSinkDeathSettings& Settings)
{
	if (not IsValid(DyingActor) || DyingActor->IsActorBeingDestroyed())
	{
		return false;
	}
	UWorld* const World = DyingActor->GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	// Read the ground point before the sink moves the actor.
	const FVector GroundPoint = GetGroundPointUnder(DyingActor);
	SpawnDecals(World, GroundPoint, Settings);
	SpawnSmoke(DyingActor, GroundPoint, Settings);
	StopDamageAndWeaponHits(DyingActor);
	StartSink(DyingActor, Settings);
	return true;
}

FVector FMissionPropSinkDeath::GetGroundPointUnder(const AActor* Actor)
{
	if (not IsValid(Actor) || Actor->GetWorld() == nullptr)
	{
		return FVector::ZeroVector;
	}
	const FVector Origin = Actor->GetActorLocation();
	FCollisionQueryParams Params(FName(TEXT("MissionPropSinkDeathGround")), false, Actor);
	FHitResult Hit;
	const bool bHit = Actor->GetWorld()->LineTraceSingleByObjectType(
		Hit,
		Origin + FVector(0.f, 0.f, MissionPropSinkDeathConstants::GroundTraceUp),
		Origin - FVector(0.f, 0.f, MissionPropSinkDeathConstants::GroundTraceDown),
		FCollisionObjectQueryParams(ECC_WorldStatic),
		Params);
	return bHit ? Hit.ImpactPoint : Origin;
}

void FMissionPropSinkDeath::SpawnDecals(UWorld* World, const FVector& GroundPoint,
                                        const FMissionPropSinkDeathSettings& Settings)
{
	using DeveloperSettings::GamePlay::Environment::RTSDecalHeight;
	for (const FMissionPropDeathDecal& Decal : Settings.Decals)
	{
		if (Decal.Materials.IsEmpty())
		{
			continue;
		}
		for (int32 DecalIndex = 0; DecalIndex < Decal.Count; ++DecalIndex)
		{
			const TSoftObjectPtr<UMaterialInterface>& Picked =
				Decal.Materials[FMath::RandRange(0, Decal.Materials.Num() - 1)];
			UMaterialInterface* const Material = Picked.LoadSynchronous();
			if (Material == nullptr)
			{
				RTSFunctionLibrary::ReportError("MissionPropSinkDeath: decal material missing: "
					+ Picked.ToString());
				continue;
			}
			const float Size = FMath::Max(1.f, FMath::RandRange(Decal.SizeRange.X, Decal.SizeRange.Y));
			const FVector Location = GroundPoint + FVector(FMath::RandRange(-Decal.Scatter, Decal.Scatter),
			                                               FMath::RandRange(-Decal.Scatter, Decal.Scatter), 0.f);
			const FRotator Rotation(MissionPropSinkDeathConstants::DecalPitch,
			                        FMath::FRandRange(0.f, MissionPropSinkDeathConstants::FullTurnDegrees), 0.f);
			// Life span 0: the decal stays for the rest of the mission.
			UGameplayStatics::SpawnDecalAtLocation(World, Material, FVector(RTSDecalHeight, Size, Size), Location,
			                                       Rotation, 0.f);
		}
	}
}

void FMissionPropSinkDeath::SpawnSmoke(const AActor* DyingActor, const FVector& GroundPoint,
                                       const FMissionPropSinkDeathSettings& Settings)
{
	if (Settings.SmokeSystem.IsNull())
	{
		return;
	}
	FCollapseFX SmokeFX;
	SmokeFX.CollapseVfx = Settings.SmokeSystem.LoadSynchronous();
	if (SmokeFX.CollapseVfx == nullptr)
	{
		RTSFunctionLibrary::ReportError("MissionPropSinkDeath: smoke system missing: "
			+ Settings.SmokeSystem.ToString());
		return;
	}
	SmokeFX.VfxScale = Settings.SmokeScale;
	SmokeFX.CollapseNiagaraParameters = Settings.SmokeParameters;
	const FVector SmokeLocation = GroundPoint + Settings.SmokeOffset;
	SmokeFX.SpawnVfxAtLocation(DyingActor, SmokeLocation);

	UWorld* const World = DyingActor->GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// Later bursts run on world timers: the actor is destroyed when the sink ends.
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	const TWeakObjectPtr<UNiagaraSystem> WeakSystem(SmokeFX.CollapseVfx);
	for (int32 BurstIndex = 1; BurstIndex < Settings.SmokeBurstCount; ++BurstIndex)
	{
		FTimerHandle BurstHandle;
		World->GetTimerManager().SetTimer(BurstHandle, FTimerDelegate::CreateLambda(
			                                  [WeakWorld, WeakSystem, SmokeFX, SmokeLocation]()
			                                  {
				                                  UWorld* const BurstWorld = WeakWorld.Get();
				                                  if (BurstWorld == nullptr || not WeakSystem.IsValid())
				                                  {
					                                  return;
				                                  }
				                                  SmokeFX.SpawnVfxAtLocation(BurstWorld, SmokeLocation);
			                                  }), Settings.SmokeBurstInterval * BurstIndex, false);
	}
}

void FMissionPropSinkDeath::StopDamageAndWeaponHits(AActor* DyingActor)
{
	// Counts as dead: weapons drop it as a target and a destructible (such as a scavengable wreck) never runs its own
	// death, which would collapse and destroy it before the sink ends.
	DyingActor->SetUnitDying();
	DyingActor->SetCanBeDamaged(false);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(DyingActor);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		if (not IsValid(Primitive))
		{
			continue;
		}
		Primitive->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
	}
}

void FMissionPropSinkDeath::StartSink(AActor* DyingActor, const FMissionPropSinkDeathSettings& Settings)
{
	FRTSVerticalCollapseSettings Sink;
	Sink.DeltaZ = Settings.SinkDepth;
	Sink.CollapseTime = Settings.SinkSeconds;
	Sink.MinPitchRoll = Settings.MaxTiltDegrees * 0.5f;
	Sink.MaxPitchRoll = Settings.MaxTiltDegrees;
	Sink.bDestroyPostVerticalCollapse = true;
	// The smoke is spawned separately so it stays at the ground point.
	FRTS_VerticalCollapse::StartVerticalCollapse(DyingActor, Sink, FCollapseFX());
}
