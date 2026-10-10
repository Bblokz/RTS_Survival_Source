#include "CoverCombatTestScenario.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverCombatTest, Log, All);

namespace CoverCombatTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr int32 EnemyDirectionCount = 12;
	constexpr float EnemyDistance = 1500.0f;
	constexpr float FullCircleDegrees = 360.0f;
	constexpr float ScoreTolerance = 0.001f;
	constexpr float WholeMapRadius = 1000000.0f;
}

void FCoverCombatTestScenario::Start()
{
	bM_IsWaitingForCoverScan = true;
	UE_LOG(LogRTSCoverCombatTest, Display, TEXT("RTS_COVER_COMBAT_TEST scheduled"));
}

void FCoverCombatTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (not bM_IsWaitingForCoverScan || not IsValid(World))
	{
		return;
	}
	// The map boots paused behind the start-game widget; the scan itself also runs while paused.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (not CoverSubsystem.GetHasCompletedFullScan())
	{
		return;
	}
	bM_IsWaitingForCoverScan = false;
	RunChecks(*World, CoverSubsystem);
}

void FCoverCombatTestScenario::Check(
	const bool bCondition,
	const TCHAR* Description,
	FCoverCombatTestTotals& InOutTotals) const
{
	if (bCondition)
	{
		return;
	}
	++InOutTotals.FailedChecks;
	UE_LOG(LogRTSCoverCombatTest, Display, TEXT("RTS_COVER_COMBAT_TEST check=FAIL %s"), Description);
}

bool FCoverCombatTestScenario::FindTestUnits(UWorld& World, ASquadUnit*& OutSoldier, ASquadUnit*& OutEnemy) const
{
	OutSoldier = nullptr;
	OutEnemy = nullptr;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
		{
			continue;
		}
		const bool bIsPlayerUnit = SquadUnit->GetOwningPlayer() == CoverCombatTestScenarioPrivate::PlayerOwnedTeam;
		ASquadUnit*& Slot = bIsPlayerUnit ? OutSoldier : OutEnemy;
		Slot = IsValid(Slot) ? Slot : SquadUnit;
	}
	return IsValid(OutSoldier) && IsValid(OutEnemy);
}

bool FCoverCombatTestScenario::FindDensestFreeCoverLocation(
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const ASquadUnit& Soldier,
	FVector& OutLocation) const
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const float SearchRadius = IsValid(CoverSettings) ? CoverSubsystem.GetAutomaticCoverSearchRadius(Soldier) : 0.0f;
	const auto GetIsFree = [&CoverSubsystem, &Soldier](const FRTSCoverPoint& CoverPoint)
	{
		const ASquadUnit* ReservingUnit = CoverSubsystem.GetCoverReservationOwner(CoverPoint.PointId);
		return not IsValid(ReservingUnit) || ReservingUnit == &Soldier;
	};
	int32 MostFreeNeighbours = 0;
	const TArray<FRTSCoverPoint> AllPoints = CoverSubsystem.FindCoverPointsInRadius(
		Soldier.GetActorLocation(),
		CoverCombatTestScenarioPrivate::WholeMapRadius);
	for (const FRTSCoverPoint& CentrePoint : AllPoints)
	{
		if (not GetIsFree(CentrePoint))
		{
			continue;
		}
		int32 FreeNeighbours = 0;
		for (const FRTSCoverPoint& Neighbour : CoverSubsystem.FindCoverPointsInRadius(CentrePoint.Location, SearchRadius))
		{
			FreeNeighbours += GetIsFree(Neighbour) ? 1 : 0;
		}
		if (FreeNeighbours > MostFreeNeighbours)
		{
			MostFreeNeighbours = FreeNeighbours;
			OutLocation = CentrePoint.Location;
		}
	}
	UE_LOG(
		LogRTSCoverCombatTest,
		Display,
		TEXT("RTS_COVER_COMBAT_TEST published_points=%d free_points_around_test_location=%d"),
		AllPoints.Num(),
		MostFreeNeighbours);
	return MostFreeNeighbours > 0;
}

bool FCoverCombatTestScenario::GetIsBestUsablePoint(
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const ASquadUnit& Soldier,
	const ASquadUnit& Enemy,
	const FRTSCombatCoverThreats& Threats,
	const FRTSCoverPoint& ChosenPoint) const
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const FRTSCombatCoverSettings Settings = CoverSubsystem.BuildCombatCoverSettings();
	const FVector SoldierLocation = Soldier.GetActorLocation();
	float ChosenScore = 0.0f;
	if (not IsValid(CoverSettings) || not FRTSCombatCoverScoring::TryScoreCoverPoint(
		ChosenPoint, SoldierLocation, Threats, Settings, ChosenScore))
	{
		return false;
	}
	const TArray<FRTSCoverPoint> NearbyPoints = CoverSubsystem.FindCoverPointsInRadius(
		SoldierLocation,
		CoverSubsystem.GetAutomaticCoverSearchRadius(Soldier));
	for (const FRTSCoverPoint& CoverPoint : NearbyPoints)
	{
		const ASquadUnit* ReservingUnit = CoverSubsystem.GetCoverReservationOwner(CoverPoint.PointId);
		float CandidateScore = 0.0f;
		if ((IsValid(ReservingUnit) && ReservingUnit != &Soldier) || not FRTSCombatCoverScoring::TryScoreCoverPoint(
			CoverPoint, SoldierLocation, Threats, Settings, CandidateScore) ||
			CandidateScore <= ChosenScore + CoverCombatTestScenarioPrivate::ScoreTolerance)
		{
			continue;
		}
		if (CoverSubsystem.GetHasTargetSpecificFiringLane(Soldier, CoverPoint, Enemy, Threats.PrimaryTargetLocation))
		{
			return false;
		}
	}
	return true;
}

FRTSCombatCoverThreats FCoverCombatTestScenario::PlaceEnemy(
	const ASquadUnit& Soldier,
	ASquadUnit& Enemy,
	const FVector& Direction) const
{
	const FVector EnemyLocation = Soldier.GetActorLocation() +
		Direction * CoverCombatTestScenarioPrivate::EnemyDistance;
	Enemy.SetActorLocation(EnemyLocation, false, nullptr, ETeleportType::TeleportPhysics);
	FRTSCombatCoverThreats Threats;
	Threats.PrimaryTargetLocation = EnemyLocation;
	Threats.ThreatLocations.Add(EnemyLocation);
	return Threats;
}

void FCoverCombatTestScenario::CheckEnemyDirection(
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	ASquadUnit& Soldier,
	ASquadUnit& Enemy,
	const FVector& EnemyDirection,
	FCoverCombatTestTotals& InOutTotals) const
{
	const FRTSCombatCoverSettings Settings = CoverSubsystem.BuildCombatCoverSettings();
	const FVector SoldierLocation = Soldier.GetActorLocation();
	++InOutTotals.DirectionsTested;
	const FRTSCombatCoverThreats Threats = PlaceEnemy(Soldier, Enemy, EnemyDirection);
	FRTSCoverPoint ChosenPoint;
	if (not CoverSubsystem.TryReserveCombatCoverPoint(Soldier, Enemy, Threats, 0.0f, nullptr, ChosenPoint))
	{
		return;
	}
	++InOutTotals.DirectionsWithCover;
	CoverSubsystem.ReleaseCoverReservation(Soldier, ChosenPoint.PointId);
	Check(
		FRTSCombatCoverScoring::GetCanAimAt(ChosenPoint, Threats.PrimaryTargetLocation, Settings),
		TEXT("the chosen point can aim at the enemy"),
		InOutTotals);
	Check(
		GetIsBestUsablePoint(CoverSubsystem, Soldier, Enemy, Threats, ChosenPoint),
		TEXT("no better-scoring free point with an open firing lane was passed over"),
		InOutTotals);
	const bool bFacesEnemy = FRTSCombatCoverScoring::GetProtectedThreatFraction(ChosenPoint, Threats, Settings) > 0.0f;
	InOutTotals.ChosenPointsFacingEnemy += bFacesEnemy ? 1 : 0;

	FRTSCoverPoint UnusedPoint;
	const bool bLeftWhileEnemyStayed = CoverSubsystem.TryReserveCombatCoverPoint(
		Soldier, Enemy, Threats, 0.0f, &ChosenPoint, UnusedPoint);
	Check(not bLeftWhileEnemyStayed, TEXT("the soldier keeps its point while the enemy stays put"), InOutTotals);
	if (bLeftWhileEnemyStayed)
	{
		CoverSubsystem.ReleaseCoverReservation(Soldier, UnusedPoint.PointId);
	}

	const FRTSCombatCoverThreats FlankThreats = PlaceEnemy(Soldier, Enemy, -EnemyDirection);
	FRTSCoverPoint FlankPoint;
	if (not CoverSubsystem.TryReserveCombatCoverPoint(Soldier, Enemy, FlankThreats, 0.0f, &ChosenPoint, FlankPoint))
	{
		return;
	}
	++InOutTotals.RepositionsWhenFlanked;
	CoverSubsystem.ReleaseCoverReservation(Soldier, FlankPoint.PointId);
	float OccupiedFlankedScore = 0.0f;
	float FlankPointScore = 0.0f;
	const bool bOccupiedStillUsable = FRTSCombatCoverScoring::TryScoreCoverPoint(
		ChosenPoint, SoldierLocation, FlankThreats, Settings, OccupiedFlankedScore);
	Check(
		FRTSCombatCoverScoring::TryScoreCoverPoint(FlankPoint, SoldierLocation, FlankThreats, Settings, FlankPointScore),
		TEXT("the point taken after a flank can aim at the enemy"),
		InOutTotals);
	Check(
		not bOccupiedStillUsable || FlankPointScore + CoverCombatTestScenarioPrivate::ScoreTolerance >=
		OccupiedFlankedScore + Settings.MinimumScoreGain,
		TEXT("a usable point is only given up for a clearly better one"),
		InOutTotals);
}

void FCoverCombatTestScenario::RunChecks(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem) const
{
	using namespace CoverCombatTestScenarioPrivate;
	FCoverCombatTestTotals Totals;
	ASquadUnit* Soldier = nullptr;
	ASquadUnit* Enemy = nullptr;
	FVector TestLocation = FVector::ZeroVector;
	if (not FindTestUnits(World, Soldier, Enemy) ||
		not FindDensestFreeCoverLocation(CoverSubsystem, *Soldier, TestLocation))
	{
		UE_LOG(LogRTSCoverCombatTest, Display, TEXT("RTS_COVER_COMBAT_TEST RESULT FAIL no soldier, enemy and cover to test with"));
		return;
	}
	const FVector SoldierStartLocation = Soldier->GetActorLocation();
	const FVector EnemyStartLocation = Enemy->GetActorLocation();
	// Cover points lie on the navmesh at the soldier's feet; the soldier keeps its own height above the ground.
	Soldier->SetActorLocation(
		FVector(TestLocation.X, TestLocation.Y, SoldierStartLocation.Z),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	for (int32 DirectionIndex = 0; DirectionIndex < EnemyDirectionCount; ++DirectionIndex)
	{
		const float YawDegrees = static_cast<float>(DirectionIndex) * FullCircleDegrees /
			static_cast<float>(EnemyDirectionCount);
		CheckEnemyDirection(
			CoverSubsystem,
			*Soldier,
			*Enemy,
			FVector::ForwardVector.RotateAngleAxis(YawDegrees, FVector::UpVector),
			Totals);
	}
	Enemy->SetActorLocation(EnemyStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Soldier->SetActorLocation(SoldierStartLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Check(Totals.DirectionsWithCover > 0, TEXT("cover is found against at least one enemy bearing"), Totals);
	Check(Totals.RepositionsWhenFlanked > 0, TEXT("a flanked soldier is given a better point at least once"), Totals);
	UE_LOG(
		LogRTSCoverCombatTest,
		Display,
		TEXT("RTS_COVER_COMBAT_TEST directions=%d with_cover=%d facing_enemy=%d repositions_when_flanked=%d"),
		Totals.DirectionsTested,
		Totals.DirectionsWithCover,
		Totals.ChosenPointsFacingEnemy,
		Totals.RepositionsWhenFlanked);
	UE_LOG(
		LogRTSCoverCombatTest,
		Display,
		TEXT("RTS_COVER_COMBAT_TEST RESULT %s failed_checks=%d map=%s"),
		Totals.FailedChecks == 0 ? TEXT("PASS") : TEXT("FAIL"),
		Totals.FailedChecks,
		*World.GetName());
}
