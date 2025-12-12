// Copyright (c) Bas Blokzijl. All rights reserved.

#include "WeaponStateMultiTrace.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "Trace/Trace.h"


void UWeaponStateMultiTrace::InitMultiTraceWeapon(
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
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst,
	const int32 TraceProjectileType)
{
	// 1) validate sockets *against the provided mesh* before base init
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

	// 2) choose a safe primary socket for the base (fallback to NAME_None if none)
	const FName PrimarySocket = (ValidSockets.Num() > 0) ? ValidSockets[0] : NAME_None;

	// 3) run normal trace setup with a *real* FireSocketName when available
	InitTraceWeapon(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		PrimarySocket,                
		NewWorld,
		NewWeaponVfx,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst,
		TraceProjectileType);

	// 4) cache config on 'this'
	M_FireSocketNames = MoveTemp(ValidSockets);
	bM_UseOwnerFireDirection = bUseOwnerFireDirection;

	// 5) one more pass is harmless (logs once if designer passed bad sockets and mesh changed)
	ValidateAndCacheSockets();
}
void UWeaponStateMultiTrace::FireWeaponSystem()
{
	if (!FireWeaponSystem_CheckPreconditions())
	{
		return;
	}

	const FVector SharedOwnerDir = FireWeaponSystem_GetSharedOwnerDirection();

	if (!ValidateAndCacheSockets())
	{
		FireWeaponSystem_HandleNoValidSockets(SharedOwnerDir);
		return;
	}

	FireWeaponSystem_DispatchTracesForAllSockets(SharedOwnerDir);
}

FVector UWeaponStateMultiTrace::FireWeaponSystem_GetSharedOwnerDirection() const
{
	return WeaponOwner.GetObject()
		? WeaponOwner->GetFireDirection(WeaponIndex)
		: FVector::ZeroVector;
}

bool UWeaponStateMultiTrace::FireWeaponSystem_CheckPreconditions() const
{
	return WeaponOwner.GetObject() && IsValid(World) && IsValid(MeshComponent);
}

void UWeaponStateMultiTrace::FireWeaponSystem_HandleNoValidSockets(const FVector& SharedOwnerDir)
{
	const TPair<FVector, FVector> Fallback = GetLaunchAndForwardVectorForSocket(NAME_None);
	const FVector LaunchLocation = Fallback.Key;
	const FVector Forward = Fallback.Value;
	const FVector Direction = bM_UseOwnerFireDirection ? SharedOwnerDir : Forward;

	FVector TraceEnd = Direction * WeaponData.Range;
	TraceEnd = FRTSWeaponHelpers::GetTraceEndWithAccuracy(LaunchLocation,
		Direction, WeaponData.Range, WeaponData.Accuracy, bIsAircraftWeapon);

	const float ProjectileLaunchTime = World->GetTimeSeconds();

	FCollisionQueryParams TraceParams(FName(TEXT("FireTrace_Multi_Fallback")), true, nullptr);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;

	FTraceDelegate TraceDelegate;
	TWeakObjectPtr<UWeaponStateMultiTrace> WeakThis(this);
	TraceDelegate.BindLambda(
		[WeakThis, ProjectileLaunchTime, LaunchLocation, TraceEnd]
		(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum) -> void
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis.Get()->OnAsyncTraceComplete(TraceDatum, ProjectileLaunchTime, LaunchLocation, TraceEnd);
		});

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate);
}

void UWeaponStateMultiTrace::FireWeaponSystem_DispatchTracesForAllSockets(const FVector& SharedOwnerDir)
{
	for (const FName& SocketName : M_FireSocketNames)
	{
		FireWeaponSystem_FireTraceFromSocket(SocketName, SharedOwnerDir);
	}
}

void UWeaponStateMultiTrace::FireWeaponSystem_FireTraceFromSocket(const FName& SocketName, const FVector& SharedOwnerDir)
{
	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVectorForSocket(SocketName);
	const FVector LaunchLocation = LaunchAndForward.Key;
	const FVector ForwardVector = LaunchAndForward.Value;

	const FVector Direction = bM_UseOwnerFireDirection ? SharedOwnerDir : ForwardVector;

	FVector TraceEnd = Direction * WeaponData.Range;
	TraceEnd = FRTSWeaponHelpers::GetTraceEndWithAccuracy(LaunchLocation, Direction, WeaponData.Range, WeaponData.Accuracy, bIsAircraftWeapon);

#if 0
	DrawDebugLine(World, LaunchLocation, TraceEnd, FColor::Green, false, 5.f, 0, 2.f);
#endif

	const float ProjectileLaunchTime = World->GetTimeSeconds();

	FCollisionQueryParams TraceParams(FName(TEXT("FireTrace_Multi")), true, nullptr);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;

	FTraceDelegate TraceDelegate;
	TWeakObjectPtr<UWeaponStateMultiTrace> WeakThis(this);
	TraceDelegate.BindLambda(
		[WeakThis, ProjectileLaunchTime, LaunchLocation, TraceEnd]
		(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum) -> void
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis.Get()->OnAsyncTraceComplete(TraceDatum, ProjectileLaunchTime, LaunchLocation, TraceEnd);
		});

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate);
}


void UWeaponStateMultiTrace::CreateLaunchVfx(
	const FVector& /*LaunchLocation*/,
	const FVector& /*ForwardVector*/,
	const bool bCreateShellCase)
{
	if (!IsValid(World) || !IsValid(MeshComponent))
	{
		return;
	}

	CreateLaunchVfx_PlayLaunchSoundOnce();
	CreateLaunchVfx_SpawnAllSockets();
	CreateLaunchVfx_MaybeSpawnShellCase(bCreateShellCase);
}

void UWeaponStateMultiTrace::CreateLaunchVfx_PlayLaunchSoundOnce() const
{
	if (M_WeaponVfx.LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World,
			M_WeaponVfx.LaunchSound,
			MeshComponent->GetComponentLocation(),
			FRotator::ZeroRotator,
			1.f, 1.f, 0.f,
			M_WeaponVfx.LaunchAttenuation,
			M_WeaponVfx.LaunchConcurrency);
	}
}

void UWeaponStateMultiTrace::CreateLaunchVfx_SpawnAllSockets()
{
	for (const FName& SocketName : M_FireSocketNames)
	{
		if (!MeshComponent->DoesSocketExist(SocketName))
		{
			continue;
		}
		CreateLaunchVfx_SpawnForSocket(SocketName);
	}
}

void UWeaponStateMultiTrace::CreateLaunchVfx_SpawnForSocket(const FName& SocketName)
{
	const FVector SocketLoc = MeshComponent->GetSocketLocation(SocketName);
	const FQuat SocketRot = MeshComponent->GetSocketQuaternion(SocketName);
	const FRotator SocketRotator = SocketRot.Rotator();

	CreateLaunchAndSmokeVfx(SocketName, SocketRotator, SocketLoc);

}

void UWeaponStateMultiTrace::CreateLaunchVfx_MaybeSpawnShellCase(const bool bCreateShellCase) const
{
	if (bCreateShellCase)
	{
		WeaponShellCase.SpawnShellCase();
	}
}

TPair<FVector, FVector> UWeaponStateMultiTrace::GetLaunchAndForwardVectorForSocket(const FName& RequestedSocketName) const
{
	FVector LaunchLocation = FVector::ZeroVector;
	FVector ForwardVector = FVector::ForwardVector;

	if (!IsValid(MeshComponent))
	{
		return { LaunchLocation, ForwardVector };
	}

	const FName UseSocket = GetLaunchAndForwardVectorForSocket_ResolveSocket(RequestedSocketName);
	if (UseSocket.IsNone())
	{
		return { LaunchLocation, ForwardVector };
	}

	GetLaunchAndForwardVectorForSocket_FetchVectors(UseSocket, LaunchLocation, ForwardVector);
	return { LaunchLocation, ForwardVector };
}

FName UWeaponStateMultiTrace::GetLaunchAndForwardVectorForSocket_ResolveSocket(const FName& Requested) const
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

void UWeaponStateMultiTrace::GetLaunchAndForwardVectorForSocket_FetchVectors(
	const FName& UseSocket,
	FVector& OutLaunchLocation,
	FVector& OutForwardVector) const
{
	OutLaunchLocation = MeshComponent->GetSocketLocation(UseSocket);
	const FQuat SocketRotation = MeshComponent->GetSocketQuaternion(UseSocket);
	OutForwardVector = SocketRotation.GetForwardVector();

#if 0
	if (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols && IsValid(World))
	{
		DrawDebugLine(World, OutLaunchLocation, OutLaunchLocation + OutForwardVector * 200.f, FColor::Red, false, 0.1f, 0, 5.f);
	}
#endif
}


bool UWeaponStateMultiTrace::ValidateAndCacheSockets()
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
			ReportErrorForWeapon(TEXT("UMultiTraceWeapon: One or more fire sockets were invalid and were removed."));
		}
	}
	return M_FireSocketNames.Num() > 0;
}
