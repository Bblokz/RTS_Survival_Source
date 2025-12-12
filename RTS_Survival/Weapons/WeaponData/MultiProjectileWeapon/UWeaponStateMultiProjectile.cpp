// Copyright (c) Bas Blokzijl. All rights reserved.

#include "UWeaponStateMultiProjectile.h"

#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"

void UWeaponStateMultiProjectile::InitMultiProjectileWeapon(
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
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst)
{
	// 1) pre-validate sockets against the provided mesh so we can pass a safe primary socket to base init
	TArray<FName> ValidSockets;
	ValidSockets.Reserve(FireSocketNames.Num());
	if (IsValid(NewMeshComponent))
	{
		for (const FName& S : FireSocketNames)
		{
			if (!S.IsNone() && NewMeshComponent->DoesSocketExist(S))
			{
				ValidSockets.Add(S);
			}
		}
	}

	// 2) choose the primary socket for the base single-socket init (or NAME_None if none)
	const FName PrimarySocket = (ValidSockets.Num() > 0) ? ValidSockets[0] : NAME_None;

	// 3) run the regular projectile setup so we keep all the pooling/shell adjustments/VFX hooks
	InitProjectileWeapon(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		PrimarySocket, // base uses this one for its GetLaunchAndForwardVector()
		NewWorld,
		ProjectileNiagaraSystem,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);

	// 4) cache our sockets
	M_FireSocketNames = MoveTemp(ValidSockets);
	ValidateAndCacheSockets(); // one extra pass is cheap and catches mesh changes
}

void UWeaponStateMultiProjectile::FireWeaponSystem()
{
	// Preconditions: owner, world, mesh
	if (!WeaponOwner.GetObject() || !IsValid(World) || !IsValid(MeshComponent))
	{
		return;
	}

	// If designer gave no valid sockets, fall back to base behavior (single socket)
	if (!ValidateAndCacheSockets())
	{
		Super::FireWeaponSystem();
		return;
	}

	// We reuse the base projectile pipeline for each socket by temporarily swapping the FireSocketName
	const FName PrevSocket = FireSocketName;

	for (const FName& SocketName : M_FireSocketNames)
	{
		const FName Resolved = ResolveSocketOrFallback(SocketName);
		if (Resolved.IsNone())
		{
			continue;
		}

		// Temporarily set the base weapon's fire socket, then call the base FireWeaponSystem
		FireSocketName = Resolved;
		Super::FireWeaponSystem();
		// will: compute launch from FireSocketName, use owner direction, pull projectile from pool, apply shell data, etc.
	}

	// Restore original socket
	FireSocketName = PrevSocket;
}

void UWeaponStateMultiProjectile::CreateLaunchVfx(
	const FVector& /*LaunchLocation*/,
	const FVector& /*ForwardVector*/,
	const bool bCreateShellCase)
{
	if (!IsValid(World) || !IsValid(MeshComponent))
	{
		return;
	}

	PlayLaunchSoundOnce();
	SpawnMuzzleVfxForAllSockets();

	// Shell case once (let the configured shell-case socket decide where it pops from)
	if (bCreateShellCase)
	{
		WeaponShellCase.SpawnShellCase();
	}
}


bool UWeaponStateMultiProjectile::ValidateAndCacheSockets()
{
	if (!IsValid(MeshComponent))
	{
		return false;
	}

	const int32 OldNum = M_FireSocketNames.Num();
	int32 Kept = 0;

	for (int32 i = 0; i < OldNum; ++i)
	{
		const FName& S = M_FireSocketNames[i];
		if (!S.IsNone() && MeshComponent->DoesSocketExist(S))
		{
			M_FireSocketNames[Kept++] = S;
		}
	}

	if (Kept != OldNum)
	{
		M_FireSocketNames.SetNum(Kept, EAllowShrinking::Yes);
		if (!bM_LoggedSocketWarning)
		{
			bM_LoggedSocketWarning = true;
			// Keep it a single report to avoid spam
			ReportErrorForWeapon(
				TEXT("UWeaponStateMultiProjectile: One or more fire sockets were invalid and were removed."));
		}
	}

	return M_FireSocketNames.Num() > 0;
}

FName UWeaponStateMultiProjectile::ResolveSocketOrFallback(const FName& Requested) const
{
	if (!Requested.IsNone() && MeshComponent->DoesSocketExist(Requested))
	{
		return Requested;
	}
	if (!FireSocketName.IsNone() && MeshComponent->DoesSocketExist(FireSocketName))
	{
		return FireSocketName;
	}
	return NAME_None;
}

void UWeaponStateMultiProjectile::PlayLaunchSoundOnce() const
{
	if (!M_WeaponVfx.LaunchSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		M_WeaponVfx.LaunchSound,
		MeshComponent->GetComponentLocation(),
		FRotator::ZeroRotator,
		1.f, 1.f, 0.f,
		M_WeaponVfx.LaunchAttenuation,
		M_WeaponVfx.LaunchConcurrency);
}

void UWeaponStateMultiProjectile::SpawnMuzzleVfxForAllSockets()
{
	for (const FName& SocketName : M_FireSocketNames)
	{
		if (!MeshComponent->DoesSocketExist(SocketName))
		{
			continue;
		}
		SpawnMuzzleVfxForSocket(SocketName);
	}
}

void UWeaponStateMultiProjectile::SpawnMuzzleVfxForSocket(const FName& SocketName)
{
	const FVector SocketLoc = MeshComponent->GetSocketLocation(SocketName);
	const FRotator SocketRot = MeshComponent->GetSocketQuaternion(SocketName).Rotator();

	CreateLaunchAndSmokeVfx(SocketName, SocketRot, SocketLoc);
}
