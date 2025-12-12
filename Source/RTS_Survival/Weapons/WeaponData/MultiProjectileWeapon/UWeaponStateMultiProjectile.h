// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "UWeaponStateMultiProjectile.generated.h"

/**
 * @brief Init parameters for a multi-socket projectile weapon.
 * Extends the regular projectile-weapon params with an array of fire sockets.
 */
USTRUCT(BlueprintType, Blueprintable)
struct FInitWeaponStateMultiProjectile : public FInitWeaponStateProjectile
{
	GENERATED_BODY()

	/** Sockets to fire from. Each socket will launch a projectile when the weapon fires. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> FireSocketNames;
};

/**
 * @class UWeaponStateMultiProjectile
 * @brief A projectile weapon that spawns a projectile from N sockets simultaneously
 *        for every single bullet fired by the base weapon state.
 *
 * Notes:
 * - Reuses the base projectile pipeline (pool manager, shell adjustments, VFX map, etc).
 * - Uses the owner's GetFireDirection(WeaponIndex) for launch rotation, same as single projectile.
 * - Plays launch SFX once; spawns muzzle VFX/smoke at every socket; spawns one shell case.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateMultiProjectile : public UWeaponStateProjectile
{
	GENERATED_BODY()

public:
	/** Initialize using explicit params. */
	void InitMultiProjectileWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		UWorld* NewWorld,
		const TArray<FName>& FireSocketNames,
		const EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false);

	inline int32 GetProjectileCount() const { return M_FireSocketNames.Num(); }

protected:
	/** Spawns one projectile per configured socket. */
	virtual void FireWeaponSystem() override;

	/** Spawns muzzle VFX/smoke for all sockets and one shell case; plays launch SFX once. */
	virtual void CreateLaunchVfx(
		const FVector& LaunchLocation,
		const FVector& ForwardVector,
		const bool bCreateShellCase = true) override;

private:
	// ---- Sockets management --------------------------------------------------

	/** The sockets we will fire from. */
	UPROPERTY()
	TArray<FName> M_FireSocketNames;

	/** Whether we've already logged a sockets warning once. */
	UPROPERTY()
	bool bM_LoggedSocketWarning = false;

	/**
	 * Validate socket names against the current MeshComponent; strips invalid ones.
	 * @return true if at least one valid socket remains.
	 */
	bool ValidateAndCacheSockets();

	/** Resolve a usable socket name (requested, then fallback to FireSocketName, else NAME_None). */
	FName ResolveSocketOrFallback(const FName& Requested) const;

	// ---- VFX helpers --------------------------------------------------------

	/** Play launch sound once at the component location (if configured). */
	void PlayLaunchSoundOnce() const;

	/** Spawn launch VFX/smoke at all sockets. */
	void SpawnMuzzleVfxForAllSockets();

	/** Spawn launch VFX/smoke at a single socket. */
	void SpawnMuzzleVfxForSocket(const FName& SocketName);
};
