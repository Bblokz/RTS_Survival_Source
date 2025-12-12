// Copyright (C) Bas Blokzijl

#include "WeaponTargetingData.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"

// Default fallback aim offsets (local space)
const FVector FWeaponTargetingData::M_FallbackOffsets[NumAimPoints] =
{
	FVector(0.f, 0.f, 100.f), // head/upper
	FVector(30.f, 0.f, 20.f), // right shoulder
	FVector(-30.f, 0.f, 20.f), // left shoulder
	FVector(0.f, 0.f, 20.f) // center/torso
};

FWeaponTargetingData::FWeaponTargetingData()
{
	M_LocalAimOffsets.Reserve(NumAimPoints);
	UseFallbackAimOffsets();
}

bool FWeaponTargetingData::WasKilledActorCurrentTarget(const AActor* KilledActor) const
{
	if (M_TargetMode != ETargetMode::Actor || not KilledActor)
	{
		return false;
	}
	return M_TargetActor && M_TargetActor == KilledActor;
}


void FWeaponTargetingData::InitTargetStruct(const int32 OwningPlayer)
{
	M_OwningPlayer = OwningPlayer;
	RerollSwitchThreshold();
	// Keep current target mode as-is; just reseed selection cadence.
}

void FWeaponTargetingData::SetTargetActor(AActor* NewTarget)
{
	if (not NewTarget)
	{
		ResetTarget();
		return;
	}

	M_TargetMode = ETargetMode::Actor;
	M_TargetActor = NewTarget;
	LoadAimOffsetsFromActor(NewTarget);

	M_SelectedIndex = 0;
	M_SwitchCounter = 0;
	RecomputeActiveWorldLocation();
	
	// DebugDraw(NewTarget->GetWorld());
	
}

void FWeaponTargetingData::SetTargetGround(const FVector& GroundWorldLocation)
{
	M_TargetMode = ETargetMode::Ground;
	M_TargetActor = nullptr;
	M_GroundWorldLocation = GroundWorldLocation;

	M_SelectedIndex = 0;
	M_SwitchCounter = 0;
	RecomputeActiveWorldLocation();
}

void FWeaponTargetingData::ResetTarget()
{
	M_TargetMode = ETargetMode::None;
	M_TargetActor = nullptr;
	M_GroundWorldLocation = FVector::ZeroVector;
	M_SelectedIndex = 0;
	M_SwitchCounter = 0;
	M_ActiveTargetLocation = FVector::ZeroVector;
}

bool FWeaponTargetingData::GetIsTargetValid() const
{
	// Ground targeting is always valid as far as this struct is concerned.
	if (M_TargetMode == ETargetMode::Ground)
	{
		return true;
	}
	return RTSFunctionLibrary::RTSIsVisibleTarget(M_TargetActor, M_OwningPlayer);
}

void FWeaponTargetingData::TickAimSelection()
{
	if (M_TargetMode != ETargetMode::Actor)
	{
		return; // only rotate aim points for actor targets
	}

	// Guard counter overrun in extremely long sessions.
	if (M_SwitchCounter >= M_MaxCounterBeforeReset)
	{
		M_SwitchCounter = 0;
		RerollSwitchThreshold();
	}

	++M_SwitchCounter;
	if (M_SwitchCounter > M_TargetCallCountSwitchAim)
	{
		M_SwitchCounter = 0;
		M_SelectedIndex = (M_SelectedIndex + 1) % NumAimPoints;
	}
	// Potentially uses the new index set above.
	RecomputeActorTargetLocationWithCurrentAimOffset();
}

void FWeaponTargetingData::LoadAimOffsetsFromActor(AActor* Target)
{
	M_LocalAimOffsets.Reset();

	if (IAimOffsetProvider* Provider = Cast<IAimOffsetProvider>(Target))
	{
		// Let the actor provide up to 4 local offsets; we fill missing with fallbacks.
		TArray<FVector> Provided;
		Provided.Reserve(NumAimPoints);
		Provider->GetAimOffsetPoints(Provided);

		const int32 Count = FMath::Clamp(Provided.Num(), 0, NumAimPoints);
		for (int32 i = 0; i < Count; ++i)
		{
			M_LocalAimOffsets.Add(Provided[i]);
		}
		for (int32 i = Count; i < NumAimPoints; ++i)
		{
			M_LocalAimOffsets.Add(M_FallbackOffsets[i]);
		}
		return;
	}

	UseFallbackAimOffsets();
}

void FWeaponTargetingData::UseFallbackAimOffsets()
{
	M_LocalAimOffsets.Reset();
	for (int32 i = 0; i < NumAimPoints; ++i)
	{
		M_LocalAimOffsets.Add(M_FallbackOffsets[i]);
	}
}

void FWeaponTargetingData::RecomputeActiveWorldLocation()
{
	switch (M_TargetMode)
	{
	case ETargetMode::Ground:
		M_ActiveTargetLocation = M_GroundWorldLocation;
		return;

	case ETargetMode::Actor:
		RecomputeActorTargetLocationWithCurrentAimOffset();
		return;

	case ETargetMode::None:
	default:
		M_ActiveTargetLocation = FVector::ZeroVector;
		return;
	}
}

void FWeaponTargetingData::RecomputeActorTargetLocationWithCurrentAimOffset()
{
	if (GetIsValidTargetActor())
	{
		const FTransform T = M_TargetActor->GetActorTransform();
		const FVector Local = M_LocalAimOffsets.IsValidIndex(M_SelectedIndex)
			                      ? M_LocalAimOffsets[M_SelectedIndex]
			                      : FallBackOffsetInvalidIndex;
		M_ActiveTargetLocation = T.TransformPosition(Local); // handles scale/mirror safely
	}
}

void FWeaponTargetingData::RerollSwitchThreshold()
{
	// Random in [50..200] inclusive.
	M_TargetCallCountSwitchAim = FMath::RandRange(15, 25);
}

bool FWeaponTargetingData::GetIsValidTargetActor() const
{
	return IsValid(M_TargetActor);
}

void FWeaponTargetingData::DebugDraw(UWorld* World) const
{
	if (not World)
	{
		return;
	}
	const float ScaleSpheres = 5;
	if (M_TargetMode != ETargetMode::Actor || not GetIsValidTargetActor())
	{
		return;
	}

	const FTransform T = M_TargetActor->GetActorTransform();
	for (int32 i = 0; i < NumAimPoints; ++i)
	{
		const FVector WS = T.TransformPosition(M_LocalAimOffsets[i]);
		const bool bIsSel = (i == M_SelectedIndex);
		DrawDebugSphere(World, WS, (bIsSel ? 18.f : 10.f) * ScaleSpheres, 10, bIsSel ? FColor::Yellow : FColor::Blue,
		                false, 3.f, 0, 1.5f);
	}
}
