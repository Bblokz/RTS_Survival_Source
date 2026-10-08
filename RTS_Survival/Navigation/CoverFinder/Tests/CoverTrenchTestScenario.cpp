#include "CoverTrenchTestScenario.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePreviewComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverTrenchTest, Log, All);

namespace CoverTrenchTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float WalkTimeoutSeconds = 60.0f;
	constexpr float StandUpTimeoutSeconds = 25.0f;
	constexpr float CrouchAgainTimeoutSeconds = 25.0f;
	// The reload the test starts, and how long the soldier gets to duck and to come back up after it.
	constexpr float TestReloadSeconds = 2.5f;
	constexpr float ReloadDuckTimeoutSeconds = 2.5f;
	constexpr float StandUpAfterReloadTimeoutSeconds = TestReloadSeconds + 10.0f;
	// In front of the trench, well inside rifle range.
	constexpr float EnemyDistanceInFrontOfTrench = 1200.0f;
	// Standing up must not move the soldier; this only allows for the floor under his feet.
	constexpr float MaximumStandUpTravel = 15.0f;

	ESquadIdleAnimationPose GetIdlePose(const ASquadUnit& SquadUnit)
	{
		const USquadUnitAnimInstance* UnitAnimation = SquadUnit.GetAnimBP_SquadUnit();
		return IsValid(UnitAnimation) ? UnitAnimation->GetIdleAnimationPose() : ESquadIdleAnimationPose::Regular;
	}

	bool GetIsInTrenchCover(const ASquadUnit& SquadUnit)
	{
		return SquadUnit.GetIsOccupyingCover() &&
			SquadUnit.GetCoverRuntimeState().AssignedCoverPoint.CoverType == ERTSCoverType::TrenchStandUp;
	}
}

void FCoverTrenchTestScenario::Start()
{
	M_Phase = ECoverTrenchTestPhase::WaitingForCoverScan;
	UE_LOG(LogRTSCoverTrenchTest, Display, TEXT("RTS_COVER_TRENCH_TEST scheduled"));
}

void FCoverTrenchTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(
		LogRTSCoverTrenchTest,
		Display,
		TEXT("RTS_COVER_TRENCH_TEST check=%s %s %s"),
		bCondition ? TEXT("PASS") : TEXT("FAIL"),
		Description,
		*Details);
}

void FCoverTrenchTestScenario::EnterPhase(
	const UWorld& World,
	const ECoverTrenchTestPhase NewPhase,
	const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseDeadlineWorldSeconds = World.GetTimeSeconds() + TimeoutSeconds;
}

bool FCoverTrenchTestScenario::GetHasPhaseTimedOut(const UWorld& World) const
{
	return World.GetTimeSeconds() >= M_PhaseDeadlineWorldSeconds;
}

void FCoverTrenchTestScenario::Finish(const UWorld& World)
{
	if (ASquadUnit* Enemy = M_Enemy.Get())
	{
		Enemy->SetActorLocation(M_EnemyStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	M_Phase = ECoverTrenchTestPhase::Inactive;
	UE_LOG(
		LogRTSCoverTrenchTest,
		Display,
		TEXT("RTS_COVER_TRENCH_TEST RESULT %s failed_checks=%d map=%s"),
		M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
		M_FailedCheckCount,
		*World.GetName());
}

void FCoverTrenchTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ECoverTrenchTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ECoverTrenchTestPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (not CoverSubsystem.GetHasCompletedFullScan())
		{
			return;
		}
		if (not SendSquadIntoTrench(*World, CoverSubsystem))
		{
			Finish(*World);
		}
		return;
	}
	if (M_Phase == ECoverTrenchTestPhase::WalkingToTrench)
	{
		TickWalkingToTrench(*World);
		return;
	}
	if (M_Phase == ECoverTrenchTestPhase::WaitingForStandUp)
	{
		TickWaitingForStandUp(*World, CoverSubsystem);
		return;
	}
	if (M_Phase == ECoverTrenchTestPhase::WaitingForReloadDuck)
	{
		TickWaitingForReloadDuck(*World);
		return;
	}
	if (M_Phase == ECoverTrenchTestPhase::WaitingForStandUpAfterReload)
	{
		TickWaitingForStandUpAfterReload(*World);
		return;
	}
	TickWaitingForCrouchAgain(*World);
}

bool FCoverTrenchTestScenario::SendSquadIntoTrench(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace CoverTrenchTestScenarioPrivate;
	TArray<FRTSCoverPoint> TrenchPoints;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		if (CoverPoint.CoverType == ERTSCoverType::TrenchStandUp)
		{
			TrenchPoints.Add(CoverPoint);
		}
	}
	Check(
		not TrenchPoints.IsEmpty(),
		TEXT("the map has trench cover"),
		FString::Printf(TEXT("trenches=%d points=%d"), CoverSubsystem.GetTrenchCoverActorCount(), TrenchPoints.Num()));

	ASquadController* PlayerSquad = nullptr;
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
	{
		ASquadController* SquadController = *SquadIterator;
		const URTSComponent* SquadRTSComponent = IsValid(SquadController) ? SquadController->GetRTSComponent() : nullptr;
		const bool bIsPlannablePlayerSquad = IsValid(SquadRTSComponent) &&
			SquadRTSComponent->GetOwningPlayer() == PlayerOwnedTeam &&
			USquadMovePreviewComponent::GetCanPlanForSquads({SquadController});
		if (bIsPlannablePlayerSquad)
		{
			PlayerSquad = SquadController;
			break;
		}
	}
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (IsValid(SquadUnit) && SquadUnit->IsUnitAlive() && SquadUnit->GetOwningPlayer() != PlayerOwnedTeam)
		{
			M_Enemy = SquadUnit;
			M_EnemyStartLocation = SquadUnit->GetActorLocation();
			break;
		}
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	USquadMovePreviewComponent* PreviewComponent = IsValid(PlayerController)
		? PlayerController->FindComponentByClass<USquadMovePreviewComponent>()
		: nullptr;
	Check(IsValid(PlayerSquad) && M_Enemy.IsValid() && IsValid(PreviewComponent), TEXT("a player squad, an enemy and the move planner exist"));
	if (TrenchPoints.IsEmpty() || not IsValid(PlayerSquad) || not M_Enemy.IsValid() || not IsValid(PreviewComponent))
	{
		return false;
	}

	// The trench point nearest to the squad, ordered through the same planned move a player's click issues.
	const FVector SquadLocation = PlayerSquad->GetActorLocation();
	TrenchPoints.Sort([&SquadLocation](const FRTSCoverPoint& Left, const FRTSCoverPoint& Right)
	{
		return FVector::DistSquared(Left.Location, SquadLocation) < FVector::DistSquared(Right.Location, SquadLocation);
	});
	M_TrenchPoint = TrenchPoints[0];
	uint32 IssuedCommandCount = 0;
	const bool bIssued = PreviewComponent->TryIssuePlannedMove(
		{PlayerSquad},
		M_TrenchPoint.Location,
		false,
		FRotator::ZeroRotator,
		false,
		IssuedCommandCount);
	Check(
		bIssued && IssuedCommandCount == 1,
		TEXT("the squad accepted a move onto the trench"),
		FString::Printf(TEXT("distance_cm=%.0f"), FVector::Dist2D(SquadLocation, M_TrenchPoint.Location)));
	EnterPhase(World, ECoverTrenchTestPhase::WalkingToTrench, WalkTimeoutSeconds);
	return bIssued;
}

ASquadUnit* FCoverTrenchTestScenario::FindSoldierInTrenchCover(UWorld& World) const
{
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		const bool bIsSettledInTrench = IsValid(SquadUnit) && SquadUnit->IsUnitAlive() &&
			SquadUnit->GetOwningPlayer() == CoverTrenchTestScenarioPrivate::PlayerOwnedTeam &&
			CoverTrenchTestScenarioPrivate::GetIsInTrenchCover(*SquadUnit) &&
			SquadUnit->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected;
		if (bIsSettledInTrench)
		{
			return SquadUnit;
		}
	}
	return nullptr;
}

void FCoverTrenchTestScenario::TickWalkingToTrench(UWorld& World)
{
	using namespace CoverTrenchTestScenarioPrivate;
	ASquadUnit* TrenchSoldier = FindSoldierInTrenchCover(World);
	if (not IsValid(TrenchSoldier) && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(IsValid(TrenchSoldier), TEXT("a soldier took cover in the trench"));
	if (not IsValid(TrenchSoldier))
	{
		Finish(World);
		return;
	}
	M_TrenchSoldier = TrenchSoldier;
	M_TrenchPoint = TrenchSoldier->GetCoverRuntimeState().AssignedCoverPoint;
	Check(
		GetIdlePose(*TrenchSoldier) == ESquadIdleAnimationPose::TrenchCover,
		TEXT("with nothing in sight the soldier crouches below the edge"),
		UEnum::GetValueAsString(GetIdlePose(*TrenchSoldier)));

	if (not PlaceLivingEnemyInFrontOfTrench(World))
	{
		Check(false, TEXT("an enemy is alive to stand up against"));
		Finish(World);
		return;
	}
	EnterPhase(World, ECoverTrenchTestPhase::WaitingForStandUp, StandUpTimeoutSeconds);
}

bool FCoverTrenchTestScenario::PlaceLivingEnemyInFrontOfTrench(UWorld& World)
{
	using namespace CoverTrenchTestScenarioPrivate;
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
	// Straight ahead of the firing step, where the soldier looks.
	const FVector FacingDirection = -M_TrenchPoint.CoverNormal.GetSafeNormal2D();
	const FVector EnemyLocation = M_TrenchPoint.Location + FacingDirection * EnemyDistanceInFrontOfTrench;
	Enemy->SetActorLocation(
		FVector(EnemyLocation.X, EnemyLocation.Y, M_EnemyStartLocation.Z),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	return true;
}

void FCoverTrenchTestScenario::TickWaitingForStandUp(
	UWorld& World,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace CoverTrenchTestScenarioPrivate;
	const ASquadUnit* TrenchSoldier = M_TrenchSoldier.Get();
	const bool bHasStoodUp = IsValid(TrenchSoldier) && GetIsInTrenchCover(*TrenchSoldier) &&
		TrenchSoldier->GetCoverRuntimeState().State == ESquadUnitCoverState::Exposed;
	if (not bHasStoodUp && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(bHasStoodUp, TEXT("the soldier exposed himself when an enemy appeared in front of the trench"));
	if (not bHasStoodUp)
	{
		Finish(World);
		return;
	}
	Check(
		GetIdlePose(*TrenchSoldier) == ESquadIdleAnimationPose::TrenchPeek,
		TEXT("exposed in a trench means standing up"),
		UEnum::GetValueAsString(GetIdlePose(*TrenchSoldier)));
	const float StandUpTravel = FVector::Dist2D(TrenchSoldier->GetActorLocation(), M_TrenchPoint.Location);
	Check(
		StandUpTravel <= MaximumStandUpTravel,
		TEXT("standing up did not move the soldier"),
		FString::Printf(TEXT("travel_cm=%.1f"), StandUpTravel));
	Check(
		not TrenchSoldier->GetWouldShootOwnCover(CoverSubsystem),
		TEXT("the soldier's weapon fires through his own trench"));

	// The enemy stays in front: the soldier still has a reason to stand, so only the reload can make him duck.
	if (ASquadUnit* StandingSoldier = M_TrenchSoldier.Get())
	{
		StandingSoldier->OnWeaponReloadStarted(TestReloadSeconds);
	}
	EnterPhase(World, ECoverTrenchTestPhase::WaitingForReloadDuck, ReloadDuckTimeoutSeconds);
}

void FCoverTrenchTestScenario::TickWaitingForReloadDuck(UWorld& World)
{
	using namespace CoverTrenchTestScenarioPrivate;
	const ASquadUnit* TrenchSoldier = M_TrenchSoldier.Get();
	const bool bHasDucked = IsValid(TrenchSoldier) && GetIsInTrenchCover(*TrenchSoldier) &&
		TrenchSoldier->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected &&
		GetIdlePose(*TrenchSoldier) == ESquadIdleAnimationPose::TrenchCover &&
		TrenchSoldier->GetIsReloadingInCover();
	if (not bHasDucked && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(bHasDucked, TEXT("the soldier ducked behind his cover when his weapon started to reload"));
	if (not bHasDucked)
	{
		Finish(World);
		return;
	}
	EnterPhase(World, ECoverTrenchTestPhase::WaitingForStandUpAfterReload, StandUpAfterReloadTimeoutSeconds);
}

void FCoverTrenchTestScenario::TickWaitingForStandUpAfterReload(UWorld& World)
{
	using namespace CoverTrenchTestScenarioPrivate;
	const ASquadUnit* TrenchSoldier = M_TrenchSoldier.Get();
	const bool bIsInTrench = IsValid(TrenchSoldier) && GetIsInTrenchCover(*TrenchSoldier);
	const bool bIsExposed = bIsInTrench &&
		TrenchSoldier->GetCoverRuntimeState().State == ESquadUnitCoverState::Exposed;
	if (bIsExposed && TrenchSoldier->GetIsReloadingInCover())
	{
		Check(false, TEXT("the soldier stays down for the whole reload"));
		Finish(World);
		return;
	}
	// The first enemy rarely survives the squad's fire this long; a fresh one gives the soldier a reason to stand.
	const bool bReloadIsOver = bIsInTrench && not TrenchSoldier->GetIsReloadingInCover();
	if (bReloadIsOver && not bM_HasBroughtEnemyAfterReload)
	{
		bM_HasBroughtEnemyAfterReload = true;
		const bool bHasEnemy = PlaceLivingEnemyInFrontOfTrench(World);
		Check(bHasEnemy, TEXT("an enemy is alive to stand up against after the reload"));
		EnterPhase(World, ECoverTrenchTestPhase::WaitingForStandUpAfterReload, StandUpTimeoutSeconds);
		return;
	}
	if (not bIsExposed && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(
		bIsExposed,
		TEXT("the soldier stood up again once the reload was over"),
		IsValid(TrenchSoldier) ? UEnum::GetValueAsString(TrenchSoldier->GetCoverRuntimeState().State) : FString());
	if (ASquadUnit* Enemy = M_Enemy.Get())
	{
		Enemy->SetActorLocation(M_EnemyStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	EnterPhase(World, ECoverTrenchTestPhase::WaitingForCrouchAgain, CrouchAgainTimeoutSeconds);
}

void FCoverTrenchTestScenario::TickWaitingForCrouchAgain(UWorld& World)
{
	using namespace CoverTrenchTestScenarioPrivate;
	const ASquadUnit* TrenchSoldier = M_TrenchSoldier.Get();
	const bool bIsCrouchedAgain = IsValid(TrenchSoldier) && GetIsInTrenchCover(*TrenchSoldier) &&
		TrenchSoldier->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected &&
		GetIdlePose(*TrenchSoldier) == ESquadIdleAnimationPose::TrenchCover;
	if (not bIsCrouchedAgain && not GetHasPhaseTimedOut(World))
	{
		return;
	}
	Check(bIsCrouchedAgain, TEXT("the soldier crouched again once the enemy was gone"));
	Finish(World);
}
