// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTS_VerticalCollapse.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	/** We tick at a modest rate; designers control total time, not tick frequency. */
	constexpr float GVerticalCollapseTickInterval = 0.016f; // ~60 fps

	/** Small epsilon to treat "instant" collapse. */
	constexpr float GTimeEpsilon = 1e-3f;

	FORCEINLINE void NormalizeMinMax(const float InA, const float InB, float& OutMin, float& OutMax)
	{
		const float A = FMath::Max(0.0f, InA);
		const float B = FMath::Max(0.0f, InB);
		if (A <= B)
		{
			OutMin = A;
			OutMax = B;
		}
		else
		{
			OutMin = B;
			OutMax = A;
		}
	}
}

static FORCEINLINE bool IsFiniteFloat(const float V) { return FMath::IsFinite(V); }

static FORCEINLINE bool IsFiniteVec(const FVector& V)
{
	return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
}

void FRTS_VerticalCollapse::StartVerticalCollapse(
	AActor* const CollapseOwner,
	const FRTSVerticalCollapseSettings& Settings,
	const FCollapseFX& CollapseFX,
	TFunction<void()>&& OnFinished /*= {}*/)
{
	if (!IsValid(CollapseOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse: Owner invalid."));
		return;
	}

	// Validate settings up front
	if (!IsFiniteFloat(Settings.CollapseTime) || Settings.CollapseTime < 0.0f ||
		!IsFiniteFloat(Settings.DeltaZ) || Settings.DeltaZ < 0.0f ||
		!IsFiniteFloat(Settings.MinRandomYaw) || !IsFiniteFloat(Settings.MaxRandomYaw) ||
		!IsFiniteFloat(Settings.MinPitchRoll) || !IsFiniteFloat(Settings.MaxPitchRoll))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse: Non-finite or negative settings."));
		return;
	}

	// Build shared context exactly once; we keep only weak pointers inside it.
	TSharedPtr<FVerticalCollapseTaskContext> Context = MakeShared<FVerticalCollapseTaskContext>(
		CollapseOwner, Settings, CollapseFX, MoveTemp(OnFinished));

	// Handle instant completion (time ~ 0) to avoid scheduling needless timers.
	if (Settings.CollapseTime <= GTimeEpsilon)
	{
		HandleStart(*Context); // init target + FX
		Context->ElapsedSeconds = Settings.CollapseTime;
		FinishVerticalCollapse(Context);
		return;
	}

	// Normal path: initialize state and start ticking.
	HandleStart(*Context);

	AActor* const Owner = Context->WeakOwner.Get();
	if (!IsValid(Owner))
	{
		return;
	}

	UWorld* const World = Owner->GetWorld();
	if (!IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("FRTS_VerticalCollapse::StartVerticalCollapse: World is invalid."));
		return;
	}

	FTimerDelegate TickDelegate;
	TickDelegate.BindStatic(&FRTS_VerticalCollapse::TickVerticalCollapse, Context);

	World->GetTimerManager().SetTimer(
		Context->TimerHandle,
		TickDelegate,
		GVerticalCollapseTickInterval,
		true /* looping */);
}

void FRTS_VerticalCollapse::HandleStart(FVerticalCollapseTaskContext& MutableContext)
{
	// Validate once more (defensive).
	if (!GetIsValidOwner(MutableContext.WeakOwner, TEXT("HandleStart")))
	{
		return;
	}

	AActor* const Owner = MutableContext.WeakOwner.Get();
	check(Owner);

	// Cache initial transform.
	MutableContext.StartLocation = Owner->GetActorLocation();
	MutableContext.StartRotation = Owner->GetActorRotation();

	// Compute target location (downwards along Z by DeltaZ).
	const float TargetZ = MutableContext.StartLocation.Z - MutableContext.Settings.DeltaZ;
	MutableContext.TargetLocation = FVector(MutableContext.StartLocation.X, MutableContext.StartLocation.Y, TargetZ);

	// ----- Rotation target computation -----
	// Yaw:
	float MinYaw = 0.0f, MaxYaw = 0.0f;
	NormalizeMinMax(MutableContext.Settings.MinRandomYaw, MutableContext.Settings.MaxRandomYaw, MinYaw, MaxYaw);
	const bool bUseYaw = !FMath::IsNearlyZero(MinYaw) || !FMath::IsNearlyZero(MaxYaw);

	float SignedYawDelta = 0.0f;
	if (bUseYaw)
	{
		const float YawMagnitude = FMath::RandRange(MinYaw, MaxYaw);
		SignedYawDelta = (FMath::RandBool() ? YawMagnitude : -YawMagnitude);
	}

	// Pitch/Roll: single magnitude controls both axes, applied along a random 2D direction.
	float MinPR = 0.0f, MaxPR = 0.0f;
	NormalizeMinMax(MutableContext.Settings.MinPitchRoll, MutableContext.Settings.MaxPitchRoll, MinPR, MaxPR);
	const bool bUsePitchRoll = !FMath::IsNearlyZero(MinPR) || !FMath::IsNearlyZero(MaxPR);

	float PitchDelta = 0.0f;
	float RollDelta = 0.0f;
	if (bUsePitchRoll)
	{
		const float MagnitudeDeg = FMath::RandRange(MinPR, MaxPR);
		// Choose a random orientation in pitch/roll plane so one scalar governs both axes.
		const float Theta = FMath::FRandRange(0.0f, 2.0f * PI);
		PitchDelta = MagnitudeDeg * FMath::Cos(Theta);
		RollDelta = MagnitudeDeg * FMath::Sin(Theta);
	}

	// Combine: if neither enabled, TargetRotation == StartRotation.
	if (!bUseYaw && !bUsePitchRoll)
	{
		MutableContext.TargetRotation = MutableContext.StartRotation;
	}
	else
	{
		MutableContext.TargetRotation = FRotator(
			MutableContext.StartRotation.Pitch + PitchDelta,
			MutableContext.StartRotation.Yaw + SignedYawDelta,
			MutableContext.StartRotation.Roll + RollDelta
		);
	}

	// Play FX at start.
	if (UWorld* const World = Owner->GetWorld())
	{
		const FVector FXBase = Owner->GetActorLocation();
		SpawnCollapseVFX(MutableContext.CollapseFX, World, FXBase);
		SpawnCollapseSFX(MutableContext.CollapseFX, World, FXBase);
	}
}

void FRTS_VerticalCollapse::TickVerticalCollapse(TSharedPtr<FVerticalCollapseTaskContext> Context)
{
	if (!EnsureValidContext(Context, TEXT("TickVerticalCollapse")))
	{
		return;
	}
	if (!IsFiniteVec(Context->StartLocation) || !IsFiniteVec(Context->TargetLocation))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse: Cached locations are not finite."));
		return;
	}
	AActor* const Owner = Context->WeakOwner.Get();
	UWorld* const World = Owner->GetWorld();
	if (!IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("TickVerticalCollapse: World is invalid."));
		return;
	}

	// Advance time and clamp alpha.
	Context->ElapsedSeconds += GVerticalCollapseTickInterval;
	const float Duration = FMath::Max(Context->Settings.CollapseTime, GTimeEpsilon);
	const float Alpha = FMath::Clamp(Context->ElapsedSeconds / Duration, 0.0f, 1.0f);

	// Interpolate Z (X/Y fixed at start for stable sink).
	const float NewZ = FMath::Lerp(Context->StartLocation.Z, Context->TargetLocation.Z, Alpha);
	const FVector NewLocation(Context->StartLocation.X, Context->StartLocation.Y, NewZ);

	// Interpolate rotation per-axis along shortest path.
	const float StartPitch = Context->StartRotation.Pitch;
	const float StartYaw = Context->StartRotation.Yaw;
	const float StartRoll = Context->StartRotation.Roll;

	const float TargetPitch = Context->TargetRotation.Pitch;
	const float TargetYaw = Context->TargetRotation.Yaw;
	const float TargetRoll = Context->TargetRotation.Roll;

	const float DeltaPitch = FMath::FindDeltaAngleDegrees(StartPitch, TargetPitch);
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(StartYaw, TargetYaw);
	const float DeltaRoll = FMath::FindDeltaAngleDegrees(StartRoll, TargetRoll);

	const float NewPitch = StartPitch + (DeltaPitch * Alpha);
	const float NewYaw = StartYaw + (DeltaYaw * Alpha);
	const float NewRoll = StartRoll + (DeltaRoll * Alpha);

	const FRotator NewRotation(NewPitch, NewYaw, NewRoll);

	// Apply.
	Owner->SetActorLocation(NewLocation, false);
	Owner->SetActorRotation(NewRotation);

	// Complete?
	if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
	{
		FinishVerticalCollapse(Context);
	}
}

void FRTS_VerticalCollapse::FinishVerticalCollapse(TSharedPtr<FVerticalCollapseTaskContext> Context)
{
	if (!EnsureValidContext(Context, TEXT("FinishVerticalCollapse")))
	{
		return;
	}

	AActor* const Owner = Context->WeakOwner.Get();
	if (UWorld* const World = Owner->GetWorld())
	{
		World->GetTimerManager().ClearTimer(Context->TimerHandle);
	}

	// Snap to exact target to avoid float drift.
	Owner->SetActorLocation(Context->TargetLocation, false);
	Owner->SetActorRotation(Context->TargetRotation);

	// Destruction vs callback:
	if (Context->Settings.bDestroyPostVerticalCollapse)
	{
		if (IsValid(Owner))
		{
			Owner->Destroy();
		}
		// Do NOT call the callback when destroy is requested.
		return;
	}

	// Otherwise, call the callback if set and still "safe" (owner valid).
	if (IsValid(Owner) && Context->OnFinished)
	{
		Context->OnFinished();
	}
}

bool FRTS_VerticalCollapse::GetIsValidOwner(const TWeakObjectPtr<AActor>& WeakOwner, const TCHAR* const Where)
{
	if (WeakOwner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(TEXT("FRTS_VerticalCollapse::%s: Owner is invalid."), Where));
	return false;
}

bool FRTS_VerticalCollapse::EnsureValidContext(
	const TSharedPtr<FVerticalCollapseTaskContext>& Context,
	const TCHAR* const Where)
{
	if (!Context.IsValid())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("FRTS_VerticalCollapse::%s: Context is invalid."), Where));
		return false;
	}
	if (!Context->WeakOwner.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("FRTS_VerticalCollapse::%s: Owner weak ptr invalid."), Where));
		return false;
	}
	return true;
}

void FRTS_VerticalCollapse::SpawnCollapseVFX(
	const FCollapseFX& FX, UWorld* const World, const FVector& BaseLocationFX)
{
	if (!IsValid(World) || !IsValid(FX.CollapseVfx))
	{
		return;
	}

	const FVector VfxLocation = BaseLocationFX + FX.FxLocationOffset;
	const FTransform NiagTransform(FX.VfxRotation, VfxLocation, FX.VfxScale);

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		FX.CollapseVfx,
		NiagTransform.GetLocation(),
		NiagTransform.GetRotation().Rotator(),
		NiagTransform.GetScale3D(),
		true, true, ENCPoolMethod::None, true);
}

void FRTS_VerticalCollapse::SpawnCollapseSFX(
	const FCollapseFX& FX, UWorld* const World, const FVector& BaseLocationFX)
{
	if (!IsValid(FX.CollapseSfx) || !IsValid(World))
	{
		return;
	}

	const FVector SfxLocation = BaseLocationFX + FX.FxLocationOffset;

	if (!IsValid(FX.SfxAttenuation) || !IsValid(FX.SfxConcurrency))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse: Invalid SFX attenuation or concurrency."));
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		FX.CollapseSfx,
		SfxLocation,
		FX.VfxRotation,
		1.f, 1.f, 0.f,
		FX.SfxAttenuation,
		FX.SfxConcurrency);
}
