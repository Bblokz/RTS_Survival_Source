#include "CoverStressTestScenario.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Behaviours/Derived/InfantryStress/InfantryStressBehaviour.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverStressTest, Log, All);

namespace CoverStressTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float FindSoldierTimeoutSeconds = 60.0f;
	// Added to the stress times of the settings, for the soldier's stress timer and the staggered cover update.
	constexpr float PhaseMarginSeconds = 3.0f;
	// In front of the soldier's cover, well inside rifle range.
	constexpr float EnemyDistanceInFrontOfCover = 1500.0f;
	constexpr float EnemyDropHeight = 60.0f;
	constexpr float MaximumEnemyDriftFromPost = 150.0f;
	// One light hit must not stress anybody; the hits after it come fast enough to stress everybody.
	constexpr float LightHitHealthRatio = 0.02f;
	constexpr float HeavyHitHealthRatio = 0.12f;
	constexpr int32 MaximumHeavyHits = 20;
	constexpr float LowestHealthRatioToHitAt = 0.3f;
	constexpr float MinimumRetreatGain = 250.0f;
	constexpr float ValueTolerance = 0.01f;

	const TCHAR* const CrouchSprayAnimationPaths[] =
	{
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/CoverAnimations/CoverSpray/Anim_SprayCrouchCover_1.Anim_SprayCrouchCover_1"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/CoverAnimations/CoverSpray/Anim_SprayCrouchCover_2.Anim_SprayCrouchCover_2"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/CoverAnimations/CoverSpray/Anim_SprayCrouchCover_3.Anim_SprayCrouchCover_3")
	};

	const TCHAR* GetPhaseName(const ESquadUnitStressPhase Phase)
	{
		switch (Phase)
		{
		case ESquadUnitStressPhase::SprayingFromCover:
			return TEXT("SprayingFromCover");
		case ESquadUnitStressPhase::RelocatingToFartherCover:
			return TEXT("RelocatingToFartherCover");
		case ESquadUnitStressPhase::SprayingFromNewCover:
			return TEXT("SprayingFromNewCover");
		case ESquadUnitStressPhase::None:
		default:
			return TEXT("None");
		}
	}

	bool GetIsSprayPlaying(const ASquadUnit& Soldier)
	{
		const USquadUnitAnimInstance* UnitAnimation = Soldier.GetAnimBP_SquadUnit();
		return IsValid(UnitAnimation) && UnitAnimation->GetIsCoverSprayPlaying();
	}
}

int32 FCoverStressTestScenario::GiveSoldierCrouchSprayAnimationsWhenMissing(USquadUnitAnimInstance& UnitAnimation)
{
	const int32 AssignedCount = UnitAnimation.GetCoverSprayAnimationCount(ESquadCoverSprayStance::Crouch);
	if (AssignedCount > 0)
	{
		return AssignedCount;
	}
	TArray<UAnimSequenceBase*> SprayAnimations;
	for (const TCHAR* SprayAnimationPath : CoverStressTestScenarioPrivate::CrouchSprayAnimationPaths)
	{
		if (UAnimSequenceBase* SprayAnimation = LoadObject<UAnimSequenceBase>(nullptr, SprayAnimationPath))
		{
			SprayAnimations.Add(SprayAnimation);
		}
	}
	UnitAnimation.Debug_SetCoverSprayAnimations(ESquadCoverSprayStance::Crouch, SprayAnimations);
	return 0;
}

void FCoverStressTestScenario::Start()
{
	M_Phase = ECoverStressTestPhase::WaitingForCoverScan;
	UE_LOG(LogRTSCoverStressTest, Display, TEXT("RTS_COVER_STRESS_TEST scheduled"));
}

void FCoverStressTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(
		LogRTSCoverStressTest,
		Display,
		TEXT("RTS_COVER_STRESS_TEST check=%s %s %s"),
		bCondition ? TEXT("PASS") : TEXT("FAIL"),
		Description,
		*Details);
}

void FCoverStressTestScenario::EnterPhase(
	const UWorld& World,
	const ECoverStressTestPhase NewPhase,
	const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseDeadlineWorldSeconds = World.GetTimeSeconds() + TimeoutSeconds;
}

bool FCoverStressTestScenario::GetHasPhaseTimedOut(const UWorld& World) const
{
	return World.GetTimeSeconds() >= M_PhaseDeadlineWorldSeconds;
}

void FCoverStressTestScenario::Finish(const UWorld& World)
{
	if (ASquadUnit* Enemy = M_Enemy.Get())
	{
		Enemy->SetActorLocation(M_EnemyStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	M_Phase = ECoverStressTestPhase::Inactive;
	UE_LOG(
		LogRTSCoverStressTest,
		Display,
		TEXT("RTS_COVER_STRESS_TEST RESULT %s failed_checks=%d map=%s"),
		M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
		M_FailedCheckCount,
		*World.GetName());
}

void FCoverStressTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ECoverStressTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ECoverStressTestPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			EnterPhase(*World, ECoverStressTestPhase::FindingSoldierInCover,
			           CoverStressTestScenarioPrivate::FindSoldierTimeoutSeconds);
		}
		return;
	}
	KeepEnemyAtPost(*World);
	if (M_Phase == ECoverStressTestPhase::FindingSoldierInCover)
	{
		TickFindingSoldier(*World);
		return;
	}
	if (M_Phase == ECoverStressTestPhase::WaitingForRelocation)
	{
		TickWaitingForRelocation(*World);
		return;
	}
	if (M_Phase == ECoverStressTestPhase::WaitingForSprayFromNewCover)
	{
		TickWaitingForSprayFromNewCover(*World);
		return;
	}
	TickWaitingForCalm(*World);
}

ASquadUnit* FCoverStressTestScenario::FindPlayerSoldierInCrouchCover(UWorld& World) const
{
	// A soldier whose weapon the behaviour's calibre rules leave alone only tests half of the stress, so one
	// with a weapon it changes is preferred.
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const UClass* BehaviourClass = IsValid(CoverSettings)
		? CoverSettings->M_InfantryStressBehaviourClass.LoadSynchronous()
		: nullptr;
	const UInfantryStressBehaviour* DefaultBehaviour = IsValid(BehaviourClass)
		? Cast<UInfantryStressBehaviour>(BehaviourClass->GetDefaultObject())
		: nullptr;
	ASquadUnit* SoldierWithUnchangedWeapon = nullptr;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		const bool bIsUsableSoldier = IsValid(SquadUnit) && SquadUnit->IsUnitAlive() &&
			SquadUnit->GetOwningPlayer() == CoverStressTestScenarioPrivate::PlayerOwnedTeam &&
			SquadUnit->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected &&
			SquadUnit->GetCoverRuntimeState().AssignedCoverPoint.CoverType == ERTSCoverType::Crouch &&
			IsValid(SquadUnit->GetWeaponState()) && IsValid(SquadUnit->GetAnimBP_SquadUnit());
		if (not bIsUsableSoldier)
		{
			continue;
		}
		if (not IsValid(DefaultBehaviour) || DefaultBehaviour->GetAppliesToWeapon(SquadUnit->GetWeaponState()))
		{
			return SquadUnit;
		}
		SoldierWithUnchangedWeapon = SquadUnit;
	}
	// Waiting a little longer may still turn one up; at the end of the wait anybody will do.
	return GetHasPhaseTimedOut(World) ? SoldierWithUnchangedWeapon : nullptr;
}

bool FCoverStressTestScenario::PlaceEnemyAtPost(UWorld& World)
{
	using namespace CoverStressTestScenarioPrivate;
	ASquadUnit* Enemy = M_Enemy.Get();
	if (not IsValid(Enemy) || not Enemy->IsUnitAlive())
	{
		Enemy = nullptr;
		for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator && Enemy == nullptr; ++UnitIterator)
		{
			ASquadUnit* Candidate = *UnitIterator;
			const bool bIsLivingEnemy = IsValid(Candidate) && Candidate->IsUnitAlive() &&
				Candidate->GetOwningPlayer() != PlayerOwnedTeam;
			Enemy = bIsLivingEnemy ? Candidate : nullptr;
		}
		if (not IsValid(Enemy))
		{
			return false;
		}
		M_Enemy = Enemy;
		M_EnemyStartLocation = Enemy->GetActorLocation();
	}
	// Dropped onto the ground from a little above it; where the enemy came from may lie higher or lower.
	const UCapsuleComponent* EnemyCapsule = Enemy->GetCapsuleComponent();
	const float EnemyHalfHeight = IsValid(EnemyCapsule) ? EnemyCapsule->GetScaledCapsuleHalfHeight() : 0.0f;
	Enemy->SetActorLocation(
		M_EnemyPost + FVector::UpVector * (EnemyHalfHeight + EnemyDropHeight),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	return true;
}

void FCoverStressTestScenario::KeepEnemyAtPost(UWorld& World)
{
	const ASquadUnit* Enemy = M_Enemy.Get();
	const bool bIsAtPost = IsValid(Enemy) && FVector::Dist2D(Enemy->GetActorLocation(), M_EnemyPost) <=
		CoverStressTestScenarioPrivate::MaximumEnemyDriftFromPost;
	if (M_Phase == ECoverStressTestPhase::FindingSoldierInCover || bIsAtPost)
	{
		return;
	}
	PlaceEnemyAtPost(World);
}

void FCoverStressTestScenario::TickFindingSoldier(UWorld& World)
{
	using namespace CoverStressTestScenarioPrivate;
	ASquadUnit* Soldier = FindPlayerSoldierInCrouchCover(World);
	if (not IsValid(Soldier) && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(IsValid(Soldier), TEXT("a player soldier holds crouch cover"));
	if (not IsValid(Soldier))
	{
		Finish(World);
		return;
	}
	M_Soldier = Soldier;
	M_FirstCoverPoint = Soldier->GetCoverRuntimeState().AssignedCoverPoint;
	M_EnemyPost = M_FirstCoverPoint.Location -
		M_FirstCoverPoint.CoverNormal.GetSafeNormal2D() * EnemyDistanceInFrontOfCover;
	const bool bHasEnemy = PlaceEnemyAtPost(World);
	Check(bHasEnemy, TEXT("an enemy soldier is alive to be stressed by"));
	if (not bHasEnemy || not StressSoldier(World, *Soldier))
	{
		Finish(World);
		return;
	}
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	EnterPhase(
		World,
		ECoverStressTestPhase::WaitingForRelocation,
		(IsValid(CoverSettings) ? CoverSettings->M_InfantryStressDuration : 0.0f) + PhaseMarginSeconds);
}

void FCoverStressTestScenario::RecordBaseline(
	const UWeaponState& SoldierWeapon,
	const UCharacterMovementComponent& SoldierMovement)
{
	const FWeaponData& WeaponData = SoldierWeapon.GetRawWeaponData();
	M_Baseline.MagCapacity = WeaponData.MagCapacity;
	M_Baseline.Accuracy = WeaponData.Accuracy;
	M_Baseline.BaseCooldown = WeaponData.BaseCooldown;
	M_Baseline.ReloadSpeed = WeaponData.ReloadSpeed;
	M_Baseline.MaxWalkSpeed = SoldierMovement.MaxWalkSpeed;
}

bool FCoverStressTestScenario::StressSoldier(UWorld& World, ASquadUnit& Soldier)
{
	using namespace CoverStressTestScenarioPrivate;
	const UHealthComponent* SoldierHealth = Soldier.GetHealthComponent();
	const UWeaponState* SoldierWeapon = Soldier.GetWeaponState();
	const UCharacterMovementComponent* SoldierMovement = Soldier.GetCharacterMovement();
	const ASquadController* SoldierSquad = Soldier.GetSquadControllerChecked();
	USquadUnitAnimInstance* SoldierAnimation = Soldier.GetAnimBP_SquadUnit();
	const float StressTolerance = IsValid(SoldierSquad) ? SoldierSquad->GetStressTolerance() : 0.0f;
	Check(
		StressTolerance > 0.0f && StressTolerance < 1.0f,
		TEXT("the soldier's squad has a stress tolerance"),
		FString::Printf(TEXT("soldier=%s tolerance=%.2f"), *Soldier.GetName(), StressTolerance));
	if (not IsValid(SoldierHealth) || not IsValid(SoldierWeapon) || not IsValid(SoldierMovement) ||
		not IsValid(SoldierAnimation) || StressTolerance <= 0.0f)
	{
		return false;
	}
	const int32 AssignedSprayAnimationCount = GiveSoldierCrouchSprayAnimationsWhenMissing(*SoldierAnimation);
	UE_LOG(
		LogRTSCoverStressTest,
		Display,
		TEXT("RTS_COVER_STRESS_TEST animation blueprint has %d crouch spray animations assigned; playing %d"),
		AssignedSprayAnimationCount,
		SoldierAnimation->GetCoverSprayAnimationCount(ESquadCoverSprayStance::Crouch));
	RecordBaseline(*SoldierWeapon, *SoldierMovement);

	ASquadUnit* Enemy = M_Enemy.Get();
	const float MaxHealth = SoldierHealth->GetMaxHealth();
	Soldier.TakeDamage(MaxHealth * LightHitHealthRatio, FDamageEvent(), nullptr, Enemy);
	Check(not Soldier.GetIsStressed(), TEXT("one light hit does not stress the soldier"));
	int32 HeavyHitCount = 0;
	while (not Soldier.GetIsStressed() && Soldier.IsUnitAlive() && HeavyHitCount < MaximumHeavyHits &&
		SoldierHealth->GetHealthPercentage() > LowestHealthRatioToHitAt)
	{
		Soldier.TakeDamage(MaxHealth * HeavyHitHealthRatio, FDamageEvent(), nullptr, Enemy);
		++HeavyHitCount;
	}
	const FSquadUnitStressState& Stress = Soldier.GetStressState();
	Check(
		Soldier.GetIsStressed() && Stress.Phase == ESquadUnitStressPhase::SprayingFromCover,
		TEXT("losing health quickly in cover stresses the soldier"),
		FString::Printf(
			TEXT("hits=%d health_left=%.0f%% phase=%s"),
			HeavyHitCount,
			SoldierHealth->GetHealthPercentage() * 100.0f,
			GetPhaseName(Stress.Phase)));
	if (not Soldier.GetIsStressed())
	{
		return false;
	}
	CheckStressedWeapon(Soldier);
	Check(
		not IsValid(Stress.Behaviour) || not Stress.Behaviour->GetIsOnWeapon() ||
		SoldierWeapon->GetCurrentMagCapacity() == SoldierWeapon->GetRawWeaponData().MagCapacity,
		TEXT("the larger magazine is full from the start"),
		FString::Printf(TEXT("rounds=%d"), SoldierWeapon->GetCurrentMagCapacity()));
	Check(
		GetIsSprayPlaying(Soldier),
		TEXT("the spray animation starts on the frame the stress does"));
	Check(
		FMath::IsNearlyEqual(SoldierMovement->MaxWalkSpeed, M_Baseline.MaxWalkSpeed, ValueTolerance),
		TEXT("the soldier is no faster while he sprays from his first cover"));
	return true;
}

void FCoverStressTestScenario::CheckStressedWeapon(const ASquadUnit& Soldier)
{
	using CoverStressTestScenarioPrivate::ValueTolerance;
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const UInfantryStressBehaviour* Behaviour = Soldier.GetStressState().Behaviour;
	const UWeaponState* SoldierWeapon = Soldier.GetWeaponState();
	const UClass* SettingsClass = IsValid(CoverSettings)
		? CoverSettings->M_InfantryStressBehaviourClass.Get()
		: nullptr;
	Check(
		IsValid(Behaviour) && Behaviour->GetClass() == SettingsClass && Behaviour->GetStressedSoldier() == &Soldier,
		TEXT("the soldier got the stress behaviour class of the cover settings"),
		GetNameSafe(IsValid(Behaviour) ? Behaviour->GetClass() : nullptr));
	if (not IsValid(Behaviour) || not IsValid(SoldierWeapon))
	{
		return;
	}
	if (not Behaviour->GetIsOnWeapon())
	{
		const FWeaponData& UnchangedWeaponData = SoldierWeapon->GetRawWeaponData();
		Check(
			UnchangedWeaponData.MagCapacity == M_Baseline.MagCapacity && UnchangedWeaponData.Accuracy == M_Baseline.Accuracy,
			TEXT("a weapon outside the behaviour's calibre rules is left as it is"),
			FString::Printf(TEXT("calibre=%.0f"), UnchangedWeaponData.WeaponCalibre));
		return;
	}
	// The same arithmetic the weapon behaviour uses, so a Blueprint class with other multipliers passes too.
	const FBehaviourWeaponMultipliers& Multipliers = Behaviour->GetBehaviourWeaponMultipliers();
	const FWeaponData& WeaponData = SoldierWeapon->GetRawWeaponData();
	const int32 ExpectedMagCapacity = M_Baseline.MagCapacity +
		FMath::CeilToInt(M_Baseline.MagCapacity * (Multipliers.MagSizeMlt - 1.0f));
	const int32 ExpectedAccuracy = M_Baseline.Accuracy +
		FMath::CeilToInt(M_Baseline.Accuracy * (Multipliers.AccuracyMlt - 1.0f));
	const bool bWeaponIsStressed = WeaponData.MagCapacity == ExpectedMagCapacity &&
		WeaponData.Accuracy == ExpectedAccuracy &&
		FMath::IsNearlyEqual(WeaponData.BaseCooldown, M_Baseline.BaseCooldown * Multipliers.BaseCooldownMlt, ValueTolerance) &&
		FMath::IsNearlyEqual(WeaponData.ReloadSpeed, M_Baseline.ReloadSpeed * Multipliers.ReloadSpeedMlt, ValueTolerance);
	Check(
		bWeaponIsStressed,
		TEXT("the weapon has the behaviour's magazine, cooldown, reload and accuracy"),
		FString::Printf(
			TEXT("mag %d->%d (x%.2f) cooldown %.2f->%.2f (x%.2f) reload %.2f->%.2f (x%.2f) accuracy %d->%d (x%.2f)"),
			M_Baseline.MagCapacity, WeaponData.MagCapacity, Multipliers.MagSizeMlt,
			M_Baseline.BaseCooldown, WeaponData.BaseCooldown, Multipliers.BaseCooldownMlt,
			M_Baseline.ReloadSpeed, WeaponData.ReloadSpeed, Multipliers.ReloadSpeedMlt,
			M_Baseline.Accuracy, WeaponData.Accuracy, Multipliers.AccuracyMlt));
}

void FCoverStressTestScenario::TickWaitingForRelocation(UWorld& World)
{
	using namespace CoverStressTestScenarioPrivate;
	ASquadUnit* Soldier = M_Soldier.Get();
	const ESquadUnitStressPhase StressPhase = IsValid(Soldier)
		? Soldier->GetStressState().Phase
		: ESquadUnitStressPhase::None;
	if (StressPhase == ESquadUnitStressPhase::SprayingFromCover && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const UCharacterMovementComponent* SoldierMovement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
	if (not IsValid(Soldier) || not Soldier->IsUnitAlive() || not IsValid(CoverSettings) || not IsValid(SoldierMovement))
	{
		Check(false, TEXT("the soldier lived to run for other cover"));
		Finish(World);
		return;
	}
	const FRTSCoverPoint& NewPoint = Soldier->GetCoverRuntimeState().AssignedCoverPoint;
	const float RetreatGain = FVector::Dist2D(NewPoint.Location, M_EnemyPost) -
		FVector::Dist2D(M_FirstCoverPoint.Location, M_EnemyPost);
	Check(
		StressPhase == ESquadUnitStressPhase::RelocatingToFartherCover && Soldier->GetHasCoverAssignment() &&
		NewPoint.PointId != M_FirstCoverPoint.PointId && RetreatGain >= MinimumRetreatGain,
		TEXT("halfway the soldier leaves for cover further from his enemy"),
		FString::Printf(
			TEXT("phase=%s to_type=%s further_cm=%.0f run_cm=%.0f"),
			GetPhaseName(StressPhase),
			*UEnum::GetValueAsString(NewPoint.CoverType),
			RetreatGain,
			FVector::Dist2D(NewPoint.Location, M_FirstCoverPoint.Location)));
	Check(
		FVector::Dist2D(NewPoint.Location, M_FirstCoverPoint.Location) <=
		CoverSettings->M_InfantryStressedCoverMaxDisplacement,
		TEXT("the new cover lies within the stressed cover displacement"));
	Check(
		FMath::IsNearlyEqual(
			SoldierMovement->MaxWalkSpeed,
			M_Baseline.MaxWalkSpeed * CoverSettings->M_InfantryStressSpeedMultiplier,
			ValueTolerance),
		TEXT("the soldier runs there faster"),
		FString::Printf(TEXT("speed %.0f->%.0f"), M_Baseline.MaxWalkSpeed, SoldierMovement->MaxWalkSpeed));
	Check(not GetIsSprayPlaying(*Soldier), TEXT("the soldier does not spray while he runs"));
	EnterPhase(
		World,
		ECoverStressTestPhase::WaitingForSprayFromNewCover,
		CoverSettings->M_InfantryStressDuration + PhaseMarginSeconds);
}

void FCoverStressTestScenario::TickWaitingForSprayFromNewCover(UWorld& World)
{
	using namespace CoverStressTestScenarioPrivate;
	const ASquadUnit* Soldier = M_Soldier.Get();
	const bool bIsStressed = IsValid(Soldier) && Soldier->GetIsStressed();
	const bool bSpraysFromNewCover = bIsStressed &&
		Soldier->GetStressState().Phase == ESquadUnitStressPhase::SprayingFromNewCover &&
		Soldier->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected && GetIsSprayPlaying(*Soldier);
	if (not bSpraysFromNewCover && bIsStressed && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(
		bSpraysFromNewCover,
		TEXT("the soldier sprays again from his new cover"),
		IsValid(Soldier)
			? FString::Printf(
				TEXT("phase=%s cover_state=%s cover_type=%s spraying=%d"),
				GetPhaseName(Soldier->GetStressState().Phase),
				*UEnum::GetValueAsString(Soldier->GetCoverRuntimeState().State),
				*UEnum::GetValueAsString(Soldier->GetCoverRuntimeState().AssignedCoverPoint.CoverType),
				GetIsSprayPlaying(*Soldier) ? 1 : 0)
			: FString());
	if (bSpraysFromNewCover)
	{
		CheckStressedWeapon(*Soldier);
	}
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	EnterPhase(
		World,
		ECoverStressTestPhase::WaitingForCalm,
		(IsValid(CoverSettings) ? CoverSettings->M_InfantryStressDuration : 0.0f) + PhaseMarginSeconds);
}

void FCoverStressTestScenario::TickWaitingForCalm(UWorld& World)
{
	using CoverStressTestScenarioPrivate::ValueTolerance;
	ASquadUnit* Soldier = M_Soldier.Get();
	if (IsValid(Soldier) && Soldier->GetIsStressed() && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	const UWeaponState* SoldierWeapon = IsValid(Soldier) ? Soldier->GetWeaponState() : nullptr;
	const UCharacterMovementComponent* SoldierMovement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
	if (not IsValid(SoldierWeapon) || not IsValid(SoldierMovement) || not Soldier->IsUnitAlive())
	{
		Check(false, TEXT("the soldier lived through his stress"));
		Finish(World);
		return;
	}
	Check(not Soldier->GetIsStressed(), TEXT("the stress wears off after the stress duration"));
	const FWeaponData& WeaponData = SoldierWeapon->GetRawWeaponData();
	const bool bWeaponIsBackToNormal = WeaponData.MagCapacity == M_Baseline.MagCapacity &&
		WeaponData.Accuracy == M_Baseline.Accuracy &&
		FMath::IsNearlyEqual(WeaponData.BaseCooldown, M_Baseline.BaseCooldown, ValueTolerance) &&
		FMath::IsNearlyEqual(WeaponData.ReloadSpeed, M_Baseline.ReloadSpeed, ValueTolerance);
	Check(
		bWeaponIsBackToNormal,
		TEXT("the weapon is back to what it was"),
		FString::Printf(
			TEXT("mag=%d cooldown=%.2f reload=%.2f accuracy=%d"),
			WeaponData.MagCapacity, WeaponData.BaseCooldown, WeaponData.ReloadSpeed, WeaponData.Accuracy));
	Check(
		FMath::IsNearlyEqual(SoldierMovement->MaxWalkSpeed, M_Baseline.MaxWalkSpeed, ValueTolerance),
		TEXT("the soldier's speed is back to what it was"),
		FString::Printf(TEXT("speed=%.0f"), SoldierMovement->MaxWalkSpeed));
	Check(
		not CoverStressTestScenarioPrivate::GetIsSprayPlaying(*Soldier) &&
		not IsValid(Soldier->GetStressState().Behaviour),
		TEXT("the spraying and the behaviour are gone"));

	// Straight after his stress the soldier is given time to recover.
	const UHealthComponent* SoldierHealth = Soldier->GetHealthComponent();
	const float HealthToTake = IsValid(SoldierHealth) ? SoldierHealth->GetCurrentHealth() * 0.5f : 0.0f;
	Soldier->TakeDamage(HealthToTake, FDamageEvent(), nullptr, M_Enemy.Get());
	Check(not Soldier->GetIsStressed(), TEXT("a soldier that just calmed down is not stressed again at once"));
	Finish(World);
}
