// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallDamageShared.h"

#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"

namespace WallDamageSharedConstants
{
	// Pieces leave the face the shell went through, spread away from the impact and get an upward kick.
	constexpr float MinOutwardSpeed = 180.f;
	constexpr float MaxOutwardSpeed = 420.f;
	constexpr float MaxSpreadSpeed = 220.f;
	constexpr float MinUpKick = 80.f;
	constexpr float MaxUpKick = 300.f;
	// Tumble, in rad/s.
	constexpr float MinSpinSpeed = 2.f;
	constexpr float MaxSpinSpeed = 9.f;
	constexpr float GroundTraceDepth = 2000.f;
	// A point event without a location is treated as splash.
	constexpr float MinValidHitLocationSize = 1.f;
	// Without a known shot (a collapse), each piece falls towards a random face, slower.
	constexpr float NoShotOutwardSpeedScale = 0.6f;
}

namespace
{
	FVector4 MakePieceVelocity(const FVector& Centre, const FVector& FaceNormal, const float ShotAlongNormal,
	                           const FVector& HitLocation, const float SpeedScale)
	{
		using namespace WallDamageSharedConstants;
		const bool bHasShotSide = FMath::Abs(ShotAlongNormal) > UE_KINDA_SMALL_NUMBER;
		const float Side = bHasShotSide ? FMath::Sign(ShotAlongNormal) : (FMath::RandBool() ? 1.f : -1.f);
		const float OutwardScale = bHasShotSide ? 1.f : NoShotOutwardSpeedScale;
		const FVector Spread = FVector::VectorPlaneProject(Centre - HitLocation, FaceNormal).GetSafeNormal();
		const FVector Velocity = FaceNormal * (Side * FMath::FRandRange(MinOutwardSpeed, MaxOutwardSpeed) * OutwardScale
				* SpeedScale)
			+ Spread * FMath::FRandRange(0.f, MaxSpreadSpeed) * SpeedScale
			+ FVector::UpVector * FMath::FRandRange(MinUpKick, MaxUpKick) * SpeedScale;
		return FVector4(Velocity, 0.0);
	}

	void SpawnBurst(const AActor& Owner, UNiagaraSystem* DebrisSystem, UStaticMesh* DebrisMesh,
	                const TConstArrayView<FWallDebrisPiece> Pieces, const FQuat& WallRotation, const FVector& HitLocation,
	                const FVector& ShotDirection, const float SpeedScale)
	{
		using namespace AddOnArmorPanelDebris;
		using namespace WallDamageSharedConstants;
		const FVector FaceNormal = WallRotation.RotateVector(FVector::YAxisVector);
		const float ShotAlongNormal = static_cast<float>(FVector::DotProduct(ShotDirection, FaceNormal));
		const FVector4 Orientation(WallRotation.X, WallRotation.Y, WallRotation.Z, WallRotation.W);
		TArray<FVector4> Starts;
		TArray<FVector4> Velocities;
		TArray<FVector4> Spins;
		TArray<FVector4> Orientations;
		FVector BurstCentre = FVector::ZeroVector;
		for (const FWallDebrisPiece& Piece : Pieces)
		{
			Starts.Add(FVector4(Piece.WorldCentre, Piece.Scale));
			Velocities.Add(MakePieceVelocity(Piece.WorldCentre, FaceNormal, ShotAlongNormal, HitLocation, SpeedScale));
			Spins.Add(FVector4(FMath::VRand(), FMath::FRandRange(MinSpinSpeed, MaxSpinSpeed)));
			Orientations.Add(Orientation);
			BurstCentre += Piece.WorldCentre;
		}
		BurstCentre /= Pieces.Num();
		UNiagaraComponent* const Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Owner.GetWorld(), DebrisSystem, BurstCentre, FRotator::ZeroRotator, FVector::OneVector, false, false,
			ENCPoolMethod::AutoRelease, false);
		if (not IsValid(Burst))
		{
			return;
		}
		Burst->SetVariableStaticMesh(DebrisMeshParameter, DebrisMesh);
		Burst->SetVariableInt(PieceCountParameter, Pieces.Num());
		Burst->SetVariableFloat(GroundHeightParameter, WallDamageShared::GetGroundHeightBelow(Owner, BurstCentre));
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceStartParameter, Starts);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceVelocityParameter, Velocities);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceSpinParameter, Spins);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceOrientationParameter, Orientations);
		Burst->Activate(true);
	}
}

bool WallDamageShared::ResolveHitLocation(const FDamageEvent& DamageEvent, const AActor* DamageCauser,
                                          FVector& OutWorldLocation, FVector& OutShotDirection, bool& bOutIsDirectHit)
{
	using namespace WallDamageSharedConstants;
	bOutIsDirectHit = false;
	OutShotDirection = FVector::ZeroVector;
	if (IsValid(DamageCauser))
	{
		const FVector CauserVelocity = DamageCauser->GetVelocity();
		OutShotDirection = CauserVelocity.IsNearlyZero()
			                   ? DamageCauser->GetActorForwardVector()
			                   : CauserVelocity.GetSafeNormal();
	}
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (PointEvent.HitInfo.Location.Size() > MinValidHitLocationSize)
		{
			OutWorldLocation = PointEvent.HitInfo.Location;
			bOutIsDirectHit = true;
			if (not PointEvent.ShotDirection.IsNearlyZero())
			{
				OutShotDirection = PointEvent.ShotDirection.GetSafeNormal();
			}
			return true;
		}
	}
	// Splash: the explosion is where the causer is.
	if (not IsValid(DamageCauser))
	{
		return false;
	}
	OutWorldLocation = DamageCauser->GetActorLocation();
	return true;
}

float WallDamageShared::ResolveHoleRadius(const FWallChunkHoleSettings& Settings, const float DamageAmount,
                                          const AActor* DamageCauser, const float PlaneDistance)
{
	if (const AProjectile* const Projectile = Cast<AProjectile>(DamageCauser))
	{
		return ResolveHoleRadiusFromCalibre(Settings, static_cast<float>(Projectile->GetWeaponCalibre()), PlaneDistance);
	}
	const float Radius = FMath::Sqrt(FMath::Max(0.f, DamageAmount)) * Settings.HoleRadiusPerSqrtDamage - PlaneDistance;
	return FMath::Clamp(Radius, 0.f, Settings.MaxHoleRadius);
}

float WallDamageShared::ResolveHoleRadiusFromCalibre(const FWallChunkHoleSettings& Settings, const float CalibreMm,
                                                     const float PlaneDistance)
{
	if (CalibreMm < static_cast<float>(Settings.MinCalibreMm))
	{
		return 0.f;
	}
	const float Radius = CalibreMm * Settings.HoleRadiusPerCalibreMm - PlaneDistance;
	return FMath::Clamp(Radius, 0.f, Settings.MaxHoleRadius);
}

float WallDamageShared::GetGroundHeightBelow(const AActor& Owner, const FVector& WorldLocation)
{
	const UWorld* const World = Owner.GetWorld();
	if (not IsValid(World))
	{
		return static_cast<float>(Owner.GetActorLocation().Z);
	}
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallDebrisGround), false, &Owner);
	const FVector TraceEnd = WorldLocation - FVector(0.0, 0.0, WallDamageSharedConstants::GroundTraceDepth);
	if (World->LineTraceSingleByChannel(GroundHit, WorldLocation, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return static_cast<float>(GroundHit.ImpactPoint.Z);
	}
	return static_cast<float>(Owner.GetActorLocation().Z);
}

void WallDamageShared::PlayCollapseSound(const AActor& Owner, const FWallChunkCollapseSettings& Settings,
                                         const FVector& WorldLocation)
{
	if (not IsValid(Settings.CollapseSound) || Owner.IsHidden())
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(&Owner, Settings.CollapseSound, WorldLocation, FRotator::ZeroRotator, 1.f, 1.f,
	                                      0.f, Settings.CollapseAttenuation, Settings.CollapseConcurrency);
}

void WallDamageShared::ThrowDebrisPieces(const AActor& Owner, UNiagaraSystem* const DebrisSystem,
                                         const TArray<TObjectPtr<UStaticMesh>>& DebrisMeshes,
                                         const TConstArrayView<FWallDebrisPiece> Pieces, const FQuat& WallRotation,
                                         const FVector& HitLocation, const FVector& ShotDirection,
                                         const float SpeedScale)
{
	// Fog of war hides enemy structures; their debris would give them away.
	if (Pieces.IsEmpty() || not IsValid(DebrisSystem) || Owner.IsHidden())
	{
		return;
	}
	for (int32 MeshIndex = 0; MeshIndex < DebrisMeshes.Num(); ++MeshIndex)
	{
		if (not IsValid(DebrisMeshes[MeshIndex]))
		{
			continue;
		}
		// One burst per debris mesh (User.DebrisMesh is one mesh per Niagara component), split at the burst size.
		TArray<FWallDebrisPiece, TInlineAllocator<AddOnArmorPanelDebris::MaxPiecesPerBurst>> BurstPieces;
		for (const FWallDebrisPiece& Piece : Pieces)
		{
			if (Piece.MeshIndex != MeshIndex)
			{
				continue;
			}
			BurstPieces.Add(Piece);
			if (BurstPieces.Num() == AddOnArmorPanelDebris::MaxPiecesPerBurst)
			{
				SpawnBurst(Owner, DebrisSystem, DebrisMeshes[MeshIndex], BurstPieces, WallRotation, HitLocation,
				           ShotDirection, SpeedScale);
				BurstPieces.Reset();
			}
		}
		if (not BurstPieces.IsEmpty())
		{
			SpawnBurst(Owner, DebrisSystem, DebrisMeshes[MeshIndex], BurstPieces, WallRotation, HitLocation,
			           ShotDirection, SpeedScale);
		}
	}
}
