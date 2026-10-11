// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MedicComponent.h"

#include "Async/Async.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "MedicPatientSubsystem.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconTypes.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "TimerManager.h"
#include "WorldCollision.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMedic, Log, All);

namespace MedicComponentConstants
{
	constexpr int32 PlayerOwnedTeam = 1;
	// How often a medic with a patient checks on it: whether it moved, can still be healed, is due a heal tick.
	constexpr float PatientUpdateInterval = 0.25f;
	// A soldier with this part of its health or more needs no medic.
	constexpr float FullHealthFraction = 0.999f;
	// Soldiers whose health fractions are this close count as equally injured; the closer one is picked.
	constexpr float EqualHealthFractionTolerance = 0.05f;
	// The medic counts as standing on its spot within this distance: the walk's acceptance radius, the unit's
	// own radius and some room for the soldiers it bumps into.
	constexpr float StandArrivalTolerance = 100.0f;
	// A patient that moved this far from where the medic was heading is walked after again.
	constexpr float PatientMoveTolerance = 60.0f;
	// A healing medic stays where it is while its patient is within the stand-off distance plus this: a soldier
	// that steps out of standing cover to fire and back again is not walked after.
	constexpr float HealReachSlack = 150.0f;
	constexpr int32 MaximumApproachAttempts = 3;
	// When the spot cannot be reached, healing from this far is accepted on the last attempt.
	constexpr float FallbackHealReach = 200.0f;
	constexpr float SkippedPatientSeconds = 10.0f;
}

UMedicComponent::UMedicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMedicComponent::SetSquadOwner(ASquadUnit* OwnerUnit)
{
	M_OwnerSquadUnit = OwnerUnit;
	// Debugging.
	(void)GetIsValidOwnerSquadUnit();
}

void UMedicComponent::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_PatientSubsystem = World->GetSubsystem<UMedicPatientSubsystem>();
	M_AnimatedIconWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedIconWidgetPoolManager(this);
	// Medics that spawn together do not all search on the same frame.
	StartUpdateTimer(HealSettings.SearchPeriod, FMath::FRandRange(0.0f, HealSettings.SearchPeriod));
}

void UMedicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleasePatient();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool UMedicComponent::GetIsValidOwnerSquadUnit() const
{
	if (M_OwnerSquadUnit.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerSquadUnit",
		"GetIsValidOwnerSquadUnit",
		this
	);

	return false;
}

bool UMedicComponent::GetIsValidPatientSubsystem() const
{
	if (M_PatientSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PatientSubsystem",
		"GetIsValidPatientSubsystem",
		this
	);

	return false;
}

bool UMedicComponent::GetIsValidAnimatedIconWidgetPoolManager() const
{
	if (M_AnimatedIconWidgetPoolManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_AnimatedIconWidgetPoolManager",
		"GetIsValidAnimatedIconWidgetPoolManager",
		this
	);

	return false;
}

void UMedicComponent::StartUpdateTimer(const float UpdateInterval, const float FirstDelay)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || UpdateInterval <= 0.0f)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		M_UpdateTimerHandle,
		this,
		&UMedicComponent::UpdateMedic,
		UpdateInterval,
		true,
		FMath::Max(FirstDelay, KINDA_SMALL_NUMBER));
}

void UMedicComponent::UpdateMedic()
{
	if (not GetIsValidOwnerSquadUnit())
	{
		return;
	}
	if (not M_OwnerSquadUnit->IsUnitAlive())
	{
		OnOwnerDied();
		return;
	}
	if (M_HealState == EMedicHealState::Idle)
	{
		UpdateIdle();
		return;
	}
	UpdatePatient();
}

void UMedicComponent::UpdateIdle()
{
	if (bM_IsSearchPending || not GetMayAutoHeal())
	{
		return;
	}
	StartPatientSearch();
}

bool UMedicComponent::GetMayAutoHeal() const
{
	if (not HealSettings.bAutoHealWhenIdle || bM_IsAutoHealSuppressed)
	{
		return false;
	}
	const ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	// Silent: polled on every update, also while the unit has not been handed to its squad yet.
	ASquadController* SquadController = OwnerUnit->M_SquadController;
	if (not IsValid(SquadController) || not SquadController->GetIsSquadFullyLoadedAndInitialized())
	{
		return false;
	}
	// The unit itself must be free as well: it may still be evading or finishing its part of a command.
	const bool bIsUnitFree = OwnerUnit->M_ActiveCommand == EAbilityID::IdIdle ||
		OwnerUnit->M_ActiveCommand == EAbilityID::IdHeal;
	return bIsUnitFree && SquadController->GetIsUnitIdle() && not OwnerUnit->GetIsSquadInsideCargo() &&
		not OwnerUnit->GetIsStressed();
}

bool UMedicComponent::GetIsPatientBeyondFollowDistance(const ASquadUnit& Patient) const
{
	if (not bM_HasAutoHealAnchor || HealSettings.MaxFollowDistance <= 0.0f)
	{
		return false;
	}
	return FVector::Dist2D(M_AutoHealAnchor, Patient.GetActorLocation()) > HealSettings.MaxFollowDistance;
}

bool UMedicComponent::GetIsValidPatient(ASquadUnit* Candidate) const
{
	const ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	if (not IsValid(Candidate) || Candidate == OwnerUnit || not IsValid(OwnerUnit) || not Candidate->IsUnitAlive())
	{
		return false;
	}
	const UHealthComponent* PatientHealth = Candidate->GetHealthComponent();
	const bool bIsInjuredAlly = IsValid(PatientHealth) &&
		Candidate->GetOwningPlayer() == OwnerUnit->GetOwningPlayer() &&
		PatientHealth->GetHealthPercentage() < MedicComponentConstants::FullHealthFraction;
	return bIsInjuredAlly && not Candidate->GetIsSquadInsideCargo();
}

bool UMedicComponent::GetIsPatientSkipped(const ASquadUnit* Candidate) const
{
	const UWorld* World = GetWorld();
	return Candidate == M_SkippedPatient.Get() && IsValid(World) &&
		World->GetTimeSeconds() < M_SkippedPatientUntilWorldSeconds;
}

void UMedicComponent::StartPatientSearch()
{
	UWorld* World = GetWorld();
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	if (not IsValid(World) || not IsValid(OwnerUnit))
	{
		return;
	}
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(
		OwnerUnit->GetOwningPlayer() == MedicComponentConstants::PlayerOwnedTeam
			? COLLISION_OBJ_PLAYER
			: COLLISION_OBJ_ENEMY);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTS_Medic_PatientSearch), false, OwnerUnit);

	const TWeakObjectPtr<UMedicComponent> WeakThis(this);
	FOverlapDelegate OverlapDelegate;
	OverlapDelegate.BindLambda(
		[WeakThis](const FTraceHandle& /*TraceHandle*/, FOverlapDatum& OverlapDatum)
		{
			// The overlaps are only read on the game thread, wherever the physics scene answers from.
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakThis, Overlaps = MoveTemp(OverlapDatum.OutOverlaps)]()
				{
					if (not WeakThis.IsValid())
					{
						return;
					}
					WeakThis->OnPatientSearchCompleted(Overlaps);
				});
		});

	bM_IsSearchPending = true;
	World->AsyncOverlapByObjectType(
		OwnerUnit->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HealSettings.SearchRadius),
		QueryParams,
		&OverlapDelegate);
}

void UMedicComponent::OnPatientSearchCompleted(const TArray<FOverlapResult>& Overlaps)
{
	bM_IsSearchPending = false;
	// The answer arrives a frame or more later; by then the medic may be busy or its squad may have a command.
	const bool bStillWantsPatient = M_HealState == EMedicHealState::Idle && M_OwnerSquadUnit.IsValid() &&
		M_OwnerSquadUnit->IsUnitAlive() && GetMayAutoHeal();
	if (not bStillWantsPatient)
	{
		return;
	}
	if (not bM_HasAutoHealAnchor)
	{
		M_AutoHealAnchor = M_OwnerSquadUnit->M_SquadController->GetSquadLocation();
		bM_HasAutoHealAnchor = true;
	}
	TArray<ASquadUnit*> Candidates;
	Candidates.Reserve(Overlaps.Num());
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ASquadUnit* SquadUnit = Cast<ASquadUnit>(Overlap.GetActor()))
		{
			Candidates.AddUnique(SquadUnit);
		}
	}
	constexpr bool bLimitToFollowDistance = true;
	ASquadUnit* Patient = SelectBestPatient(Candidates, bLimitToFollowDistance);
	if (not IsValid(Patient))
	{
		return;
	}
	constexpr bool bIsOrderedHeal = false;
	StartHealingPatient(*Patient, bIsOrderedHeal);
}

ASquadUnit* UMedicComponent::SelectBestPatient(
	const TArray<ASquadUnit*>& Candidates,
	const bool bLimitToFollowDistance) const
{
	const ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	if (not IsValid(OwnerUnit) || not GetIsValidPatientSubsystem())
	{
		return nullptr;
	}
	const FVector MedicLocation = OwnerUnit->GetActorLocation();
	ASquadUnit* BestPatient = nullptr;
	bool bBestIsTendedTo = false;
	float BestHealthFraction = 0.0f;
	float BestDistanceSquared = 0.0f;
	for (ASquadUnit* Candidate : Candidates)
	{
		const bool bIsUsable = GetIsValidPatient(Candidate) && not GetIsPatientSkipped(Candidate) &&
			not (bLimitToFollowDistance && GetIsPatientBeyondFollowDistance(*Candidate));
		if (not bIsUsable)
		{
			continue;
		}
		const bool bIsTendedTo = M_PatientSubsystem->GetMedicCountForPatient(Candidate) > 0;
		const float HealthFraction = Candidate->GetHealthComponent()->GetHealthPercentage();
		const float DistanceSquared = FVector::DistSquared2D(MedicLocation, Candidate->GetActorLocation());
		bool bIsBetter = not IsValid(BestPatient);
		if (IsValid(BestPatient) && bIsTendedTo != bBestIsTendedTo)
		{
			bIsBetter = not bIsTendedTo;
		}
		else if (IsValid(BestPatient))
		{
			const bool bIsEquallyInjured = FMath::IsNearlyEqual(
				HealthFraction,
				BestHealthFraction,
				MedicComponentConstants::EqualHealthFractionTolerance);
			bIsBetter = bIsEquallyInjured
				? DistanceSquared < BestDistanceSquared
				: HealthFraction < BestHealthFraction;
		}
		if (not bIsBetter)
		{
			continue;
		}
		BestPatient = Candidate;
		bBestIsTendedTo = bIsTendedTo;
		BestHealthFraction = HealthFraction;
		BestDistanceSquared = DistanceSquared;
	}
	return BestPatient;
}

ASquadUnit* UMedicComponent::SelectNextOrderedPatient() const
{
	ASquadController* PatientSquad = M_OrderedPatientSquad.Get();
	if (not IsValid(PatientSquad))
	{
		return nullptr;
	}
	constexpr bool bLimitToFollowDistance = false;
	return SelectBestPatient(PatientSquad->GetSquadUnitsChecked(), bLimitToFollowDistance);
}

bool UMedicComponent::StartOrderedHeal(ASquadUnit* OrderedPatient)
{
	if (not GetIsValidOwnerSquadUnit() || not IsValid(OrderedPatient) || not GetIsValidPatientSubsystem())
	{
		return false;
	}
	StopHealing();
	M_OrderedPatientSquad = OrderedPatient->M_SquadController;
	// The soldier the player pointed at goes first, unless another medic has it and a squad mate has nobody.
	const bool bTakeOrderedPatient = GetIsValidPatient(OrderedPatient) &&
		M_PatientSubsystem->GetMedicCountForPatient(OrderedPatient) == 0;
	ASquadUnit* Patient = bTakeOrderedPatient ? OrderedPatient : SelectNextOrderedPatient();
	if (not IsValid(Patient))
	{
		M_OrderedPatientSquad.Reset();
		return false;
	}
	constexpr bool bIsOrderedHeal = true;
	StartHealingPatient(*Patient, bIsOrderedHeal);
	return true;
}

void UMedicComponent::StartHealingPatient(ASquadUnit& Patient, const bool bIsOrderedHeal)
{
	if (not GetIsValidOwnerSquadUnit())
	{
		return;
	}
	ReleasePatient();
	M_Patient = &Patient;
	if (GetIsValidPatientSubsystem())
	{
		M_PatientSubsystem->ClaimPatient(&Patient);
	}
	bM_IsOrderedHeal = bIsOrderedHeal;
	M_FailedApproachCount = 0;
	M_MedicStartLocation = M_OwnerSquadUnit->GetActorLocation();
	UE_LOG(
		LogRTSMedic,
		Verbose,
		TEXT("RTS_MEDIC %s heals %s; ordered=%d distance_cm=%.0f"),
		*M_OwnerSquadUnit->GetName(),
		*Patient.GetName(),
		bIsOrderedHeal ? 1 : 0,
		FVector::Dist2D(M_MedicStartLocation, Patient.GetActorLocation()));
	StartUpdateTimer(
		MedicComponentConstants::PatientUpdateInterval,
		MedicComponentConstants::PatientUpdateInterval);
	StartApproach();
}

FVector UMedicComponent::GetStandLocationForPatient(
	const ASquadUnit& Patient,
	const FVector& MedicStartLocation) const
{
	const FVector PatientLocation = Patient.GetActorLocation();
	FVector TowardMedicStart = (MedicStartLocation - PatientLocation).GetSafeNormal2D();
	if (TowardMedicStart.IsNearlyZero())
	{
		TowardMedicStart = -Patient.GetActorForwardVector().GetSafeNormal2D();
	}
	bool bIsNavigable = false;
	const FSquadUnitCoverRuntimeState& PatientCover = Patient.GetCoverRuntimeState();
	if (PatientCover.GetIsOccupyingCover())
	{
		// The cover normal points from the cover to the soldier behind it, so further along it is the side the
		// cover shields.
		const FVector AwayFromCover = PatientCover.AssignedCoverPoint.CoverNormal.GetSafeNormal2D();
		const FVector BehindPatient = RTSFunctionLibrary::GetLocationProjected(
			this,
			PatientLocation + AwayFromCover * HealSettings.PatientStandOffDistance,
			true,
			bIsNavigable);
		if (bIsNavigable)
		{
			return BehindPatient;
		}
	}
	const FVector BesidePatient = RTSFunctionLibrary::GetLocationProjected(
		this,
		PatientLocation + TowardMedicStart * HealSettings.PatientStandOffDistance,
		true,
		bIsNavigable);
	return bIsNavigable ? BesidePatient : PatientLocation;
}

void UMedicComponent::StartApproach()
{
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	const ASquadUnit* Patient = M_Patient.Get();
	if (not IsValid(OwnerUnit) || not IsValid(Patient))
	{
		return;
	}
	UE_LOG(
		LogRTSMedic,
		VeryVerbose,
		TEXT("RTS_MEDIC %s walks to %s; state=%s patient_moved_cm=%.0f patient_command=%s patient_cover=%s medic_command=%s medic_from_spot_cm=%.0f medic_walking=%d"),
		*OwnerUnit->GetName(),
		*Patient->GetName(),
		*UEnum::GetValueAsString(M_HealState),
		FVector::Dist2D(Patient->GetActorLocation(), M_PatientLocationAtApproach),
		*UEnum::GetValueAsString(Patient->GetActiveCommand()),
		*UEnum::GetValueAsString(Patient->GetCoverRuntimeState().State),
		*UEnum::GetValueAsString(OwnerUnit->GetActiveCommand()),
		FVector::Dist2D(OwnerUnit->GetActorLocation(), M_StandLocation),
		OwnerUnit->GetIsPathFollowingActive() ? 1 : 0);
	M_HealState = EMedicHealState::MovingToPatient;
	StopHealAnimation();
	SetOwnerWeaponDisabled(false);
	M_PatientLocationAtApproach = Patient->GetActorLocation();
	M_StandLocation = GetStandLocationForPatient(*Patient, M_MedicStartLocation);
	if (GetIsAtStandLocation())
	{
		BeginHealing();
		return;
	}
	// Sets the unit's command to IdHeal, which keeps it out of automatic cover, and calls back through
	// OnFinishedMovementForHeal. A walk that could not start is picked up by the next update.
	bM_IsStartingWalk = true;
	OwnerUnit->ExecuteMoveToSelfPathFinding(M_StandLocation, EAbilityID::IdHeal, true);
	bM_IsStartingWalk = false;
}

void UMedicComponent::OnFinishedMovementForHeal(const bool bWasAborted)
{
	if (M_HealState != EMedicHealState::MovingToPatient || bM_IsStartingWalk || bWasAborted)
	{
		return;
	}
	OnApproachEnded();
}

void UMedicComponent::OnApproachEnded()
{
	using namespace MedicComponentConstants;
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	ASquadUnit* Patient = M_Patient.Get();
	// Sending the medic again aborts its previous walk, and that abort reports after the new walk started.
	if (not IsValid(OwnerUnit) || OwnerUnit->GetIsPathFollowingActive())
	{
		return;
	}
	if (not GetIsValidPatient(Patient))
	{
		OnPatientDone();
		return;
	}
	if (GetHasPatientMovedSinceApproach(*Patient, PatientMoveTolerance))
	{
		StartApproach();
		return;
	}
	const bool bIsLastAttempt = M_FailedApproachCount + 1 >= MaximumApproachAttempts;
	const bool bIsWithinFallbackReach = FVector::Dist2D(
		OwnerUnit->GetActorLocation(),
		Patient->GetActorLocation()) <= FallbackHealReach;
	if (GetIsAtStandLocation() || (bIsLastAttempt && bIsWithinFallbackReach))
	{
		BeginHealing();
		return;
	}
	++M_FailedApproachCount;
	UE_LOG(
		LogRTSMedic,
		Verbose,
		TEXT("RTS_MEDIC %s did not reach its spot next to %s; attempt=%d spot_distance_cm=%.0f patient_distance_cm=%.0f"),
		*OwnerUnit->GetName(),
		*Patient->GetName(),
		M_FailedApproachCount,
		FVector::Dist2D(OwnerUnit->GetActorLocation(), M_StandLocation),
		FVector::Dist2D(OwnerUnit->GetActorLocation(), Patient->GetActorLocation()));
	if (M_FailedApproachCount >= MaximumApproachAttempts)
	{
		GiveUpOnPatient();
		return;
	}
	StartApproach();
}

void UMedicComponent::BeginHealing()
{
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	const ASquadUnit* Patient = M_Patient.Get();
	const UWorld* World = GetWorld();
	if (not IsValid(OwnerUnit) || not IsValid(Patient) || not IsValid(World))
	{
		return;
	}
	M_HealState = EMedicHealState::Healing;
	M_FailedApproachCount = 0;
	// A medic that already stood next to its patient did not walk, so nothing took it out of its cover yet.
	OwnerUnit->CancelAutomaticCoverForCommandMovement(TEXT("medic heal"));
	OwnerUnit->M_ActiveCommand = EAbilityID::IdHeal;
	SetOwnerWeaponDisabled(true);
	FacePatient(*Patient);
	UpdateHealAnimation(*Patient);
	M_NextHealWorldSeconds = World->GetTimeSeconds() + HealSettings.HealInterval;
}

void UMedicComponent::UpdatePatient()
{
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	ASquadUnit* Patient = M_Patient.Get();
	if (not GetIsValidPatient(Patient))
	{
		OnPatientDone();
		return;
	}
	if (not bM_IsOrderedHeal && not GetMayAutoHeal())
	{
		// The squad's command normally stops the medic before it starts; this catches whatever did not.
		StopHealing();
		return;
	}
	if (not bM_IsOrderedHeal && GetIsPatientBeyondFollowDistance(*Patient))
	{
		GiveUpOnPatient();
		return;
	}
	// Something else, such as stepping out of an ally's way, moved the unit and took its command.
	if (OwnerUnit->M_ActiveCommand != EAbilityID::IdHeal && not OwnerUnit->GetIsPathFollowingActive())
	{
		StartApproach();
		return;
	}
	if (M_HealState == EMedicHealState::MovingToPatient)
	{
		UpdateMovingToPatient(*Patient);
		return;
	}
	UpdateHealing(*Patient);
}

void UMedicComponent::UpdateMovingToPatient(const ASquadUnit& Patient)
{
	if (GetHasPatientMovedSinceApproach(Patient, MedicComponentConstants::PatientMoveTolerance))
	{
		StartApproach();
		return;
	}
	if (not M_OwnerSquadUnit->GetIsPathFollowingActive())
	{
		OnApproachEnded();
	}
}

void UMedicComponent::UpdateHealing(ASquadUnit& Patient)
{
	const float HealReach = HealSettings.PatientStandOffDistance + MedicComponentConstants::HealReachSlack;
	if (FVector::Dist2D(M_OwnerSquadUnit->GetActorLocation(), Patient.GetActorLocation()) > HealReach)
	{
		StartApproach();
		return;
	}
	FacePatient(Patient);
	// Asked for again on every update: a montage that cut the heal animation off is over by now.
	UpdateHealAnimation(Patient);
	const UWorld* World = GetWorld();
	if (IsValid(World) && World->GetTimeSeconds() >= M_NextHealWorldSeconds)
	{
		M_NextHealWorldSeconds = World->GetTimeSeconds() + HealSettings.HealInterval;
		ApplyHealTick(Patient);
	}
}

void UMedicComponent::ApplyHealTick(ASquadUnit& Patient)
{
	UHealthComponent* PatientHealth = Patient.GetHealthComponent();
	if (not IsValid(PatientHealth))
	{
		return;
	}
	PatientHealth->Heal(HealSettings.HealAmountPerTick);
	ShowHealIcon(Patient);
}

void UMedicComponent::ShowHealIcon(ASquadUnit& Patient)
{
	if (not bM_ShowHealIcon)
	{
		return;
	}
	if (not GetIsValidAnimatedIconWidgetPoolManager())
	{
		bM_ShowHealIcon = false;
		return;
	}
	M_AnimatedIconWidgetPoolManager->ShowAnimatedIconAttachedToActor(
		ERTSVerticalAnimatedIcon::Healing,
		&Patient,
		HealSettings.HealIconOffset);
}

void UMedicComponent::FacePatient(const ASquadUnit& Patient) const
{
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	const FVector ToPatient = (Patient.GetActorLocation() - OwnerUnit->GetActorLocation()).GetSafeNormal2D();
	if (ToPatient.IsNearlyZero())
	{
		return;
	}
	OwnerUnit->SetActorRotation(FRotator(0.0f, ToPatient.Rotation().Yaw, 0.0f));
}

void UMedicComponent::UpdateHealAnimation(const ASquadUnit& Patient) const
{
	USquadUnitAnimInstance* MedicAnimation = M_OwnerSquadUnit->GetAnimBP_SquadUnit();
	if (not IsValid(MedicAnimation))
	{
		return;
	}
	const USquadUnitAnimInstance* PatientAnimation = Patient.GetAnimBP_SquadUnit();
	const FSquadUnitCoverRuntimeState& PatientCover = Patient.GetCoverRuntimeState();
	// A soldier at a trench step counts as low as well: he crouches below the edge between his shots.
	const bool bIsInLowCover = PatientCover.GetIsOccupyingCover() &&
		PatientCover.AssignedCoverPoint.CoverType != ERTSCoverType::StandingLeft &&
		PatientCover.AssignedCoverPoint.CoverType != ERTSCoverType::StandingRight;
	const bool bIsPatientLow = bIsInLowCover ||
		(IsValid(PatientAnimation) && PatientAnimation->GetIsCrouchedOrProne());
	MedicAnimation->SetMedicHeal(bIsPatientLow ? ESquadMedicHealStance::Crouch : ESquadMedicHealStance::Standing);
}

void UMedicComponent::StopHealAnimation() const
{
	const ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	USquadUnitAnimInstance* MedicAnimation = IsValid(OwnerUnit) ? OwnerUnit->GetAnimBP_SquadUnit() : nullptr;
	if (not IsValid(MedicAnimation))
	{
		return;
	}
	MedicAnimation->SetMedicHeal(ESquadMedicHealStance::None);
}

void UMedicComponent::SetOwnerWeaponDisabled(const bool bDisabled)
{
	if (bDisabled == bM_HasDisabledWeapon)
	{
		return;
	}
	bM_HasDisabledWeapon = bDisabled;
	const ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	AInfantryWeaponMaster* InfantryWeapon = IsValid(OwnerUnit) ? OwnerUnit->GetInfantryWeapon() : nullptr;
	if (not IsValid(InfantryWeapon))
	{
		return;
	}
	if (bDisabled)
	{
		// The heal animation needs both hands.
		constexpr bool bStopReload = true;
		constexpr bool bMakeWeaponInvisible = true;
		InfantryWeapon->DisableWeaponSearch(bStopReload, bMakeWeaponInvisible);
		return;
	}
	InfantryWeapon->SetAutoEngageTargets(true);
}

void UMedicComponent::OnPatientDone()
{
	UE_LOG(
		LogRTSMedic,
		Verbose,
		TEXT("RTS_MEDIC %s is done with %s; state=%s ordered=%d"),
		*GetNameSafe(M_OwnerSquadUnit.Get()),
		*GetNameSafe(M_Patient.Get()),
		*UEnum::GetValueAsString(M_HealState),
		bM_IsOrderedHeal ? 1 : 0);
	ReleasePatient();
	ASquadUnit* NextPatient = bM_IsOrderedHeal ? SelectNextOrderedPatient() : nullptr;
	if (IsValid(NextPatient))
	{
		constexpr bool bIsOrderedHeal = true;
		StartHealingPatient(*NextPatient, bIsOrderedHeal);
		return;
	}
	FinishHealing();
}

void UMedicComponent::GiveUpOnPatient()
{
	const UWorld* World = GetWorld();
	M_SkippedPatient = M_Patient;
	M_SkippedPatientUntilWorldSeconds = IsValid(World)
		? World->GetTimeSeconds() + MedicComponentConstants::SkippedPatientSeconds
		: 0.0f;
	OnPatientDone();
}

void UMedicComponent::FinishHealing()
{
	const bool bWasOrderedHeal = bM_IsOrderedHeal;
	StopHealing();
	if (not bWasOrderedHeal || not M_OwnerSquadUnit.IsValid())
	{
		return;
	}
	// After the medic is back to idle: the report may end the squad's heal command and start its next one.
	ASquadController* SquadController = M_OwnerSquadUnit->M_SquadController;
	if (IsValid(SquadController))
	{
		SquadController->OnMedicFinishedOrderedHeal();
	}
}

void UMedicComponent::StopHealing()
{
	const bool bWasBusy = M_HealState != EMedicHealState::Idle;
	ReleasePatient();
	M_HealState = EMedicHealState::Idle;
	bM_IsOrderedHeal = false;
	M_OrderedPatientSquad.Reset();
	M_FailedApproachCount = 0;
	if (not bWasBusy)
	{
		return;
	}
	ReturnOwnerToIdle();
	StartUpdateTimer(HealSettings.SearchPeriod, HealSettings.SearchPeriod);
}

void UMedicComponent::ReturnOwnerToIdle()
{
	StopHealAnimation();
	SetOwnerWeaponDisabled(false);
	ASquadUnit* OwnerUnit = M_OwnerSquadUnit.Get();
	if (not IsValid(OwnerUnit) || OwnerUnit->M_ActiveCommand != EAbilityID::IdHeal)
	{
		return;
	}
	// Stops the walk to the patient and sets the unit's own command back to idle.
	OwnerUnit->TerminateMovementCommand();
}

void UMedicComponent::ReleasePatient()
{
	const ASquadUnit* Patient = M_Patient.Get(true);
	M_Patient.Reset();
	if (Patient == nullptr || not M_PatientSubsystem.IsValid())
	{
		return;
	}
	M_PatientSubsystem->ReleasePatient(Patient);
}

void UMedicComponent::OnSquadCommandStarting(const EAbilityID AbilityStarting)
{
	(void)AbilityStarting;
	bM_IsAutoHealSuppressed = false;
	bM_HasAutoHealAnchor = false;
	StopHealing();
}

void UMedicComponent::SuppressAutoHealUntilNextCommand()
{
	bM_IsAutoHealSuppressed = true;
}

void UMedicComponent::OnOwnerDied()
{
	const bool bWasOrderedHeal = bM_IsOrderedHeal;
	ReleasePatient();
	M_HealState = EMedicHealState::Idle;
	bM_IsOrderedHeal = false;
	M_OrderedPatientSquad.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
	}
	ASquadController* SquadController = M_OwnerSquadUnit.IsValid() ? M_OwnerSquadUnit->M_SquadController : nullptr;
	if (bWasOrderedHeal && IsValid(SquadController))
	{
		SquadController->OnMedicFinishedOrderedHeal();
	}
}

bool UMedicComponent::GetIsAtStandLocation() const
{
	return FVector::Dist2D(M_OwnerSquadUnit->GetActorLocation(), M_StandLocation) <=
		MedicComponentConstants::StandArrivalTolerance;
}

bool UMedicComponent::GetHasPatientMovedSinceApproach(const ASquadUnit& Patient, const float Tolerance) const
{
	return FVector::Dist2D(Patient.GetActorLocation(), M_PatientLocationAtApproach) > Tolerance;
}
