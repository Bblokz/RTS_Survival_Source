// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "WallDamageShared.generated.h"

class AActor;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UStaticMesh;
struct FDamageEvent;

// Settings and helpers shared by the two wall damage prototypes: baked chunks (UWallChunkDamageComponent) and
// procedural holes (UWallHoleDamageComponent). Docs/Environment/WallChunkDamage.md.

USTRUCT(BlueprintType)
struct FWallChunkHoleSettings
{
	GENERATED_BODY()

	// Shells below this calibre chip the wall without knocking anything out.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	int32 MinCalibreMm = 20;

	// Hole radius per mm of shell calibre: 75 mm -> 64 cm, 150 mm -> 128 cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float HoleRadiusPerCalibreMm = 0.85f;

	// For hits without a projectile (bombs, abilities, hitscan): hole radius per square root of the damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float HoleRadiusPerSqrtDamage = 5.f;

	// Chunks: a direct hit with at least this hole radius always knocks out the nearest chunk, smaller ones by chance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float GuaranteedChunkRadius = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float MaxHoleRadius = 220.f;

	// While the wall stands, at most this share of it times its lost health is gone; death takes the rest.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaxHiddenShareWhileAlive = 0.65f;
};

USTRUCT(BlueprintType)
struct FWallChunkCollapseSettings
{
	GENERATED_BODY()

	// Replaces the geometry-collection collapse: everything left flies off and the wall is destroyed later.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	bool bCollapseIntoChunksOnDeath = true;

	// After the debris has settled and shrunk away.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float DestroyOwnerDelaySeconds = 4.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	float DeathSpeedScale = 1.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	TObjectPtr<USoundBase> CollapseSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	TObjectPtr<USoundAttenuation> CollapseAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallDamage")
	TObjectPtr<USoundConcurrency> CollapseConcurrency;
};

// One flying piece: where it starts, its uniform scale and which debris mesh it uses.
struct FWallDebrisPiece
{
	FVector WorldCentre = FVector::ZeroVector;
	float Scale = 1.f;
	int32 MeshIndex = INDEX_NONE;
};

namespace WallDamageShared
{
	/**
	 * @brief Point events carry the impact; any other event uses the causer's position (splash).
	 * @param DamageEvent The owner's damage event.
	 * @param DamageCauser Projectile, bomb or ability actor; may be null.
	 * @param OutWorldLocation Impact or explosion position.
	 * @param OutShotDirection Shell direction, zero when unknown.
	 * @param bOutIsDirectHit True for a point event with a location.
	 * @return Whether a location was found.
	 */
	bool ResolveHitLocation(const FDamageEvent& DamageEvent, const AActor* DamageCauser, FVector& OutWorldLocation,
	                        FVector& OutShotDirection, bool& bOutIsDirectHit);

	/**
	 * @brief Hole radius on the wall face from the projectile's calibre, else from the damage.
	 * @param Settings Hole settings of the wall.
	 * @param DamageAmount Damage the hit dealt.
	 * @param DamageCauser Projectile (calibre) or another actor.
	 * @param PlaneDistance How far outside the slab the hit lies (splash); shrinks the hole.
	 * @return Radius in cm; zero when the hit removes nothing.
	 */
	float ResolveHoleRadius(const FWallChunkHoleSettings& Settings, float DamageAmount, const AActor* DamageCauser,
	                        float PlaneDistance);
	float ResolveHoleRadiusFromCalibre(const FWallChunkHoleSettings& Settings, float CalibreMm, float PlaneDistance);

	float GetGroundHeightBelow(const AActor& Owner, const FVector& WorldLocation);

	void PlayCollapseSound(const AActor& Owner, const FWallChunkCollapseSettings& Settings, const FVector& WorldLocation);

	/**
	 * @brief Throws pieces through the pooled panel debris system: one burst per mesh, at most 20 pieces each.
	 * @param Owner The wall; nothing flies while fog of war hides it.
	 * @param DebrisSystem NS_AddOnArmorPanelDebris.
	 * @param DebrisMeshes Meshes the pieces index into.
	 * @param Pieces Pieces to throw.
	 * @param WallRotation Wall mesh rotation: pieces start in it, and its Y axis is the face normal.
	 * @param HitLocation Pieces spread away from it on the face.
	 * @param ShotDirection Pieces leave the face the shell went through; zero picks a face per piece.
	 * @param SpeedScale Death throws harder than a hit.
	 */
	void ThrowDebrisPieces(const AActor& Owner, UNiagaraSystem* DebrisSystem,
	                       const TArray<TObjectPtr<UStaticMesh>>& DebrisMeshes, TConstArrayView<FWallDebrisPiece> Pieces,
	                       const FQuat& WallRotation, const FVector& HitLocation, const FVector& ShotDirection,
	                       float SpeedScale);
}
