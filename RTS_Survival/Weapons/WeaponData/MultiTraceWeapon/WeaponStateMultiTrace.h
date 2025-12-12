// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "WeaponStateMultiTrace.generated.h"

/**
 * @brief Init parameters for a multi-socket trace weapon.
 * Extends the regular trace-weapon params with an array of fire sockets and a policy
 * for which direction to use (owner-provided vs per-socket forward).
 */
USTRUCT(BlueprintType, Blueprintable)
struct FInitWeaponStateMultiTrace : public FInitWeaponStatTrace
{
	GENERATED_BODY()

	/** Sockets to fire from. Each socket will emit an async trace when firing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> FireSocketNames;

	/**
	 * If true (default), each trace uses the owner's GetFireDirection(WeaponIndex).
	 * If false, each trace uses the socket's own forward vector at the moment of fire.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseOwnerFireDirection = true;
};


/**
 * @class UWeaponStateMultiTrace
 * @brief A trace weapon that fires from N sockets in parallel (one async line-trace per socket).
 *
 * ## When to use
 * - Multi-barrel MGs/rotaries
 * - Twin-linked MGs
 * - Any muzzle-bank that must trace from each barrel
 *
 * ## Why this class
 * - Reuses the existing trace-weapon pipeline (armor pen, hits, VFX, tracer manager)
 * - Only extends the launch-socket and VFX fan-out logic
 *
 * ## Owner direction
 * - Use a shared fire direction from the owner (default) or each socket’s forward vector.
 *
 * @note Plays one launch sound and spawns one shell case (at its configured socket).
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateMultiTrace : public UWeaponStateTrace
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initialize a multi-socket trace weapon.
	 * Mirrors the regular trace init + adds socket list and owner-direction policy.
	 */
	void InitMultiTraceWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		UWorld* NewWorld,
		const TArray<FName>& FireSocketNames,
		const bool bUseOwnerFireDirection,
		FWeaponVFX NewWeaponVfx,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false,
		const int32 TraceProjectileType = 1);

	/** @inheritdoc */
	virtual void FireWeaponSystem() override final;

	inline int32 GetTraceCount() const { return M_FireSocketNames.Num(); }

protected:
	/**
	 * @brief Spawn launch SFX/VFX for *all* configured sockets.
	 * @param LaunchLocation Ignored (multi-variant spawns per socket).
	 * @param ForwardVector Ignored (multi-variant spawns per socket).
	 * @param bCreateShellCase If true, a single shell-case spawn is triggered once.
	 */
	virtual void CreateLaunchVfx(
		const FVector& LaunchLocation,
		const FVector& ForwardVector,
		const bool bCreateShellCase = true) override;

	/**
	 * @brief Compute launch location and forward vector for a specific socket.
	 * Falls back to FireSocketName if Requested does not exist; zero if no valid socket is available.
	 */
	TPair<FVector, FVector> GetLaunchAndForwardVectorForSocket(const FName& RequestedSocketName) const;

	/**
	 * @brief Validate socket names, strip invalid ones (logs a single warning).
	 * @return true if at least one socket remains valid.
	 */
	bool ValidateAndCacheSockets();

	/** Sockets to emit traces from. */
	UPROPERTY()
	TArray<FName> M_FireSocketNames;

	/** Policy: use owner direction for all traces vs per-socket forward. */
	UPROPERTY()
	bool bM_UseOwnerFireDirection = true;

	/** Cached: whether we already warned about bad sockets. */
	UPROPERTY()
	bool bM_LoggedSocketWarning = false;

private:
	// -------- FireWeaponSystem split helpers ---------------------------------

	/** @return Owner-provided direction for this weapon index (may be zero). */
	FVector FireWeaponSystem_GetSharedOwnerDirection() const;

	/** @return true if preconditions satisfied (owner/world/mesh valid). */
	bool FireWeaponSystem_CheckPreconditions() const;

	/** @brief Handles the no-valid-sockets path with a single fallback trace. */
	void FireWeaponSystem_HandleNoValidSockets(const FVector& SharedOwnerDir);

	/** @brief Dispatch async traces for all sockets. */
	void FireWeaponSystem_DispatchTracesForAllSockets(const FVector& SharedOwnerDir);

	/**
	 * @brief Schedules an async trace from the provided socket, using either owner direction
	 *        or the socket forward vector.
	 */
	void FireWeaponSystem_FireTraceFromSocket(const FName& SocketName, const FVector& SharedOwnerDir);

	// -------- CreateLaunchVfx split helpers ----------------------------------

	/** @brief Plays the launch sound once at the mesh component location (if set). */
	void CreateLaunchVfx_PlayLaunchSoundOnce() const;

	/** @brief Spawns muzzle VFX + optional smoke at all sockets. */
	void CreateLaunchVfx_SpawnAllSockets();

	/** @brief Spawns the muzzle VFX + optional smoke at a single socket. */
	void CreateLaunchVfx_SpawnForSocket(const FName& SocketName);

	/** @brief Spawns a single shell case if requested. */
	void CreateLaunchVfx_MaybeSpawnShellCase(const bool bCreateShellCase) const;

	// -------- GetLaunchAndForwardVectorForSocket split helpers ---------------

	/** @return A socket name that exists on the mesh (fallback to FireSocketName or NAME_None). */
	FName GetLaunchAndForwardVectorForSocket_ResolveSocket(const FName& Requested) const;

	/** @brief Fill out vectors for a resolved socket (assumes it exists). */
	void GetLaunchAndForwardVectorForSocket_FetchVectors(
		const FName& UseSocket,
		FVector& OutLaunchLocation,
		FVector& OutForwardVector) const;
};
