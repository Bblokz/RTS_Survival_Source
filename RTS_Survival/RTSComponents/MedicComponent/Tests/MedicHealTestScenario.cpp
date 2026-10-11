#include "MedicHealTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/MedicComponent/HealAbilityTypes.h"
#include "RTS_Survival/RTSComponents/MedicComponent/MedicComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMedicTest, Log, All);

namespace MedicHealTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float FindCastTimeoutSeconds = 60.0f;
	// Soldiers nobody sees move slower than their walk speed in an unattended run, so walks get a lot of time.
	constexpr float PhaseTimeoutSeconds = 150.0f;
	constexpr float TestSearchRadius = 8000.0f;
	constexpr float TestSearchPeriod = 0.5f;
	constexpr float TestHealInterval = 0.5f;
	constexpr float TestHealAmountPerTick = 20.0f;
	// Keeps a patient injured for the length of a walk.
	constexpr float SlowHealAmountPerTick = 1.0f;
	constexpr float FullHealthFraction = 1.0f;
	constexpr float HealedHealthFraction = 0.99f;
	constexpr float LowHealthFraction = 0.3f;
	constexpr float MediumHealthFraction = 0.6f;
	constexpr float EqualHealthFraction = 0.5f;
	constexpr float MinimumDirectionAlignment = 0.5f;
	constexpr float MinimumFacingAlignment = 0.9f;
	constexpr float StandOffTolerance = 60.0f;
	constexpr float MaximumHealDistance = 300.0f;
	constexpr float PatientWalkDistance = 700.0f;
	constexpr float MinimumPatientWalkDistance = 300.0f;
	constexpr float LeashMargin = 400.0f;
	constexpr float LeashWalkDistance = 1800.0f;
	constexpr float SuppressedWaitSeconds = 4.0f;
	constexpr float MedicSquadStepDistance = 300.0f;
	// A little longer than a medic leaves out the patient it gave up on, which the follow distance part caused.
	constexpr float SkippedPatientWaitSeconds = 12.0f;
	constexpr int32 FramesBetweenBystanderChecks = 15;
	constexpr int32 NoHealAbilityOnCard = INDEX_NONE;

	UMedicComponent* GetMedicComponent(const ASquadUnit* Soldier)
	{
		return IsValid(Soldier) ? Soldier->GetMedicComponent() : nullptr;
	}

	float GetHealthFraction(const ASquadUnit& Soldier)
	{
		const UHealthComponent* SoldierHealth = Soldier.GetHealthComponent();
		return IsValid(SoldierHealth) ? SoldierHealth->GetHealthPercentage() : 0.0f;
	}

	// Sets the health directly: damage would stress a soldier in cover and send him running.
	void SetHealthFraction(const ASquadUnit& Soldier, const float HealthFraction)
	{
		UHealthComponent* SoldierHealth = Soldier.GetHealthComponent();
		if (IsValid(SoldierHealth))
		{
			SoldierHealth->SetCurrentHealth(SoldierHealth->GetMaxHealth() * HealthFraction);
		}
	}

	bool GetIsLivingPlayerSoldier(ASquadUnit* Soldier)
	{
		return IsValid(Soldier) && Soldier->IsUnitAlive() && Soldier->GetOwningPlayer() == PlayerOwnedTeam;
	}

	bool GetIsUsableSquad(ASquadController* Squad)
	{
		if (not IsValid(Squad) || not Squad->GetIsSquadFullyLoadedAndInitialized() || Squad->GetSquadUnitsCount() <= 0)
		{
			return false;
		}
		const URTSComponent* SquadRTSComponent = Squad->GetRTSComponent();
		const UCargoSquad* CargoSquad = Squad->FindComponentByClass<UCargoSquad>();
		const bool bIsInsideCargo = IsValid(CargoSquad) && CargoSquad->GetIsInsideCargo();
		return IsValid(SquadRTSComponent) && SquadRTSComponent->GetOwningPlayer() == PlayerOwnedTeam &&
			not Squad->GetSquadAlreadyHasTeamWeapon() && not bIsInsideCargo;
	}

	ASquadUnit* GetLivingSoldierOfSquad(ASquadController& Squad, const ASquadUnit* SoldierToSkip)
	{
		for (ASquadUnit* Soldier : Squad.GetSquadUnitsChecked())
		{
			if (GetIsLivingPlayerSoldier(Soldier) && Soldier != SoldierToSkip)
			{
				return Soldier;
			}
		}
		return nullptr;
	}

	/** @return The subtype of the squad's IdHeal card entry, or NoHealAbilityOnCard. */
	int32 GetCardHealSubtype(ASquadController& Squad)
	{
		for (const FUnitAbilityEntry& AbilityEntry : Squad.GetUnitAbilityEntries())
		{
			if (AbilityEntry.AbilityId == EAbilityID::IdHeal)
			{
				return AbilityEntry.CustomType;
			}
		}
		return NoHealAbilityOnCard;
	}

	bool GetIsHealAnimationPlaying(const ASquadUnit& Medic)
	{
		const USquadUnitAnimInstance* MedicAnimation = Medic.GetAnimBP_SquadUnit();
		return IsValid(MedicAnimation) && MedicAnimation->GetIsMedicHealPlaying();
	}

	bool GetIsPatientLow(const ASquadUnit& Patient)
	{
		const FSquadUnitCoverRuntimeState& PatientCover = Patient.GetCoverRuntimeState();
		const bool bIsInLowCover = PatientCover.GetIsOccupyingCover() &&
			PatientCover.AssignedCoverPoint.CoverType != ERTSCoverType::StandingLeft &&
			PatientCover.AssignedCoverPoint.CoverType != ERTSCoverType::StandingRight;
		const USquadUnitAnimInstance* PatientAnimation = Patient.GetAnimBP_SquadUnit();
		return bIsInLowCover || (IsValid(PatientAnimation) && PatientAnimation->GetIsCrouchedOrProne());
	}
}

void FMedicHealTestScenario::Start()
{
	M_Phase = EMedicHealTestPhase::WaitingForCoverScan;
	UE_LOG(LogRTSMedicTest, Display, TEXT("RTS_MEDIC_TEST scheduled"));
}

void FMedicHealTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(
		LogRTSMedicTest,
		Display,
		TEXT("RTS_MEDIC_TEST check=%s %s %s"),
		bCondition ? TEXT("PASS") : TEXT("FAIL"),
		Description,
		*Details);
}

void FMedicHealTestScenario::EnterPhase(const UWorld& World, const EMedicHealTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartWorldSeconds = World.GetTimeSeconds();
	M_PhaseDeadlineWorldSeconds = M_PhaseStartWorldSeconds + (NewPhase == EMedicHealTestPhase::FindingCast
		? MedicHealTestScenarioPrivate::FindCastTimeoutSeconds
		: MedicHealTestScenarioPrivate::PhaseTimeoutSeconds);
}

bool FMedicHealTestScenario::GetHasPhaseTimedOut(const UWorld& World) const
{
	return World.GetTimeSeconds() >= M_PhaseDeadlineWorldSeconds;
}

void FMedicHealTestScenario::Finish(const UWorld& World)
{
	M_Phase = EMedicHealTestPhase::Inactive;
	UE_LOG(
		LogRTSMedicTest,
		Display,
		TEXT("RTS_MEDIC_TEST RESULT %s failed_checks=%d map=%s"),
		M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
		M_FailedCheckCount,
		*World.GetName());
}

bool FMedicHealTestScenario::GetIsCastAlive(const UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const bool bIsCastAlive = GetIsLivingPlayerSoldier(M_Medic.Get()) && GetIsLivingPlayerSoldier(M_SecondMedic.Get()) &&
		GetIsLivingPlayerSoldier(M_Patient.Get()) && GetIsLivingPlayerSoldier(M_SecondPatient.Get()) &&
		M_MedicSquad.IsValid() && M_SecondMedicSquad.IsValid() && M_PatientSquad.IsValid() &&
		IsValid(GetMedicComponent(M_Medic.Get())) && IsValid(GetMedicComponent(M_SecondMedic.Get()));
	if (bIsCastAlive)
	{
		return true;
	}
	Check(false, TEXT("the medics and their patients lived through the test"));
	Finish(World);
	return false;
}

bool FMedicHealTestScenario::FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent)
{
	if (not GetHasPhaseTimedOut(World))
	{
		return false;
	}
	using namespace MedicHealTestScenarioPrivate;
	const ASquadUnit* Medic = M_Medic.Get();
	const ASquadUnit* Patient = M_Patient.Get();
	const UMedicComponent* MedicComponent = GetMedicComponent(Medic);
	Check(
		false,
		ExpectedEvent,
		IsValid(MedicComponent) && IsValid(Patient)
			? FString::Printf(
				TEXT("timed out: patient_squad_command=%s patient_walked_cm=%.0f medic_squad_command=%s medic_state=%s medic_patient=%s medic_command=%s distance_cm=%.0f patient_health=%.2f second_patient_health=%.2f"),
				M_PatientSquad.IsValid() ? *UEnum::GetValueAsString(M_PatientSquad->GetActiveCommandID()) : TEXT("none"),
				FVector::Dist2D(Patient->GetActorLocation(), M_PatientLocationBeforeWalk),
				M_MedicSquad.IsValid() ? *UEnum::GetValueAsString(M_MedicSquad->GetActiveCommandID()) : TEXT("none"),
				*UEnum::GetValueAsString(MedicComponent->GetHealState()),
				*GetNameSafe(MedicComponent->GetPatient()),
				*UEnum::GetValueAsString(Medic->GetActiveCommand()),
				FVector::Dist2D(Medic->GetActorLocation(), Patient->GetActorLocation()),
				GetHealthFraction(*Patient),
				M_SecondPatient.IsValid() ? GetHealthFraction(*M_SecondPatient) : 0.0f)
			: FString(TEXT("timed out")));
	Finish(World);
	return true;
}

void FMedicHealTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == EMedicHealTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == EMedicHealTestPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			EnterPhase(*World, EMedicHealTestPhase::FindingCast);
		}
		return;
	}
	if (M_Phase == EMedicHealTestPhase::FindingCast)
	{
		TickFindingCast(*World);
		return;
	}
	if (not GetIsCastAlive(*World))
	{
		return;
	}
	KeepCastMovingSmoothly();
	if (++M_FrameCounter % MedicHealTestScenarioPrivate::FramesBetweenBystanderChecks == 0)
	{
		KeepBystandersHealthy(*World);
	}
	switch (M_Phase)
	{
	case EMedicHealTestPhase::AutoWaitingForPatient:
		TickAutoWaitingForPatient(*World);
		break;
	case EMedicHealTestPhase::AutoWaitingForHealing:
		TickAutoWaitingForHealing(*World);
		break;
	case EMedicHealTestPhase::AutoWaitingForHealthGain:
		TickAutoWaitingForHealthGain(*World);
		break;
	case EMedicHealTestPhase::FollowWaitingForReapproach:
		TickFollowWaitingForReapproach(*World);
		break;
	case EMedicHealTestPhase::LeashWaitingForRelease:
		TickLeashWaitingForRelease(*World);
		break;
	case EMedicHealTestPhase::SpreadWaitingForTwoPatients:
		TickSpreadWaitingForTwoPatients(*World);
		break;
	case EMedicHealTestPhase::SpreadWaitingForSharedPatient:
		TickSpreadWaitingForSharedPatient(*World);
		break;
	case EMedicHealTestPhase::SpreadWaitingForIdle:
		TickSpreadWaitingForIdle(*World);
		break;
	case EMedicHealTestPhase::OrderWaitingForSecondPatient:
		TickOrderWaitingForSecondPatient(*World);
		break;
	case EMedicHealTestPhase::OrderWaitingForOrderDone:
		TickOrderWaitingForOrderDone(*World);
		break;
	case EMedicHealTestPhase::CancelWaitingWhileSuppressed:
		TickCancelWaitingWhileSuppressed(*World);
		break;
	case EMedicHealTestPhase::ResumeWaitingForAutoHeal:
		TickResumeWaitingForAutoHeal(*World);
		break;
	default:
		break;
	}
}

ASquadUnit* FMedicHealTestScenario::FindPlayerSoldierInCoverWithSquadMate(
	UWorld& World,
	ASquadUnit*& OutSquadMate) const
{
	using namespace MedicHealTestScenarioPrivate;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* Soldier = *UnitIterator;
		if (not GetIsLivingPlayerSoldier(Soldier) ||
			Soldier->GetCoverRuntimeState().State != ESquadUnitCoverState::Protected)
		{
			continue;
		}
		ASquadController* Squad = Soldier->GetSquadControllerChecked();
		if (not GetIsUsableSquad(Squad))
		{
			continue;
		}
		OutSquadMate = GetLivingSoldierOfSquad(*Squad, Soldier);
		if (IsValid(OutSquadMate))
		{
			return Soldier;
		}
	}
	return nullptr;
}

ASquadController* FMedicHealTestScenario::FindNearestMedicSquad(
	UWorld& World,
	const FVector& Location,
	const TArray<const ASquadController*>& SquadsToSkip) const
{
	ASquadController* NearestSquad = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
	{
		ASquadController* Squad = *SquadIterator;
		if (SquadsToSkip.Contains(Squad) || not MedicHealTestScenarioPrivate::GetIsUsableSquad(Squad))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(Location, Squad->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestSquad = Squad;
			NearestDistanceSquared = DistanceSquared;
		}
	}
	return NearestSquad;
}

UMedicComponent* FMedicHealTestScenario::MakeMedic(ASquadUnit& Soldier) const
{
	using namespace MedicHealTestScenarioPrivate;
	UMedicComponent* MedicComponent = Soldier.Debug_AddMedicComponent();
	if (not IsValid(MedicComponent))
	{
		return nullptr;
	}
	FMedicHealSettings& HealSettings = MedicComponent->HealSettings;
	// Switched on by the test once its patients are hurt.
	HealSettings.bAutoHealWhenIdle = false;
	HealSettings.SearchRadius = TestSearchRadius;
	HealSettings.SearchPeriod = TestSearchPeriod;
	HealSettings.HealInterval = TestHealInterval;
	HealSettings.HealAmountPerTick = TestHealAmountPerTick;
	// Unlimited, except in the part of the test that is about the follow distance.
	HealSettings.MaxFollowDistance = 0.0f;
	return MedicComponent;
}

bool FMedicHealTestScenario::FindCast(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	ASquadUnit* SecondPatient = nullptr;
	ASquadUnit* Patient = FindPlayerSoldierInCoverWithSquadMate(World, SecondPatient);
	if (not IsValid(Patient))
	{
		return false;
	}
	ASquadController* PatientSquad = Patient->GetSquadControllerChecked();
	ASquadController* MedicSquad = FindNearestMedicSquad(World, Patient->GetActorLocation(), {PatientSquad});
	if (not IsValid(MedicSquad))
	{
		return false;
	}
	ASquadController* SecondMedicSquad = FindNearestMedicSquad(
		World,
		Patient->GetActorLocation(),
		{PatientSquad, MedicSquad});
	ASquadUnit* Medic = GetLivingSoldierOfSquad(*MedicSquad, nullptr);
	// On a map with only two player squads the second medic comes from the first medic's squad.
	ASquadUnit* SecondMedic = IsValid(SecondMedicSquad)
		? GetLivingSoldierOfSquad(*SecondMedicSquad, nullptr)
		: GetLivingSoldierOfSquad(*MedicSquad, Medic);
	if (not IsValid(Medic) || not IsValid(SecondMedic))
	{
		return false;
	}
	M_Patient = Patient;
	M_SecondPatient = SecondPatient;
	M_PatientSquad = PatientSquad;
	M_Medic = Medic;
	M_SecondMedic = SecondMedic;
	M_MedicSquad = MedicSquad;
	M_SecondMedicSquad = IsValid(SecondMedicSquad) ? SecondMedicSquad : MedicSquad;
	return IsValid(MakeMedic(*Medic)) && IsValid(MakeMedic(*SecondMedic));
}

void FMedicHealTestScenario::KeepCastMovingSmoothly() const
{
	for (const TWeakObjectPtr<ASquadController>& Squad : {M_PatientSquad, M_MedicSquad, M_SecondMedicSquad})
	{
		if (not Squad.IsValid())
		{
			continue;
		}
		for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			UCharacterMovementComponent* SoldierMovement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
			if (IsValid(SoldierMovement))
			{
				// The optimizer sets its interval again on its own updates, so this is repeated every frame.
				SoldierMovement->SetComponentTickInterval(0.0f);
			}
		}
	}
}

void FMedicHealTestScenario::KeepBystandersHealthy(UWorld& World) const
{
	using namespace MedicHealTestScenarioPrivate;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* Soldier = *UnitIterator;
		const bool bIsBystander = GetIsLivingPlayerSoldier(Soldier) && Soldier != M_Patient.Get() &&
			Soldier != M_SecondPatient.Get();
		if (bIsBystander && GetHealthFraction(*Soldier) < FullHealthFraction)
		{
			SetHealthFraction(*Soldier, FullHealthFraction);
		}
	}
}

void FMedicHealTestScenario::CheckStandLocation(const UMedicComponent& MedicComponent, const ASquadUnit& Patient)
{
	using namespace MedicHealTestScenarioPrivate;
	const FVector PatientLocation = Patient.GetActorLocation();
	const FVector StandLocation = MedicComponent.GetStandLocation();
	const FVector TowardStand = (StandLocation - PatientLocation).GetSafeNormal2D();
	const float StandDistance = FVector::Dist2D(StandLocation, PatientLocation);
	const float StandOffDistance = MedicComponent.HealSettings.PatientStandOffDistance;
	Check(
		FMath::Abs(StandDistance - StandOffDistance) <= StandOffTolerance,
		TEXT("the medic stops at the stand-off distance from its patient, on the navmesh"),
		FString::Printf(TEXT("distance_cm=%.0f wanted_cm=%.0f"), StandDistance, StandOffDistance));
	const FSquadUnitCoverRuntimeState& PatientCover = Patient.GetCoverRuntimeState();
	if (not PatientCover.GetIsOccupyingCover())
	{
		UE_LOG(LogRTSMedicTest, Display, TEXT("RTS_MEDIC_TEST the patient left its cover before the medic set off"));
		return;
	}
	const FVector AwayFromCover = PatientCover.AssignedCoverPoint.CoverNormal.GetSafeNormal2D();
	Check(
		FVector::DotProduct(TowardStand, AwayFromCover) >= MinimumDirectionAlignment,
		TEXT("for a patient in cover the medic stands on the side away from what the cover faces"),
		FString::Printf(
			TEXT("cover_type=%s alignment=%.2f"),
			*UEnum::GetValueAsString(PatientCover.AssignedCoverPoint.CoverType),
			FVector::DotProduct(TowardStand, AwayFromCover)));
}

void FMedicHealTestScenario::CheckMedicIsHealing(const ASquadUnit& Medic, const ASquadUnit& Patient)
{
	using namespace MedicHealTestScenarioPrivate;
	const FVector TowardPatient = (Patient.GetActorLocation() - Medic.GetActorLocation()).GetSafeNormal2D();
	const float FacingAlignment = FVector::DotProduct(Medic.GetActorForwardVector().GetSafeNormal2D(), TowardPatient);
	Check(
		FacingAlignment >= MinimumFacingAlignment,
		TEXT("the medic is turned towards its patient before it heals"),
		FString::Printf(TEXT("alignment=%.2f"), FacingAlignment));
	Check(
		FVector::Dist2D(Medic.GetActorLocation(), Patient.GetActorLocation()) <= MaximumHealDistance,
		TEXT("the medic heals from next to its patient"),
		FString::Printf(TEXT("distance_cm=%.0f"), FVector::Dist2D(Medic.GetActorLocation(), Patient.GetActorLocation())));
	Check(
		Medic.GetActiveCommand() == EAbilityID::IdHeal && not Medic.GetHasCoverAssignment(),
		TEXT("a healing medic is busy with IdHeal and takes no automatic cover"),
		*UEnum::GetValueAsString(Medic.GetActiveCommand()));
	const USquadUnitAnimInstance* MedicAnimation = Medic.GetAnimBP_SquadUnit();
	if (not IsValid(MedicAnimation))
	{
		Check(false, TEXT("the medic has the squad unit animation instance"));
		return;
	}
	const ESquadMedicHealStance WantedStance = GetIsPatientLow(Patient)
		? ESquadMedicHealStance::Crouch
		: ESquadMedicHealStance::Standing;
	Check(
		MedicAnimation->GetMedicHealAnimationCount(ESquadMedicHealStance::Standing) == 3 &&
		MedicAnimation->GetMedicHealAnimationCount(ESquadMedicHealStance::Crouch) == 3,
		TEXT("the animation Blueprint has three standing and three crouched heal animations"),
		FString::Printf(
			TEXT("standing=%d crouch=%d"),
			MedicAnimation->GetMedicHealAnimationCount(ESquadMedicHealStance::Standing),
			MedicAnimation->GetMedicHealAnimationCount(ESquadMedicHealStance::Crouch)));
	Check(
		MedicAnimation->GetIsMedicHealPlaying() && MedicAnimation->GetMedicHealStance() == WantedStance,
		TEXT("the heal animation plays: crouched for a low patient, standing otherwise"),
		FString::Printf(
			TEXT("playing=%d stance=%s patient_low=%d"),
			MedicAnimation->GetIsMedicHealPlaying() ? 1 : 0,
			MedicAnimation->GetMedicHealStance() == ESquadMedicHealStance::Crouch ? TEXT("crouch") : TEXT("standing"),
			GetIsPatientLow(Patient) ? 1 : 0));
}

void FMedicHealTestScenario::TickFindingCast(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	if (not FindCast(World))
	{
		if (GetHasPhaseTimedOut(World))
		{
			Check(false, TEXT("a player soldier holds cover and two more player squads can supply medics"));
			Finish(World);
		}
		return;
	}
	UE_LOG(
		LogRTSMedicTest,
		Display,
		TEXT("RTS_MEDIC_TEST cast medic=%s second_medic=%s patient=%s second_patient=%s cover_type=%s medic_distance_cm=%.0f"),
		*M_Medic->GetName(),
		*M_SecondMedic->GetName(),
		*M_Patient->GetName(),
		*M_SecondPatient->GetName(),
		*UEnum::GetValueAsString(M_Patient->GetCoverRuntimeState().AssignedCoverPoint.CoverType),
		FVector::Dist2D(M_Medic->GetActorLocation(), M_Patient->GetActorLocation()));
	KeepBystandersHealthy(World);
	SetHealthFraction(*M_Patient, LowHealthFraction);
	SetHealthFraction(*M_SecondPatient, MediumHealthFraction);
	GetMedicComponent(M_Medic.Get())->HealSettings.bAutoHealWhenIdle = true;
	EnterPhase(World, EMedicHealTestPhase::AutoWaitingForPatient);
}

void FMedicHealTestScenario::TickAutoWaitingForPatient(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const ASquadUnit* PickedPatient = MedicComponent->GetPatient();
	if (not IsValid(PickedPatient))
	{
		FinishWhenTimedOut(World, TEXT("an idle medic finds an injured allied soldier by itself"));
		return;
	}
	Check(
		PickedPatient == M_Patient.Get() && not MedicComponent->GetIsOrderedHeal(),
		TEXT("an idle medic picks the most injured allied soldier by itself"),
		FString::Printf(
			TEXT("picked=%s health=%.2f other_health=%.2f"),
			*PickedPatient->GetName(),
			GetHealthFraction(*M_Patient),
			GetHealthFraction(*M_SecondPatient)));
	CheckStandLocation(*MedicComponent, *PickedPatient);
	EnterPhase(World, EMedicHealTestPhase::AutoWaitingForHealing);
}

void FMedicHealTestScenario::TickAutoWaitingForHealing(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	if (MedicComponent->GetHealState() != EMedicHealState::Healing)
	{
		FinishWhenTimedOut(World, TEXT("the medic walks up to its patient and starts healing"));
		return;
	}
	Check(MedicComponent->GetPatient() == M_Patient.Get(), TEXT("the medic walked up to its patient and heals it"));
	CheckMedicIsHealing(*M_Medic, *M_Patient);
	M_PatientHealthAtHealStart = GetHealthFraction(*M_Patient);
	EnterPhase(World, EMedicHealTestPhase::AutoWaitingForHealthGain);
}

void FMedicHealTestScenario::TickAutoWaitingForHealthGain(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	if (GetHealthFraction(*M_Patient) <= M_PatientHealthAtHealStart)
	{
		FinishWhenTimedOut(World, TEXT("healing gives the patient health"));
		return;
	}
	Check(
		true,
		TEXT("healing gives the patient health"),
		FString::Printf(TEXT("%.2f->%.2f"), M_PatientHealthAtHealStart, GetHealthFraction(*M_Patient)));
	StartFollowTest(World);
}

void FMedicHealTestScenario::StartFollowTest(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	// Only the one patient from here on, and one that stays injured for the length of its walk.
	SetHealthFraction(*M_SecondPatient, FullHealthFraction);
	SetHealthFraction(*M_Patient, LowHealthFraction);
	GetMedicComponent(M_Medic.Get())->HealSettings.HealAmountPerTick = SlowHealAmountPerTick;
	M_PatientLocationBeforeWalk = M_Patient->GetActorLocation();
	// Towards the other medic's squad: soldiers stand there, so the way there can be walked.
	const FVector WalkDirection =
		(M_SecondMedicSquad->GetActorLocation() - M_PatientLocationBeforeWalk).GetSafeNormal2D();
	bool bIsNavigable = false;
	const FVector WalkDestination = RTSFunctionLibrary::GetLocationProjected(
		&World,
		M_PatientLocationBeforeWalk + WalkDirection * PatientWalkDistance,
		true,
		bIsNavigable);
	const ECommandQueueError MoveError = M_PatientSquad->MoveToLocation(WalkDestination, true, FRotator::ZeroRotator);
	Check(
		bIsNavigable && MoveError == ECommandQueueError::NoError,
		TEXT("the patient's squad is ordered to walk off while the medic heals"));
	EnterPhase(World, EMedicHealTestPhase::FollowWaitingForReapproach);
}

void FMedicHealTestScenario::TickFollowWaitingForReapproach(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const float PatientWalkedDistance = FVector::Dist2D(M_Patient->GetActorLocation(), M_PatientLocationBeforeWalk);
	const bool bMedicHealsAfterWalk = PatientWalkedDistance >= MinimumPatientWalkDistance &&
		M_PatientSquad->GetIsUnitIdle() && MedicComponent->GetPatient() == M_Patient.Get() &&
		MedicComponent->GetHealState() == EMedicHealState::Healing &&
		FVector::Dist2D(M_Medic->GetActorLocation(), M_Patient->GetActorLocation()) <= MaximumHealDistance;
	if (not bMedicHealsAfterWalk)
	{
		FinishWhenTimedOut(World, TEXT("the medic moves with a patient that walks off and heals it where it stops"));
		return;
	}
	Check(
		true,
		TEXT("the medic moves with a patient that walks off and heals it where it stops"),
		FString::Printf(TEXT("patient_walked_cm=%.0f"), PatientWalkedDistance));
	CheckMedicIsHealing(*M_Medic, *M_Patient);
	StartLeashTest(World);
}

void FMedicHealTestScenario::StartLeashTest(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const FVector FollowAnchor = MedicComponent->Debug_GetAutoHealAnchor();
	const FVector PatientLocation = M_Patient->GetActorLocation();
	MedicComponent->HealSettings.MaxFollowDistance = FVector::Dist2D(FollowAnchor, PatientLocation) + LeashMargin;
	SetHealthFraction(*M_Patient, LowHealthFraction);
	FVector AwayFromAnchor = (PatientLocation - FollowAnchor).GetSafeNormal2D();
	if (AwayFromAnchor.IsNearlyZero())
	{
		AwayFromAnchor = M_Patient->GetActorForwardVector().GetSafeNormal2D();
	}
	bool bIsNavigable = false;
	const FVector WalkDestination = RTSFunctionLibrary::GetLocationProjected(
		&World,
		PatientLocation + AwayFromAnchor * LeashWalkDistance,
		true,
		bIsNavigable);
	const ECommandQueueError MoveError = M_PatientSquad->MoveToLocation(WalkDestination, true, FRotator::ZeroRotator);
	Check(
		bIsNavigable && MoveError == ECommandQueueError::NoError,
		TEXT("the patient's squad is ordered to walk past the medic's follow distance"),
		FString::Printf(
			TEXT("follow_distance_cm=%.0f destination_from_anchor_cm=%.0f"),
			MedicComponent->HealSettings.MaxFollowDistance,
			FVector::Dist2D(FollowAnchor, WalkDestination)));
	bM_MedicFollowedBeforeRelease = false;
	EnterPhase(World, EMedicHealTestPhase::LeashWaitingForRelease);
}

void FMedicHealTestScenario::TickLeashWaitingForRelease(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const FVector FollowAnchor = MedicComponent->Debug_GetAutoHealAnchor();
	const float FollowDistance = MedicComponent->HealSettings.MaxFollowDistance;
	const float PatientDistance = FVector::Dist2D(FollowAnchor, M_Patient->GetActorLocation());
	bM_MedicFollowedBeforeRelease |= MedicComponent->GetHealState() == EMedicHealState::MovingToPatient;
	if (MedicComponent->GetPatient() == M_Patient.Get())
	{
		FinishWhenTimedOut(World, TEXT("the medic lets go of a patient that walked past its follow distance"));
		return;
	}
	Check(
		PatientDistance > FollowDistance && bM_MedicFollowedBeforeRelease,
		TEXT("the medic follows its patient up to the follow distance from its squad and then lets go"),
		FString::Printf(
			TEXT("patient_from_anchor_cm=%.0f follow_distance_cm=%.0f followed=%d medic_from_anchor_cm=%.0f"),
			PatientDistance,
			FollowDistance,
			bM_MedicFollowedBeforeRelease ? 1 : 0,
			FVector::Dist2D(FollowAnchor, M_Medic->GetActorLocation())));
	Check(
		MedicComponent->GetHealState() == EMedicHealState::Idle && M_Medic->GetActiveCommand() == EAbilityID::IdIdle &&
		not GetIsHealAnimationPlaying(*M_Medic),
		TEXT("with nobody else to heal the medic is idle again"),
		*UEnum::GetValueAsString(MedicComponent->GetHealState()));
	MedicComponent->HealSettings.MaxFollowDistance = 0.0f;
	MedicComponent->HealSettings.HealAmountPerTick = TestHealAmountPerTick;
	StartSpreadTest(World);
}

ECommandQueueError FMedicHealTestScenario::OrderMedicSquadToStepAside(UWorld& World) const
{
	const FVector SquadLocation = M_MedicSquad->GetActorLocation();
	const FVector TowardPatient = (M_Patient->GetActorLocation() - SquadLocation).GetSafeNormal2D();
	bool bIsNavigable = false;
	const FVector StepDestination = RTSFunctionLibrary::GetLocationProjected(
		&World,
		SquadLocation + TowardPatient * MedicHealTestScenarioPrivate::MedicSquadStepDistance,
		true,
		bIsNavigable);
	return M_MedicSquad->MoveToLocation(StepDestination, true, FRotator::ZeroRotator);
}

void FMedicHealTestScenario::StartSpreadTest(UWorld& World)
{
	bM_HasHurtSpreadPatients = false;
	EnterPhase(World, EMedicHealTestPhase::SpreadWaitingForTwoPatients);
}

void FMedicHealTestScenario::HurtSpreadPatients(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	bM_HasHurtSpreadPatients = true;
	SetHealthFraction(*M_Patient, EqualHealthFraction);
	SetHealthFraction(*M_SecondPatient, EqualHealthFraction);
	UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	UMedicComponent* SecondMedicComponent = GetMedicComponent(M_SecondMedic.Get());
	// Slow, so both soldiers are still injured by the time the second medic picks.
	MedicComponent->HealSettings.HealAmountPerTick = SlowHealAmountPerTick;
	SecondMedicComponent->HealSettings.HealAmountPerTick = SlowHealAmountPerTick;
	SecondMedicComponent->HealSettings.bAutoHealWhenIdle = true;
	EnterPhase(World, EMedicHealTestPhase::SpreadWaitingForTwoPatients);
}

void FMedicHealTestScenario::TickSpreadWaitingForTwoPatients(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	if (not bM_HasHurtSpreadPatients)
	{
		if (World.GetTimeSeconds() - M_PhaseStartWorldSeconds >= SkippedPatientWaitSeconds)
		{
			HurtSpreadPatients(World);
		}
		return;
	}
	const ASquadUnit* FirstPick = GetMedicComponent(M_Medic.Get())->GetPatient();
	const ASquadUnit* SecondPick = GetMedicComponent(M_SecondMedic.Get())->GetPatient();
	if (not IsValid(FirstPick) || not IsValid(SecondPick))
	{
		FinishWhenTimedOut(World, TEXT("two medics each find a patient"));
		return;
	}
	Check(
		FirstPick != SecondPick,
		TEXT("two medics with two injured soldiers each take one"),
		FString::Printf(TEXT("first=%s second=%s"), *FirstPick->GetName(), *SecondPick->GetName()));
	// One injured soldier left: the medic whose patient is healthy now has nobody else to go to.
	SetHealthFraction(*M_SecondPatient, FullHealthFraction);
	SetHealthFraction(*M_Patient, LowHealthFraction);
	EnterPhase(World, EMedicHealTestPhase::SpreadWaitingForSharedPatient);
}

void FMedicHealTestScenario::TickSpreadWaitingForSharedPatient(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	UMedicComponent* SecondMedicComponent = GetMedicComponent(M_SecondMedic.Get());
	const bool bShareThePatient = MedicComponent->GetPatient() == M_Patient.Get() &&
		SecondMedicComponent->GetPatient() == M_Patient.Get();
	if (not bShareThePatient)
	{
		FinishWhenTimedOut(World, TEXT("two medics heal the same soldier when he is the only one injured"));
		return;
	}
	Check(true, TEXT("two medics heal the same soldier when he is the only one injured"));
	MedicComponent->HealSettings.HealAmountPerTick = TestHealAmountPerTick;
	SecondMedicComponent->HealSettings.HealAmountPerTick = TestHealAmountPerTick;
	SetHealthFraction(*M_Patient, FullHealthFraction);
	EnterPhase(World, EMedicHealTestPhase::SpreadWaitingForIdle);
}

void FMedicHealTestScenario::TickSpreadWaitingForIdle(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const UMedicComponent* SecondMedicComponent = GetMedicComponent(M_SecondMedic.Get());
	const bool bAreBothIdle = MedicComponent->GetHealState() == EMedicHealState::Idle &&
		SecondMedicComponent->GetHealState() == EMedicHealState::Idle;
	if (not bAreBothIdle)
	{
		FinishWhenTimedOut(World, TEXT("medics stop when their patient is healthy"));
		return;
	}
	Check(
		M_Medic->GetActiveCommand() == EAbilityID::IdIdle && M_SecondMedic->GetActiveCommand() == EAbilityID::IdIdle &&
		not GetIsHealAnimationPlaying(*M_Medic) && not GetIsHealAnimationPlaying(*M_SecondMedic),
		TEXT("medics stop healing, and their heal animation, when their patient is healthy"));
	StartOrderTest(World);
}

void FMedicHealTestScenario::StartOrderTest(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	// The second medic sits the heal order out, so who heals whom is down to the order alone.
	GetMedicComponent(M_SecondMedic.Get())->HealSettings.bAutoHealWhenIdle = false;
	FUnitAbilityEntry HealAbilityEntry;
	HealAbilityEntry.AbilityId = EAbilityID::IdHeal;
	HealAbilityEntry.CustomType = static_cast<int32>(EHealAbilityType::HealingActive);
	const bool bAddedAbility = M_MedicSquad->AddAbility(HealAbilityEntry);
	Check(
		bAddedAbility && GetCardHealSubtype(*M_MedicSquad) == static_cast<int32>(EHealAbilityType::HealingActive),
		TEXT("the medic squad's card shows the heal button"));
	Check(
		M_PatientSquad->HealTarget(M_Patient.Get(), true) != ECommandQueueError::NoError,
		TEXT("a squad without the heal ability refuses a heal order"));
	Check(
		M_MedicSquad->CancelHeal() != ECommandQueueError::NoError,
		TEXT("cancel is not on offer while no heal order runs"));

	SetHealthFraction(*M_Patient, MediumHealthFraction);
	SetHealthFraction(*M_SecondPatient, LowHealthFraction);
	const ECommandQueueError HealError = M_MedicSquad->HealTarget(M_Patient.Get(), true);
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	Check(
		HealError == ECommandQueueError::NoError && M_MedicSquad->GetActiveCommandID() == EAbilityID::IdHeal,
		TEXT("the heal order becomes the squad's active command"),
		*UEnum::GetValueAsString(M_MedicSquad->GetActiveCommandID()));
	Check(
		GetCardHealSubtype(*M_MedicSquad) == static_cast<int32>(EHealAbilityType::HealingDisabled),
		TEXT("while the heal order runs the card shows the cancel button"));
	Check(
		MedicComponent->GetIsOrderedHeal() && MedicComponent->GetPatient() == M_Patient.Get(),
		TEXT("the soldier the player pointed at is healed first, although his squad mate is worse off"),
		*GetNameSafe(MedicComponent->GetPatient()));
	EnterPhase(World, EMedicHealTestPhase::OrderWaitingForSecondPatient);
}

void FMedicHealTestScenario::TickOrderWaitingForSecondPatient(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	if (MedicComponent->GetPatient() != M_SecondPatient.Get())
	{
		FinishWhenTimedOut(World, TEXT("on a heal order the medic goes on to the next injured soldier of that squad"));
		return;
	}
	Check(
		GetHealthFraction(*M_Patient) >= HealedHealthFraction && MedicComponent->GetIsOrderedHeal() &&
		M_MedicSquad->GetActiveCommandID() == EAbilityID::IdHeal,
		TEXT("on a heal order the medic goes on to the next injured soldier of that squad"),
		FString::Printf(TEXT("first_patient_health=%.2f"), GetHealthFraction(*M_Patient)));
	EnterPhase(World, EMedicHealTestPhase::OrderWaitingForOrderDone);
}

void FMedicHealTestScenario::TickOrderWaitingForOrderDone(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	if (not M_MedicSquad->GetIsUnitIdle())
	{
		FinishWhenTimedOut(World, TEXT("the heal order ends when the squad needs no more healing"));
		return;
	}
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	Check(
		GetHealthFraction(*M_SecondPatient) >= HealedHealthFraction && not MedicComponent->GetIsOrderedHeal() &&
		MedicComponent->GetHealState() == EMedicHealState::Idle,
		TEXT("the heal order ends when the squad needs no more healing"),
		FString::Printf(TEXT("second_patient_health=%.2f"), GetHealthFraction(*M_SecondPatient)));
	Check(
		GetCardHealSubtype(*M_MedicSquad) == static_cast<int32>(EHealAbilityType::HealingActive),
		TEXT("after the heal order the card shows the heal button again"));
	StartCancelTest(World);
}

void FMedicHealTestScenario::StartCancelTest(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	SetHealthFraction(*M_Patient, LowHealthFraction);
	const ECommandQueueError HealError = M_MedicSquad->HealTarget(M_Patient.Get(), true);
	const bool bShowsCancelButton =
		GetCardHealSubtype(*M_MedicSquad) == static_cast<int32>(EHealAbilityType::HealingDisabled);
	const ECommandQueueError CancelError = M_MedicSquad->CancelHeal();
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	Check(
		HealError == ECommandQueueError::NoError && bShowsCancelButton && CancelError == ECommandQueueError::NoError,
		TEXT("the cancel button is accepted while a heal order runs"));
	Check(
		M_MedicSquad->GetIsUnitIdle() && MedicComponent->GetHealState() == EMedicHealState::Idle &&
		not MedicComponent->GetIsOrderedHeal() && not IsValid(MedicComponent->GetPatient()) &&
		M_Medic->GetActiveCommand() == EAbilityID::IdIdle,
		TEXT("cancelling ends the heal order at once"),
		*UEnum::GetValueAsString(M_MedicSquad->GetActiveCommandID()));
	Check(
		GetCardHealSubtype(*M_MedicSquad) == static_cast<int32>(EHealAbilityType::HealingActive) &&
		MedicComponent->GetIsAutoHealSuppressed(),
		TEXT("after cancelling the card shows the heal button again"));
	EnterPhase(World, EMedicHealTestPhase::CancelWaitingWhileSuppressed);
}

void FMedicHealTestScenario::TickCancelWaitingWhileSuppressed(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	const bool bWentBackToPatient = IsValid(MedicComponent->GetPatient());
	if (not bWentBackToPatient && World.GetTimeSeconds() - M_PhaseStartWorldSeconds < SuppressedWaitSeconds)
	{
		return;
	}
	Check(
		not bWentBackToPatient && GetHealthFraction(*M_Patient) < HealedHealthFraction,
		TEXT("a medic that was told to stop does not walk back to its patient by itself"));
	const ECommandQueueError MoveError = OrderMedicSquadToStepAside(World);
	Check(
		MoveError == ECommandQueueError::NoError && not MedicComponent->GetIsAutoHealSuppressed(),
		TEXT("the squad's next command lets the medic heal by itself again"));
	EnterPhase(World, EMedicHealTestPhase::ResumeWaitingForAutoHeal);
}

void FMedicHealTestScenario::TickResumeWaitingForAutoHeal(UWorld& World)
{
	using namespace MedicHealTestScenarioPrivate;
	const UMedicComponent* MedicComponent = GetMedicComponent(M_Medic.Get());
	if (MedicComponent->GetPatient() != M_Patient.Get())
	{
		FinishWhenTimedOut(World, TEXT("once its squad is idle again the medic heals by itself again"));
		return;
	}
	Check(
		not MedicComponent->GetIsOrderedHeal() && M_MedicSquad->GetIsUnitIdle(),
		TEXT("once its squad is idle again the medic heals by itself again"));
	const ECommandQueueError MoveError = OrderMedicSquadToStepAside(World);
	Check(
		MoveError == ECommandQueueError::NoError && MedicComponent->GetHealState() == EMedicHealState::Idle &&
		not IsValid(MedicComponent->GetPatient()) && M_Medic->GetActiveCommand() != EAbilityID::IdHeal &&
		not GetIsHealAnimationPlaying(*M_Medic),
		TEXT("a command for its squad takes the medic off its patient at once"),
		*UEnum::GetValueAsString(M_Medic->GetActiveCommand()));
	SetHealthFraction(*M_Patient, FullHealthFraction);
	Finish(World);
}

#endif // RTS_WITH_MAP_TESTS
