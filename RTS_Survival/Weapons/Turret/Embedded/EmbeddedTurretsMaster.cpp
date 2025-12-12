// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.


#include "EmbeddedTurretsMaster.h"

#include "EmbededTurretInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"


AEmbeddedTurretsMaster::AEmbeddedTurretsMaster():
	EmbeddedTurretMesh(nullptr),
	M_TurretHeight(0),
	bM_IsAssaultTurret(false),
	M_MaxYaw(0),
	M_MinYaw(0)
{
}

void AEmbeddedTurretsMaster::InitEmbeddedTurret(
    USkeletalMeshComponent* NewEmbeddedTurretMesh,
    const float TurretHeight,
    const bool bIsAssaultTurret,
    const float MinTurretYaw,
    const float MaxTurretYaw,
    TScriptInterface<IEmbeddedTurretInterface> NewEmbeddedOwner,
    const bool bIsArtillery,
    const float ArtilleryDistanceUseMaxPitch)
{
    EmbeddedTurretMesh = NewEmbeddedTurretMesh;
    M_TurretHeight = TurretHeight;
    bM_IsAssaultTurret = bIsAssaultTurret;
    M_MinYaw = MinTurretYaw;
    M_MaxYaw = MaxTurretYaw;
    EmbeddedOwner = NewEmbeddedOwner;
    bM_IsArtillery = bIsArtillery;
    M_ArtilleryDistanceUseMaxPitch = ArtilleryDistanceUseMaxPitch;

    // ---  cache base local yaw once for Idle_Base ---
    M_EmbeddedBaseLocalYaw = ComputeEmbeddedBaseLocalYaw();
    if (bM_IsAssaultTurret)
    {
        M_EmbeddedBaseLocalYaw = ClampYawToLimits(M_EmbeddedBaseLocalYaw);
    }
}

void AEmbeddedTurretsMaster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AEmbeddedTurretsMaster::BeginPlay()
{
	Super::BeginPlay();
}

FTransform AEmbeddedTurretsMaster::GetTurretTransform() const
{
	if (EmbeddedOwner.GetObject() != nullptr && IsValid(EmbeddedTurretMesh))
	{
		FTransform TurretTransform = EmbeddedTurretMesh->GetComponentTransform();
		FRotator TurretRotator = TurretTransform.Rotator();
		TurretRotator.Yaw = IEmbeddedTurretInterface::Execute_GetCurrentTurretAngle(EmbeddedOwner.GetObject());
		TurretTransform.SetRotation(TurretRotator.Quaternion());

		FVector TurretLocation = TurretTransform.GetLocation();
		TurretLocation.Z += M_TurretHeight;
		TurretTransform.SetLocation(TurretLocation);
		return TurretTransform;
	}
	RTSFunctionLibrary::PrintString("TANK DOES NOT IMPLEMENT EMBEDDED TURRET INTERFACE"
		"\n OR MESH IS NOT VALID");
	return FTransform();
}

FRotator AEmbeddedTurretsMaster::GetTargetRotation(const FVector& TargetLocation, const FVector& TurretLocation) const
{
	// Transform the target location into the turret's local space
	const FVector LocalTargetLocation = EmbeddedTurretMesh->GetComponentTransform().InverseTransformPosition(
		TargetLocation);

	// Calculate the rotation that makes a forward vector point at the target location. In this case
	// we effectively calculate the rotation from the local forward direction which is the zero vector in local space.
	return UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, LocalTargetLocation);
}

void AEmbeddedTurretsMaster::UpdateTargetPitchCPP(float NewPitch)
{
	// Base desired pitch from the look-at result, clamped to turret limits.
	float DesiredPitch = FMath::Clamp(NewPitch, M_MinTurretPitch, M_MaxTurretPitch);

	// Artillery: if target is far enough, force max pitch for arc shots (works for Actor or Ground).
	if (bM_IsArtillery && HasValidTarget())
	{
		const FVector TurretLoc = GetTurretTransform().GetLocation();
		const FVector TgtLoc    = GetActiveTargetLocation();
		const float   Distance  = FVector::Dist(TgtLoc, TurretLoc);

		if (Distance > M_ArtilleryDistanceUseMaxPitch)
		{
			DesiredPitch = M_MaxTurretPitch;
		}
	}

	// Store on master for weapons to consume.
	TargetPitch = DesiredPitch;

	// Drive the embedded owner anim/logic.
	if (EmbeddedOwner.GetObject() != nullptr)
	{
		IEmbeddedTurretInterface::Execute_UpdateTargetPitch(EmbeddedOwner.GetObject(), TargetPitch);
	}

	if constexpr (DeveloperSettings::Debugging::GEmbedded_Turret_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Original pitch: " + FString::SanitizeFloat(NewPitch));
		RTSFunctionLibrary::PrintString("New calculated pitch: " + FString::SanitizeFloat(TargetPitch));
	}
}


void AEmbeddedTurretsMaster::RotateTurret(const float DeltaTime)
{
    if (!GetIsValidEmbeddedOwner())
    {
        RTSFunctionLibrary::PrintString("TANK DOES NOT IMPLEMENT EMBEDDED TURRET INTERFACE");
        return;
    }

    // Detect Idle_Base “mode”: no valid target + Idle_Base policy
    const bool bIsIdleBase =
        (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Base) && !HasValidTarget();

    if (bIsIdleBase)
    {
        // Set the local base target once (no continuous recompute)
        if (!bM_UseLocalIdleBaseTarget_Embedded)
        {
            SetEmbeddedIdleBaseTarget();
        }

        // Fall through to your existing rotation using LocalTargetYaw = M_TargetRotator.Yaw
        // (this keeps the smooth rotation behaviour)
    }
    else
    {
        // Any active targeting mode or different idle policy => let normal logic run
        bM_UseLocalIdleBaseTarget_Embedded = false;
    }

    const FRotator Current = GetTurretTransform().Rotator();
    const float CurrentYaw = Current.Yaw;
    const float LocalTargetYaw = SteeringState.M_TargetRotator.Yaw;

    const float DeltaAngle = FMath::FindDeltaAngleDegrees(CurrentYaw, LocalTargetYaw);
    if (bM_IsAssaultTurret)
    {
        AssaultTurretRotation(CurrentYaw, LocalTargetYaw, DeltaAngle, DeltaTime);
        return;
    }

    bM_IsRotatedToEngage = FMath::Abs(DeltaAngle) <= AllowedDegreesOffTarget;
    NormalTurretRotation(CurrentYaw, LocalTargetYaw, DeltaAngle, DeltaTime);
}

void AEmbeddedTurretsMaster::NormalTurretRotation(
	const float& CurrentYaw,
	const float& LocalTargetYaw,
	const float& DeltaAngle,
	const float& DeltaTime)
{
	// RTSFunctionLibrary::PrintString("DeltaAngle: " + FString::SanitizeFloat(DeltaAngle));
	// RTSFunctionLibrary::PrintString("LocalTargetyaw: " + FString::SanitizeFloat(LocalTargetYaw));
	if (FMath::Abs(DeltaAngle) <= DeltaTime * RotationSpeed)
	{
		// Embedded Owner is check for validity beforehand.
		IEmbeddedTurretInterface::Execute_SetTurretAngle(EmbeddedOwner.GetObject(), LocalTargetYaw);
		bM_IsRotatedToEngage = true;
		StopTurretRotation();
		return;
	}
	// Positive or negative direction.
	const int direction = FMath::Sign(DeltaAngle);
	const float yawIncreaseFactor = RotationSpeed * direction * DeltaTime;
	// Set the new turret angle using the interface method.
	// Embedded Owner is checked for validity beforehand.
	IEmbeddedTurretInterface::Execute_SetTurretAngle(EmbeddedOwner.GetObject(), CurrentYaw + yawIncreaseFactor);
	if constexpr (DeveloperSettings::Debugging::GEmbedded_Turret_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Rotating", FColor::Purple);
		RTSFunctionLibrary::PrintString("Target yaw :" + FString::SanitizeFloat(LocalTargetYaw)
		                                + "\n current yaw: " + FString::SanitizeFloat(CurrentYaw), FColor::Red);
	}
}

void AEmbeddedTurretsMaster::AssaultTurretRotation(
	const float& CurrentYaw,
	const float& LocalTargetYaw,
	const float& DeltaAngle,
	const float& DeltaTime)

{
	float NewYaw = CurrentYaw;
	if (FMath::Abs(DeltaAngle) <= DeltaTime * RotationSpeed)
	{
		// Embedded Owner is checked for validity beforehand.
		IEmbeddedTurretInterface::Execute_SetTurretAngle(EmbeddedOwner.GetObject(), LocalTargetYaw);
		bM_IsRotatedToEngage = true;
		StopTurretRotation();
		return;
	}
	// Check if the target yaw is within the turret's yaw bounds.
	if (LocalTargetYaw >= M_MinYaw && LocalTargetYaw <= M_MaxYaw)
	{
		// Positive or negative direction.
		const int Direction = FMath::Sign(DeltaAngle);
		const float YawIncreaseFactor = RotationSpeed * Direction * DeltaTime;
		NewYaw += YawIncreaseFactor;
	}
	else
	{
		// The target is out of yaw bounds. Turn the turret to its min or max yaw, depending on the target's position.
		NewYaw = (DeltaAngle > 0) ? M_MaxYaw : M_MinYaw;
		// Only update when the target rotator is updated otherwise spamming this will overshoot the hull. 
		if (bIsTargetRotatorUpdated)
		{
			EmbeddedOwner->Execute_TurnBase(EmbeddedOwner.GetObject(), LocalTargetYaw);
			bIsTargetRotatorUpdated = false;
		}
	}
	IEmbeddedTurretInterface::Execute_SetTurretAngle(EmbeddedOwner.GetObject(), NewYaw);
	if constexpr (DeveloperSettings::Debugging::GEmbedded_Turret_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Rotating", FColor::Purple);
		RTSFunctionLibrary::PrintString("Target yaw :" + FString::SanitizeFloat(LocalTargetYaw)
		                                + "\n current yaw: " + FString::SanitizeFloat(CurrentYaw), FColor::Red);
	}
}

float AEmbeddedTurretsMaster::ComputeEmbeddedBaseLocalYaw() const
{
    // Fallback: the mesh’s current relative yaw (still local to its parent).
    if (GetIsValidEmbeddedTurretMesh())
    {
        return EmbeddedTurretMesh->GetRelativeRotation().Yaw;
    }

    // Safe default
    return 0.f;
}

float AEmbeddedTurretsMaster::ClampYawToLimits(const float InYaw) const
{
    // If you use wrap-around yaw (-180..180), keep that scheme; otherwise simple clamp.
    return FMath::Clamp(InYaw, M_MinYaw, M_MaxYaw);
}

void AEmbeddedTurretsMaster::SetEmbeddedIdleBaseTarget()
{
    const float BaseYaw = bM_IsAssaultTurret
        ? ClampYawToLimits(M_EmbeddedBaseLocalYaw)
        : M_EmbeddedBaseLocalYaw;

    // Use the inherited target rotator: embedded rotation code already reads its .Yaw as a LOCAL yaw target
    SteeringState.M_TargetRotator = FRotator(0.f, BaseYaw, 0.f);
    bM_UseLocalIdleBaseTarget_Embedded = true;

    // We’ll rotate smoothly in RotateTurret() using your existing paths.
    bM_IsRotatedToEngage = false;
    bM_IsFullyRotatedToTarget = false;
}
