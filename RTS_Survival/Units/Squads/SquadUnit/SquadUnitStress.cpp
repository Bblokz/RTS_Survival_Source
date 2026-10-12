// Copyright (C) Bas Blokzijl - All rights reserved.

// The stress part of ASquadUnit: a soldier in cover that loses health quickly sprays blind fire, runs for cover
// further from his enemy and sprays again. See DesignTexts/InfantryCoverSystemDesign.md, "Infantry stress".

#include "SquadUnit.h"

#include "AnimSquadUnit/SquadUnitAnimInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SquadUnitSpacialVoiceLinePlayer/SquadUnitSpatialVoiceLinePlayer.h"
#include "RTS_Survival/Behaviours/Derived/InfantryStress/InfantryStressBehaviour.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSquadUnitStress, Log, All);

namespace SquadUnitStressStatics
{
	constexpr float SpeedComparisonTolerance = 0.5f;
}

float ASquadUnit::TakeDamage(
	const float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float HealthBeforeDamage = IsValid(HealthComponent) ? HealthComponent->GetCurrentHealth() : 0.0f;
	const float DamageResult = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (IsUnitAlive() && IsValid(HealthComponent))
	{
		RecordHealthLossForStress(HealthBeforeDamage - HealthComponent->GetCurrentHealth());
	}
	return DamageResult;
}

bool ASquadUnit::GetIsHoldingCoverPoint() const
{
	return M_CoverRuntimeState.State == ESquadUnitCoverState::Protected ||
		M_CoverRuntimeState.State == ESquadUnitCoverState::Exposed;
}

void ASquadUnit::RecordHealthLossForStress(const float HealthLost)
{
	// This runs for every hit on every soldier, so whatever rules most hits out is asked first.
	if (HealthLost <= 0.0f || GetIsStressed())
	{
		return;
	}
	// Silent: a soldier whose squad is not set up yet has no stress tolerance to measure against.
	const float StressTolerance = IsValid(M_SquadController) ? M_SquadController->GetStressTolerance() : 0.0f;
	const UWorld* World = GetWorld();
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	if (StressTolerance <= 0.0f || not IsValid(World) || not IsValid(CoverSettings) ||
		not CoverSettings->bM_EnableInfantryStress)
	{
		return;
	}
	const float WorldSeconds = World->GetTimeSeconds();
	const float MaxHealth = HealthComponent->GetMaxHealth();
	if (WorldSeconds < M_Stress.ImmuneUntilWorldSeconds || MaxHealth <= 0.0f)
	{
		return;
	}
	// The meter drains a whole tolerance per window, so health lost more slowly than that never fills it.
	const float DrainPerSecond = StressTolerance / FMath::Max(
		CoverSettings->M_InfantryStressHealthLossWindowSeconds,
		KINDA_SMALL_NUMBER);
	const float SecondsSinceLastLoss = WorldSeconds - M_Stress.LastHealthLossWorldSeconds;
	M_Stress.RecentHealthLossFraction = FMath::Max(
		0.0f,
		M_Stress.RecentHealthLossFraction - SecondsSinceLastLoss * DrainPerSecond);
	M_Stress.RecentHealthLossFraction += HealthLost / MaxHealth;
	M_Stress.LastHealthLossWorldSeconds = WorldSeconds;
	// Outside cover the meter keeps counting, but only a soldier holding a point gets stressed.
	if (M_Stress.RecentHealthLossFraction < StressTolerance || not GetIsHoldingCoverPoint())
	{
		return;
	}
	StartStress();
}

void ASquadUnit::StartStress()
{
	UWorld* World = GetWorld();
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		: nullptr;
	if (not IsValid(CoverSettings) || not IsValid(CoverSubsystem))
	{
		return;
	}
	constexpr float SprayFromCoverDurationRatio = 0.5f;
	const float WorldSeconds = World->GetTimeSeconds();
	const float StressDuration = CoverSettings->M_InfantryStressDuration;
	M_Stress.Phase = ESquadUnitStressPhase::SprayingFromCover;
	M_Stress.RelocateWorldSeconds = WorldSeconds + StressDuration * SprayFromCoverDurationRatio;
	M_Stress.EndWorldSeconds = WorldSeconds + StressDuration;
	M_Stress.RecentHealthLossFraction = 0.0f;

	// The weapon changes on this very frame, so the first spray animation already fires the stressed weapon.
	M_Stress.Behaviour = CoverSubsystem->AcquireInfantryStressBehaviour();
	if (IsValid(M_Stress.Behaviour))
	{
		M_Stress.Behaviour->StartOnSoldier(*this);
	}
	CoverSubsystem->RegisterStressedUnit(*this);
	UpdateStressSpray();
	PlayStressedVoiceLine();
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSSquadUnitStress,
			Verbose,
			TEXT("RTS_STRESS_START unit=%s cover_type=%s duration_s=%.1f behaviour=%s"),
			*GetName(),
			*UEnum::GetValueAsString(M_CoverRuntimeState.AssignedCoverPoint.CoverType),
			StressDuration,
			*GetNameSafe(IsValid(M_Stress.Behaviour) ? M_Stress.Behaviour->GetClass() : nullptr));
	}
}

void ASquadUnit::PlayStressedVoiceLine() const
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	// Always spatial, also when selected: the shout comes from the soldier himself, not from his radio.
	// Forced, as a soldier in a fire fight is nearly always on the spatial cooldown of his fire voice lines.
	SpatialVoiceLinePlayer->ForcePlaySpatialVoiceLine(ERTSVoiceLine::Stressed, GetActorLocation());
}

void ASquadUnit::TickStress()
{
	const UWorld* World = GetWorld();
	if (not IsUnitAlive() || not IsValid(World) || World->GetTimeSeconds() >= M_Stress.EndWorldSeconds)
	{
		EndStress();
		return;
	}
	if (M_Stress.Phase == ESquadUnitStressPhase::SprayingFromCover &&
		World->GetTimeSeconds() >= M_Stress.RelocateWorldSeconds)
	{
		StartStressRelocation();
	}
	// Silent: without a behaviour class the stress has no text or icon to repeat.
	if (IsValid(M_Stress.Behaviour))
	{
		M_Stress.Behaviour->UpdateSoldierFeedback(World->GetTimeSeconds());
	}
	// Arrived, or the run was called off: either way he sprays from wherever he holds cover now.
	const bool bRunIsOver = GetIsHoldingCoverPoint() || not M_CoverRuntimeState.GetHasAssignment();
	if (M_Stress.Phase == ESquadUnitStressPhase::RelocatingToFartherCover && bRunIsOver)
	{
		M_Stress.Phase = ESquadUnitStressPhase::SprayingFromNewCover;
	}
	UpdateStressSpray();
}

void ASquadUnit::EndStress()
{
	if (not GetIsStressed())
	{
		return;
	}
	M_Stress.Phase = ESquadUnitStressPhase::None;
	const UWorld* World = GetWorld();
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		: nullptr;
	if (IsValid(World))
	{
		M_Stress.ImmuneUntilWorldSeconds = World->GetTimeSeconds() +
			(IsValid(CoverSettings) ? CoverSettings->M_InfantryStressRecoverySeconds : 0.0f);
	}
	if (IsValid(M_Stress.Behaviour))
	{
		M_Stress.Behaviour->EndOnSoldier();
	}
	// Silent: when the world is being torn down there is nobody left to hand the behaviour back to.
	if (IsValid(CoverSubsystem))
	{
		CoverSubsystem->UnregisterStressedUnit(*this);
		CoverSubsystem->ReleaseInfantryStressBehaviour(M_Stress.Behaviour);
	}
	M_Stress.Behaviour = nullptr;
	M_Stress.RecentHealthLossFraction = 0.0f;
	RemoveStressSpeed();
	// Silent: a unit without an animation instance never sprayed.
	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->SetCoverSpray(ESquadCoverSprayStance::None);
	}
	// Back to what his cover allows: a soldier behind standing cover may only fire once he has stepped out.
	ApplyCurrentCoverWeaponState();
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(LogRTSSquadUnitStress, Verbose, TEXT("RTS_STRESS_END unit=%s"), *GetName());
	}
}

ESquadCoverSprayStance ASquadUnit::GetStressSprayStance() const
{
	const ERTSCoverType CoverType = M_CoverRuntimeState.AssignedCoverPoint.CoverType;
	const bool bIsStandingCover = CoverType == ERTSCoverType::StandingLeft ||
		CoverType == ERTSCoverType::StandingRight;
	return bIsStandingCover ? ESquadCoverSprayStance::Standing : ESquadCoverSprayStance::Crouch;
}

void ASquadUnit::UpdateStressSpray()
{
	const bool bShouldSpray = M_Stress.Phase != ESquadUnitStressPhase::RelocatingToFartherCover &&
		GetIsHoldingCoverPoint() && not GetIsReloadingInCover();
	// Silent: a unit without an animation instance still fires its stressed weapon.
	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->SetCoverSpray(bShouldSpray ? GetStressSprayStance() : ESquadCoverSprayStance::None);
	}
	// Blind fire: he shoots from behind his cover, whether or not he could see his target from there.
	if (bShouldSpray)
	{
		SetCoverWeaponFireBlocked(false);
	}
}

bool ASquadUnit::GetStressThreatLocation(FVector& OutThreatLocation) const
{
	if (IsValid(GetCurrentCoverTarget(OutThreatLocation)))
	{
		return true;
	}
	// Without a target the enemy is taken to be where the soldier's cover faces: that is what it was picked
	// against. Most damage does not say where it came from, and a shell that does is at his feet by then.
	constexpr float AssumedThreatDistance = 2000.0f;
	const FRTSCoverPoint& CoverPoint = M_CoverRuntimeState.AssignedCoverPoint;
	OutThreatLocation = CoverPoint.Location - CoverPoint.CoverNormal.GetSafeNormal2D() * AssumedThreatDistance;
	return false;
}

bool ASquadUnit::TryReserveStressRelocationPoint(
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	FRTSCoverPoint& OutFartherPoint)
{
	// Only cover he can reach with time left to spray from it: half of what is left of his stress is for running.
	constexpr float RunShareOfRemainingStress = 0.5f;
	const UWorld* World = GetWorld();
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const UCharacterMovementComponent* UnitMovement = GetCharacterMovement();
	// A soldier under orders, or one that left his cover in the meantime, is not sent anywhere by his stress.
	if (not IsValid(World) || not IsValid(CoverSettings) || not IsValid(UnitMovement) ||
		not GetIsHoldingCoverPoint() || not GetMayKeepAutomaticCover())
	{
		return false;
	}
	const float RemainingStressSeconds = M_Stress.EndWorldSeconds - World->GetTimeSeconds();
	const float ReachableDistance = UnitMovement->MaxWalkSpeed * CoverSettings->M_InfantryStressSpeedMultiplier *
		RemainingStressSeconds * RunShareOfRemainingStress;
	FVector ThreatLocation = FVector::ZeroVector;
	// Only a real target has a weapon range to stay inside of.
	const float MaximumDistanceToThreat = GetStressThreatLocation(ThreatLocation)
		? GetMaximumCoverDistanceToTarget()
		: 0.0f;
	const bool bFoundFartherCover = CoverSubsystem.TryReserveStressRetreatCoverPoint(
		*this,
		ThreatLocation,
		FMath::Min(CoverSettings->M_InfantryStressedCoverMaxDisplacement, ReachableDistance),
		MaximumDistanceToThreat,
		OutFartherPoint);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSSquadUnitStress,
			Verbose,
			TEXT("RTS_STRESS_RELOCATE unit=%s found=%d has_target=%d reachable_cm=%.0f to_type=%s run_cm=%.0f threat_distance_cm=%.0f new_threat_distance_cm=%.0f"),
			*GetName(),
			bFoundFartherCover ? 1 : 0,
			MaximumDistanceToThreat > 0.0f ? 1 : 0,
			ReachableDistance,
			bFoundFartherCover ? *UEnum::GetValueAsString(OutFartherPoint.CoverType) : TEXT("-"),
			bFoundFartherCover ? FVector::Dist2D(GetActorLocation(), OutFartherPoint.Location) : 0.0f,
			FVector::Dist2D(GetActorLocation(), ThreatLocation),
			bFoundFartherCover ? FVector::Dist2D(OutFartherPoint.Location, ThreatLocation) : 0.0f);
	}
	return bFoundFartherCover;
}

void ASquadUnit::StartStressRelocation()
{
	M_Stress.Phase = ESquadUnitStressPhase::RelocatingToFartherCover;
	UWorld* World = GetWorld();
	URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		: nullptr;
	FRTSCoverPoint FartherPoint;
	if (not IsValid(CoverSubsystem) || not TryReserveStressRelocationPoint(*CoverSubsystem, FartherPoint))
	{
		// Nowhere better to go: TickStress puts him back to spraying from where he is.
		return;
	}
	// Silent: a unit without an animation instance never sprayed.
	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->SetCoverSpray(ESquadCoverSprayStance::None);
	}
	if (IsValid(M_Stress.Behaviour))
	{
		M_Stress.Behaviour->NotifySoldierRelocates();
	}
	ApplyStressSpeed();
	// Taking the new assignment gives up the point he holds and its reservation.
	if (not SetCoverAssignment(FartherPoint, ESquadUnitCoverUseReason::Attack))
	{
		CoverSubsystem->ReleaseCoverReservation(*this, FartherPoint.PointId);
		return;
	}
	if (not StartCoverMovement())
	{
		ClearCoverStateInternal(false);
	}
}

void ASquadUnit::ApplyStressSpeed()
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	UCharacterMovementComponent* UnitMovement = GetCharacterMovement();
	const bool bAlreadyApplied = M_Stress.AppliedSpeedMultiplier != 1.0f;
	if (not IsValid(CoverSettings) || not IsValid(UnitMovement) || bAlreadyApplied)
	{
		return;
	}
	constexpr float MinimumSpeedMultiplier = 1.0f;
	M_Stress.AppliedSpeedMultiplier = FMath::Max(
		MinimumSpeedMultiplier,
		CoverSettings->M_InfantryStressSpeedMultiplier);
	UnitMovement->MaxWalkSpeed *= M_Stress.AppliedSpeedMultiplier;
	UnitMovement->MaxAcceleration *= M_Stress.AppliedSpeedMultiplier;
	M_Stress.StressedMaxWalkSpeed = UnitMovement->MaxWalkSpeed;
	M_Stress.StressedMaxAcceleration = UnitMovement->MaxAcceleration;
}

void ASquadUnit::RemoveStressSpeed()
{
	using SquadUnitStressStatics::SpeedComparisonTolerance;
	UCharacterMovementComponent* UnitMovement = GetCharacterMovement();
	const float AppliedSpeedMultiplier = M_Stress.AppliedSpeedMultiplier;
	M_Stress.AppliedSpeedMultiplier = 1.0f;
	if (not IsValid(UnitMovement) || AppliedSpeedMultiplier == 1.0f)
	{
		return;
	}
	// Something else set his speed in the meantime; that value already left the stress out.
	if (FMath::IsNearlyEqual(UnitMovement->MaxWalkSpeed, M_Stress.StressedMaxWalkSpeed, SpeedComparisonTolerance))
	{
		UnitMovement->MaxWalkSpeed /= AppliedSpeedMultiplier;
	}
	if (FMath::IsNearlyEqual(
		UnitMovement->MaxAcceleration,
		M_Stress.StressedMaxAcceleration,
		SpeedComparisonTolerance))
	{
		UnitMovement->MaxAcceleration /= AppliedSpeedMultiplier;
	}
}
