// NomadicSkeletalAttachment.cpp
// Copyright (C) Bas Blokzijl

#include "NomadicSkeletalAttachment.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ANomadicAttachmentSkeletal::ANomadicAttachmentSkeletal()
{
	// Make sure to not use the inherited static mesh tick function.
	bM_UseStaticRotation = false;
}

void ANomadicAttachmentSkeletal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSkeletalNomadic();
	Super::EndPlay(EndPlayReason);
}

void ANomadicAttachmentSkeletal::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if constexpr (DeveloperSettings::Debugging::GNomadicSkeletalAttachments_Compile_DebugSymbols)
	{
		Debug(DeltaSeconds);
	}

	if (M_SkeletalState == ENomadicSkeletalState::Inactive)
	{
		return;
	}
	if (not GetIsValidSkeletalMesh() || not GetIsValidNomadicAnimInstance())
	{
		StopSkeletalNomadic();
		return;
	}

	switch (M_SkeletalState)
	{
	case ENomadicSkeletalState::Waiting:
		M_TimeUntilNextAction -= DeltaSeconds;
		if (M_TimeUntilNextAction <= 0.f)
		{
			if (bM_PendingReturnToBaseAfterWait)
			{
				bM_PendingReturnToBaseAfterWait = false;
				StartReturnToTPose();
			}
			else
			{
				DecideNextAction();
			}
		}
		break;

	case ENomadicSkeletalState::PlayingMontage:
		// Driven by OnMontageEnded.
		break;

	case ENomadicSkeletalState::RotatingToTarget:
	case ENomadicSkeletalState::ReturningToTPose:
		StepYawTowardTarget(DeltaSeconds, M_MontageSettings.CustomRotationSpeedDegPerSec);
		break;

	case ENomadicSkeletalState::HoldingFixedAO:
		// AO-only average-hold: intentionally do nothing; we keep orientation until mode changes.
		break;

	default: break;
	}
}

void ANomadicAttachmentSkeletal::StartSkeletalNomadicOn(USkeletalMeshComponent* SkeletalMesh,
                                                        UNomadicAttachmentAnimInstance* AnimInstance,
                                                        const FNomadicMontageSettings& Settings)
{
	M_SkeletalMesh = SkeletalMesh;
	M_AnimInstance = AnimInstance;
	M_MontageSettings = Settings;
	M_AnimationState = Settings.InitialAnimationState;

	if (not GetIsValidSkeletalMesh() || not GetIsValidNomadicAnimInstance())
	{
		return;
	}

	// Ensure delay min/max are sane.
	if (M_MontageSettings.MaxDelayBetweenMontages < M_MontageSettings.MinDelayBetweenMontages)
	{
		Swap(M_MontageSettings.MinDelayBetweenMontages, M_MontageSettings.MaxDelayBetweenMontages);
	}

	if (not TryCacheBaseAimYaw())
	{
		RTSFunctionLibrary::ReportError(TEXT("NomadicAttachmentSkeletal: Could not cache baseline yaw."));
		return;
	}

	// Initialize AO to baseline.
	SetAOYawDeg(M_BaseAimYawDeg);

	// Prepare audio if any sound is configured.
	bM_SoundReady = false;
	SetupOrRefreshAudioComponentIfNeeded();

	M_SkeletalState = ENomadicSkeletalState::Waiting;
	M_TimeUntilNextAction = 0.f; // decide immediately
	bM_PendingReturnToBaseAfterWait = false;
	bM_ReturningForMontage = false;
	M_PendingMontage = nullptr;
	M_PendingMontageSound = nullptr;

	SetActorTickEnabled(true);

	// Enter idle sound straight away (fade from silence).
	if (bM_SoundReady)
	{
		Audio_PlayIdle();
	}
}

void ANomadicAttachmentSkeletal::StopSkeletalNomadic()
{
	ClearMontageBinding();

	M_SkeletalState = ENomadicSkeletalState::Inactive;
	M_TimeUntilNextAction = 0.f;
	bM_PendingReturnToBaseAfterWait = false;
	bM_ReturningForMontage = false;
	M_PendingMontage = nullptr;
	M_PendingMontageSound = nullptr;

	// Reset AO to baseline if we can.
	if (GetIsValidNomadicAnimInstance())
	{
		SetAOYawDeg(M_BaseAimYawDeg);
	}

	// Fade out audio if present.
	if (GetIsValidSoundComponent())
	{
		Audio_FadeOutOnly();
	}
}

void ANomadicAttachmentSkeletal::SetAttachmentAnimationState(const EAttachmentAnimationState NewState)
{
	if (M_AnimationState == NewState)
	{
		return;
	}

	const EAttachmentAnimationState Old = M_AnimationState;
	M_AnimationState = NewState;

	// ---------- Robust transition handling (prevents rapid ping-pong) ----------
	if (NewState == EAttachmentAnimationState::AOOnly)
	{
		// Force-stop any active montage and drop any pending montage intent.
		if (UAnimInstance* const Anim = M_AnimInstance.Get())
		{
			Anim->Montage_Stop(0.f);
		}
		bM_ReturningForMontage = false;
		M_PendingMontage = nullptr;
		M_PendingMontageSound = nullptr;

		// Restart the FSM cleanly in AO-only mode.
		M_SkeletalState = ENomadicSkeletalState::Waiting;
		M_TimeUntilNextAction = 0.f;

		// Optional: AO-only average-hold will be ensured in DecideNextAction.
		if (bM_SoundReady)
		{
			Audio_PlayIdle();
		}
		return;
	}

	// Leaving AO-only: release any fixed hold and start fresh.
	if (Old == EAttachmentAnimationState::AOOnly && M_SkeletalState == ENomadicSkeletalState::HoldingFixedAO)
	{
		M_SkeletalState = ENomadicSkeletalState::Waiting;
		M_TimeUntilNextAction = 0.f;
	}
}

void ANomadicAttachmentSkeletal::SetOnAOOnlyMoveToAverageCustomRotation(const bool bEnable)
{
	bM_OnAOOnlyMoveToAverageCustomRotation = bEnable;

	// If currently in AO-only, enforce/clear the behavior immediately.
	if (M_AnimationState == EAttachmentAnimationState::AOOnly)
	{
		if (bEnable)
		{
			M_SkeletalState = ENomadicSkeletalState::Waiting;
			M_TimeUntilNextAction = 0.f; // trigger DecideNextAction → ensure hold
		}
		else if (M_SkeletalState == ENomadicSkeletalState::HoldingFixedAO)
		{
			M_SkeletalState = ENomadicSkeletalState::Waiting;
			M_TimeUntilNextAction = 0.f; // resume normal AO-only randomness
		}
	}
}

void ANomadicAttachmentSkeletal::ForceAOOnly_StopAnyMontageImmediately()
{
	// Stronger version used by training "stop" helper.
	if (UAnimInstance* const Anim = M_AnimInstance.Get())
	{
		Anim->Montage_Stop(0.f);
	}
	bM_ReturningForMontage = false;
	M_PendingMontage = nullptr;
	M_PendingMontageSound = nullptr;

	M_AnimationState = EAttachmentAnimationState::AOOnly;
	M_SkeletalState = ENomadicSkeletalState::Waiting;
	M_TimeUntilNextAction = 0.f;

	if (bM_SoundReady)
	{
		Audio_PlayIdle();
	}
}

void ANomadicAttachmentSkeletal::EnableMontageAndAO_FlushIfHoldingAOOnly()
{
	// Clear any AO-only "hold" as we are beginning a fresh item.
	if (M_SkeletalState == ENomadicSkeletalState::HoldingFixedAO)
	{
		M_SkeletalState = ENomadicSkeletalState::Waiting;
	}

	// Drop any stale pending montage/sound from previous item to avoid cross-item leakage.
	bM_ReturningForMontage = false;
	M_PendingMontage = nullptr;
	M_PendingMontageSound = nullptr;

	M_AnimationState = EAttachmentAnimationState::MontageAndAO;
	M_TimeUntilNextAction = 0.f; // decide immediately next tick
}

bool ANomadicAttachmentSkeletal::GetIsValidSkeletalMesh() const
{
	if (M_SkeletalMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("No valid skeletal mesh setup for Nomadic Attachment: ") + GetName());
	return false;
}

bool ANomadicAttachmentSkeletal::GetIsValidNomadicAnimInstance() const
{
	const UNomadicAttachmentAnimInstance* const Anim = M_AnimInstance.Get();
	if (IsValid(Anim))
	{
		// Optional sanity: ensure instance is actually on the mesh.
		const USkeletalMeshComponent* const Skel = M_SkeletalMesh.Get();
		if (Skel && Skel->GetAnimInstance() != Anim)
		{
			RTSFunctionLibrary::ReportError(TEXT("AnimInstance mismatch on: ") + GetName());
		}
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("No valid nomadic AnimInstance provided for: ") + GetName());
	return false;
}

bool ANomadicAttachmentSkeletal::GetIsValidSoundComponent() const
{
	if (M_SoundComponent.IsValid())
	{
		return true;
	}
	// No error here: sound is optional per design.
	return false;
}

bool ANomadicAttachmentSkeletal::TryCacheBaseAimYaw()
{
	// Prefer reading current bone yaw if a bone was provided; otherwise use AO's current value.
	if (M_MontageSettings.RotationBoneName.IsNone())
	{
		M_BaseAimYawDeg = GetCurrentAOYawDeg();
		return true;
	}

	const USkeletalMeshComponent* const Skel = M_SkeletalMesh.Get();
	if (not Skel)
	{
		return false;
	}

	const FTransform BoneXform = Skel->GetBoneTransform(M_MontageSettings.RotationBoneName,
	                                                    ERelativeTransformSpace::RTS_Component);
	const FRotator R = BoneXform.Rotator();
	M_BaseAimYawDeg = R.Yaw; // Use component-space Yaw as baseline.
	return true;
}

float ANomadicAttachmentSkeletal::GetCurrentAOYawDeg() const
{
	const UNomadicAttachmentAnimInstance* const Anim = M_AnimInstance.Get();
	return Anim ? Anim->NomadicYawDeg : 0.f;
}

void ANomadicAttachmentSkeletal::SetAOYawDeg(const float NewYawDeg) const
{
	UNomadicAttachmentAnimInstance* const Anim = M_AnimInstance.Get();
	if (not Anim)
	{
		return;
	}
	Anim->NomadicYawDeg = NewYawDeg;
}

void ANomadicAttachmentSkeletal::DecideNextAction()
{
	// Respect external mode selection first.
	switch (M_AnimationState)
	{
	case EAttachmentAnimationState::AOOnly:
		{
			// AO-only average-hold takes priority over random AO glances.
			if (IsAOOnlyAverageHoldEnabled())
			{
				EnsureAOOnlyAverageHoldTarget(true /*one-shot AO start*/);
				return;
			}

			StartRandomAORotation();

			// Non-looping AO cue: restart at each AO start.
			if (bM_SoundReady)
			{
				Audio_PlayAONonLoopingStart();
			}
			return;
		}
	case EAttachmentAnimationState::MontageOnly:
		{
			UAnimMontage* PickedMontage = nullptr;
			USoundBase* PickedSound = nullptr;

			if (not TryPickRandomMontage(PickedMontage, PickedSound))
			{
				// No montage to play — remain/return idle.
				BeginWaitRandomDelay();
				if (bM_SoundReady)
				{
					Audio_PlayIdle();
				}
				M_SkeletalState = ENomadicSkeletalState::Waiting;
				return;
			}

			// If not at baseline, go to baseline first, then play (no reroll).
			const float CurrentAO = GetCurrentAOYawDeg();
			const bool bAtBase = FMath::IsNearlyEqual(CurrentAO, M_BaseAimYawDeg, 0.1f);
			if (not bAtBase)
			{
				M_PendingMontage = PickedMontage;
				M_PendingMontageSound = PickedSound;
				bM_ReturningForMontage = true;
				StartReturnToTPose();
				return;
			}

			if (bM_SoundReady)
			{
				Audio_PlayMontageSound(PickedSound);
			}

			if (TryPlayMontage(PickedMontage))
			{
				M_SkeletalState = ENomadicSkeletalState::PlayingMontage;
				return;
			}

			// Could not play the montage; go idle.
			BeginWaitRandomDelay();
			if (bM_SoundReady)
			{
				Audio_PlayIdle();
			}
			M_SkeletalState = ENomadicSkeletalState::Waiting;
			return;
		}
	case EAttachmentAnimationState::MontageAndAO:
	default:
		break;
	}

	// Default combined mode: roll chance for montage vs AO.
	const float Rand01 = FMath::FRand();
	const bool bDoMontage = (Rand01 < M_MontageSettings.MontagePlayChance01);

	if (bDoMontage)
	{
		UAnimMontage* PickedMontage = nullptr;
		USoundBase* PickedSound = nullptr;

		if (not TryPickRandomMontage(PickedMontage, PickedSound))
		{
			StartRandomAORotation();
			if (bM_SoundReady)
			{
				Audio_PlayAONonLoopingStart();
			}
			return;
		}

		// If we're not at baseline yet, first return to baseline, then play THIS montage (no reroll).
		const float CurrentAO = GetCurrentAOYawDeg();
		const bool bAtBase = FMath::IsNearlyEqual(CurrentAO, M_BaseAimYawDeg, 0.1f);
		if (not bAtBase)
		{
			M_PendingMontage = PickedMontage;
			M_PendingMontageSound = PickedSound;
			bM_ReturningForMontage = true;
			StartReturnToTPose();
			return;
		}

		// Audio: fade to montage sound now (if any).
		if (bM_SoundReady)
		{
			Audio_PlayMontageSound(PickedSound);
		}

		if (TryPlayMontage(PickedMontage))
		{
			M_SkeletalState = ENomadicSkeletalState::PlayingMontage;
			return;
		}

		// Fallback: if play failed, just do AO this tick.
		StartRandomAORotation();
		if (bM_SoundReady)
		{
			Audio_PlayAONonLoopingStart();
		}
		return;
	}

	// AO path selected.
	StartRandomAORotation();
	if (bM_SoundReady)
	{
		Audio_PlayAONonLoopingStart();
	}
}

bool ANomadicAttachmentSkeletal::TryPickRandomMontage(UAnimMontage*& OutPickedMontage,
                                                      USoundBase*& OutPickedSound) const
{
	OutPickedMontage = nullptr;
	OutPickedSound = nullptr;

	const int32 NumEntries = M_MontageSettings.MontageEntries.Num();
	if (NumEntries <= 0)
	{
		return false;
	}

	const int32 Index = FMath::RandRange(0, NumEntries - 1);
	const FNomadicMontageEntry& Entry = M_MontageSettings.MontageEntries[Index];

	if (not IsValid(Entry.Montage))
	{
		return false;
	}

	OutPickedMontage = Entry.Montage;
	OutPickedSound = Entry.MontageSound; // may be null (allowed)
	return true;
}

bool ANomadicAttachmentSkeletal::TryPlayMontage(UAnimMontage* const MontageToPlay)
{
	if (not GetIsValidNomadicAnimInstance() || not IsValid(MontageToPlay))
	{
		return false;
	}

	UAnimInstance* const Anim = M_AnimInstance.Get();
	if (not Anim)
	{
		return false;
	}

	if (not bM_MontageBound)
	{
		Anim->OnMontageEnded.AddDynamic(this, &ANomadicAttachmentSkeletal::OnMontageEnded);
		bM_MontageBound = true;
	}

	Anim->Montage_Play(MontageToPlay);
	return true;
}

void ANomadicAttachmentSkeletal::StartRandomAORotation()
{
	// Degrees (rotation magnitude), never seconds.
	const float MinDeg = FMath::Max(0.f, GetMinAORotationMagnitudeDeg());
	const float MaxDeg = FMath::Max(MinDeg, GetMaxAORotationMagnitudeDeg());

	const float MagnitudeDeg = FMath::FRandRange(MinDeg, MaxDeg);
	float Sign = 1.f;
	if (M_MontageSettings.bAllowNegativeCustomRotations)
	{
		Sign = (FMath::FRand() < 0.5f) ? -1.f : 1.f;
	}

	const float Current = GetCurrentAOYawDeg();
	M_TargetAimYawDeg = Current + Sign * MagnitudeDeg;

	M_SkeletalState = ENomadicSkeletalState::RotatingToTarget;
}

void ANomadicAttachmentSkeletal::StartReturnToTPose()
{
	M_TargetAimYawDeg = M_BaseAimYawDeg;
	M_SkeletalState = ENomadicSkeletalState::ReturningToTPose;
}

void ANomadicAttachmentSkeletal::StepYawTowardTarget(const float DeltaSeconds, const float SpeedDegPerSec)
{
	const float Current = GetCurrentAOYawDeg();
	const float Delta = M_TargetAimYawDeg - Current;

	if (FMath::IsNearlyZero(Delta, 0.1f))
	{
		SetAOYawDeg(M_TargetAimYawDeg);

		if (M_SkeletalState == ENomadicSkeletalState::RotatingToTarget)
		{
			// If AO-only average-hold is enabled and we reached the average target, hold there.
			if (IsAOOnlyAverageHoldEnabled() &&
				FMath::IsNearlyEqual(M_TargetAimYawDeg, ComputeAOOnlyAverageTargetYaw(), 0.1f))
			{
				M_SkeletalState = ENomadicSkeletalState::HoldingFixedAO;
				return;
			}

			BeginWaitRandomDelay();
			bM_PendingReturnToBaseAfterWait = true; // after this wait, return to base
			M_SkeletalState = ENomadicSkeletalState::Waiting;
			return;
		}

		// ReturningToTPose:
		if (bM_ReturningForMontage && M_PendingMontage)
		{
			// CRITICAL GUARD: if animation mode changed to AOOnly while returning, drop the montage.
			if (M_AnimationState == EAttachmentAnimationState::AOOnly)
			{
				bM_ReturningForMontage = false;
				M_PendingMontage = nullptr;
				M_PendingMontageSound = nullptr;

				BeginWaitRandomDelay();
				if (bM_SoundReady)
				{
					Audio_PlayIdle();
				}
				M_SkeletalState = ENomadicSkeletalState::Waiting;
				return;
			}

			// Audio: switch to montage sound now that we're at baseline.
			if (bM_SoundReady)
			{
				Audio_PlayMontageSound(M_PendingMontageSound);
			}

			bM_ReturningForMontage = false;

			if (TryPlayMontage(M_PendingMontage))
			{
				M_PendingMontage = nullptr;
				M_PendingMontageSound = nullptr;
				M_SkeletalState = ENomadicSkeletalState::PlayingMontage;
				return;
			}

			// Failed to play: clear and fall back to normal wait/decide.
			M_PendingMontage = nullptr;
			M_PendingMontageSound = nullptr;
		}

		BeginWaitRandomDelay();

		// Audio: we are idle now.
		if (bM_SoundReady)
		{
			Audio_PlayIdle();
		}

		M_SkeletalState = ENomadicSkeletalState::Waiting; // after this wait, decide next action
		return;
	}

	const float Step = FMath::Clamp(SpeedDegPerSec * DeltaSeconds, 0.f, FMath::Abs(Delta)) * FMath::Sign(Delta);
	SetAOYawDeg(Current + Step);
}

void ANomadicAttachmentSkeletal::BeginWaitRandomDelay()
{
	// Wait times are seconds; use delay settings (+ extra in AOOnly), never degree magnitudes.
	const float MinD = FMath::Max(0.f, GetMinDelay_StateAdjusted());
	const float MaxD = FMath::Max(MinD, GetMaxDelay_StateAdjusted());
	M_TimeUntilNextAction = FMath::FRandRange(MinD, MaxD);
}

void ANomadicAttachmentSkeletal::ClearMontageBinding()
{
	if (not bM_MontageBound)
	{
		return;
	}
	if (UAnimInstance* const Anim = M_AnimInstance.Get())
	{
		Anim->OnMontageEnded.RemoveDynamic(this, &ANomadicAttachmentSkeletal::OnMontageEnded);
	}
	bM_MontageBound = false;
}

void ANomadicAttachmentSkeletal::OnMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	BeginWaitRandomDelay();
	M_SkeletalState = ENomadicSkeletalState::Waiting;
	bM_PendingReturnToBaseAfterWait = false;
	bM_ReturningForMontage = false;
	M_PendingMontage = nullptr;
	M_PendingMontageSound = nullptr;

	// Audio: montage finished → go idle (or fade out if no idle).
	if (bM_SoundReady)
	{
		Audio_PlayIdle();
	}
}

// --------------------- Delay (seconds) helpers ---------------------

float ANomadicAttachmentSkeletal::GetMinDelay_StateAdjusted() const
{
	if (M_AnimationState == EAttachmentAnimationState::AOOnly)
	{
		const float Extra = FMath::Max(0.f, M_MontageSettings.AOOnly_ExtraDelayBetweenAnimations);
		return M_MontageSettings.MinDelayBetweenMontages + Extra;
	}
	return M_MontageSettings.MinDelayBetweenMontages;
}

float ANomadicAttachmentSkeletal::GetMaxDelay_StateAdjusted() const
{
	if (M_AnimationState == EAttachmentAnimationState::AOOnly)
	{
		const float Extra = FMath::Max(0.f, M_MontageSettings.AOOnly_ExtraDelayBetweenAnimations);
		return M_MontageSettings.MaxDelayBetweenMontages + Extra;
	}
	return M_MontageSettings.MaxDelayBetweenMontages;
}

// --------------------- Rotation magnitude (degrees) helpers ---------------------

float ANomadicAttachmentSkeletal::GetMinAORotationMagnitudeDeg() const
{
	return M_MontageSettings.MinCustomRotationDeg;
}

float ANomadicAttachmentSkeletal::GetMaxAORotationMagnitudeDeg() const
{
	return M_MontageSettings.MaxCustomRotationDeg;
}

// --------------------- AO-only average-hold helpers ---------------------

bool ANomadicAttachmentSkeletal::IsAOOnlyAverageHoldEnabled() const
{
	return (M_AnimationState == EAttachmentAnimationState::AOOnly) && bM_OnAOOnlyMoveToAverageCustomRotation;
}

float ANomadicAttachmentSkeletal::ComputeAOOnlyAverageTargetYaw() const
{
	const float AvgDeg = 0.5f * (M_MontageSettings.MinCustomRotationDeg + M_MontageSettings.MaxCustomRotationDeg);
	return M_BaseAimYawDeg + AvgDeg;
}

bool ANomadicAttachmentSkeletal::EnsureAOOnlyAverageHoldTarget(const bool bPlaySoundIfStarted)
{
	if (not IsAOOnlyAverageHoldEnabled())
	{
		return false;
	}

	const float TargetYaw = ComputeAOOnlyAverageTargetYaw();
	const float CurrentYaw = GetCurrentAOYawDeg();

	if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.1f))
	{
		M_TargetAimYawDeg = TargetYaw;
		M_SkeletalState = ENomadicSkeletalState::HoldingFixedAO;
		return false;
	}

	M_TargetAimYawDeg = TargetYaw;
	M_SkeletalState = ENomadicSkeletalState::RotatingToTarget;

	if (bPlaySoundIfStarted && bM_SoundReady)
	{
		Audio_PlayAONonLoopingStart();
	}
	return true;
}

void ANomadicAttachmentSkeletal::Debug(const float DeltaTime)
{
	const FVector DebugLoc = GetActorLocation() + FVector(0.f, 0.f, 500.f);
	const FString AnimStatestr = UEnum::GetValueAsString(M_AnimationState);
	const FString SkeletalStatestr = UEnum::GetValueAsString(M_SkeletalState);
	const FString TimeLeftWaiting = FString::SanitizeFloat(M_TimeUntilNextAction, 2);
	const FString TargetYaw = FString::SanitizeFloat(M_TargetAimYawDeg, 1);
	const FString DebugStr =
		FString::Printf(TEXT("AnimState: %s\nSkeletalState: %s\nTimeUntilNextAction: %s\nTargetYaw: %s"),
		                *AnimStatestr, *SkeletalStatestr, *TimeLeftWaiting, *TargetYaw);
	RTSFunctionLibrary::DrawDebugAtLocation(this,
	                                        DebugLoc,
	                                        DebugStr,
	                                        FColor::Green,
	                                        DeltaTime * 2);
}

// ============================ Audio helpers ============================

bool ANomadicAttachmentSkeletal::HasAnyConfiguredSound() const
{
	if (IsValid(M_MontageSettings.AONonLoopingSound) || IsValid(M_MontageSettings.IdleSound))
	{
		return true;
	}

	for (const FNomadicMontageEntry& Entry : M_MontageSettings.MontageEntries)
	{
		if (IsValid(Entry.MontageSound))
		{
			return true;
		}
	}
	return false;
}

void ANomadicAttachmentSkeletal::SetupOrRefreshAudioComponentIfNeeded()
{
	bM_SoundReady = false;

	if (not GetIsValidNomadicAnimInstance() || not HasAnyConfiguredSound())
	{
		return; // No audio will be used.
	}

	UAudioComponent* AudioComp = M_SoundComponent.Get();
	if (not AudioComp)
	{
		AudioComp = NewObject<UAudioComponent>(this);
		if (not AudioComp)
		{
			return;
		}
		AudioComp->bAutoActivate = false;
		AudioComp->bAutoDestroy = false; // keep component alive across fades/one-shots
		AudioComp->SetupAttachment(GetRootComponent());
		AudioComp->RegisterComponent();
		M_SoundComponent = AudioComp;
	}

	// Apply attenuation & concurrency from settings (global for this attachment).
	AudioComp->AttenuationSettings = M_MontageSettings.SoundAttenuation;
	AudioComp->ConcurrencySet.Empty();
	if (IsValid(M_MontageSettings.SoundConcurrency))
	{
		AudioComp->ConcurrencySet.Add(M_MontageSettings.SoundConcurrency);
	}

	bM_SoundReady = true;
}

void ANomadicAttachmentSkeletal::Audio_FadeToSound(USoundBase* const NewSound) const
{
	if (not GetIsValidSoundComponent())
	{
		return;
	}

	UAudioComponent* const AudioComp = M_SoundComponent.Get();
	const float FadeTime = FMath::Max(0.f, M_MontageSettings.SoundFadeSeconds);

	if (not IsValid(NewSound))
	{
		Audio_FadeOutOnly();
		return;
	}

	if (AudioComp->IsPlaying() && AudioComp->Sound == NewSound)
	{
		return;
	}

	if (AudioComp->IsPlaying())
	{
		AudioComp->FadeOut(FadeTime, 0.f);
	}

	AudioComp->SetSound(NewSound);
	AudioComp->FadeIn(FadeTime, 1.f);
}

void ANomadicAttachmentSkeletal::Audio_FadeOutOnly() const
{
	if (not GetIsValidSoundComponent())
	{
		return;
	}
	UAudioComponent* const AudioComp = M_SoundComponent.Get();
	if (AudioComp->IsPlaying())
	{
		const float FadeTime = FMath::Max(0.f, M_MontageSettings.SoundFadeSeconds);
		AudioComp->FadeOut(FadeTime, 0.f);
	}
}

void ANomadicAttachmentSkeletal::Audio_PlayAONonLoopingStart() const
{
	// Restart a one-shot at AO begin.
	if (not bM_SoundReady)
	{
		return;
	}

	USoundBase* const StartCue = M_MontageSettings.AONonLoopingSound;
	if (not IsValid(StartCue))
	{
		return;
	}

	UAudioComponent* const AudioComp = M_SoundComponent.Get();
	if (not AudioComp)
	{
		return;
	}

	AudioComp->Stop();
	AudioComp->SetSound(StartCue);
	AudioComp->Play(0.f);
}

void ANomadicAttachmentSkeletal::Audio_PlayIdle() const
{
	if (not bM_SoundReady)
	{
		return;
	}
	if (IsValid(M_MontageSettings.IdleSound))
	{
		Audio_FadeToSound(M_MontageSettings.IdleSound);
	}
	else
	{
		Audio_FadeOutOnly();
	}
}

void ANomadicAttachmentSkeletal::Audio_PlayMontageSound(USoundBase* const MontageSound) const
{
	if (not bM_SoundReady)
	{
		return;
	}
	if (IsValid(MontageSound))
	{
		Audio_FadeToSound(MontageSound);
	}
	else
	{
		Audio_FadeOutOnly();
	}
}
