// Copyright (C) Bas Blokzijl - All rights reserved.

// Regression tests for the squad advance fixes in Docs/Investigations/SquadAdvanceInvestigation.md: cover keeps the
// weapon's engagement distance, in-range soldiers keep firing while the squad closes range, and attacks started by
// the aggressive stance keep the cover grace time.
// Even the includes, friend access, settings overrides and test registration are absent from Shipping builds.
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Templates/UnrealTemplate.h"
#include "NavigationPath.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSTargetAcquisition.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AISquadUnit/AISquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponAIState/WeaponAIState.h"
#include "UObject/StrongObjectPtr.h"

/** @brief Supplies controlled inputs to production decisions without adding gameplay hooks or reflected types. */
struct FSquadAdvanceDiagnosticAccess
{
	static void PublishCover(URTSCoverFinderWorldSubsystem& Subsystem, const TArray<FRTSCoverPoint>& Points)
	{
		Subsystem.M_CoverReservations.Reset();
		Subsystem.M_CoverPoints = Points;
		Subsystem.RebuildCoverSpatialGrid();
	}

	static FRTSCombatCoverSettings GetCombatSettings(const URTSCoverFinderWorldSubsystem& Subsystem)
	{
		return Subsystem.BuildCombatCoverSettings();
	}

	static void SetOwningPlayer(ASquadUnit& Soldier, const int32 OwningPlayer)
	{
		URTSComponent* RTSComponent = Soldier.GetRTSComponent();
		if (not IsValid(RTSComponent))
		{
			RTSComponent = NewObject<URTSComponent>(&Soldier);
			Soldier.AddInstanceComponent(RTSComponent);
			Soldier.RTSComponent = RTSComponent;
		}
		RTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(OwningPlayer));
	}

	static void SetCoverState(ASquadUnit& Soldier, const ESquadUnitCoverState State)
	{
		Soldier.M_CoverRuntimeState.State = State;
	}

	static void SetCombatState(ASquadUnit& Soldier, const EAbilityID Command, AActor* Target)
	{
		Soldier.M_ActiveCommand = Command;
		Soldier.M_TargetActor = Target;
	}

	static void SetupWeapon(ASquadUnit& Soldier, AInfantryWeaponMaster& Weapon, AActor& Target, const float Range)
	{
		Soldier.M_InfantryWeapon = &Weapon;
		Weapon.M_OwningSquadUnit = &Soldier;
		Weapon.M_WeaponRangeData.ForceSetRange(Range);
		Weapon.SetOwningPlayer(Soldier.GetOwningPlayer());
		Weapon.M_TargetingData.SetTargetActor(&Target);
		// Already engaging this target: an attack command on it needs no weapon state or timers.
		Weapon.M_WeaponAIState = EWeaponAIState::SpecificEngage;
	}

	static void SetWeaponGroundTarget(AInfantryWeaponMaster& Weapon, const FVector& GroundLocation)
	{
		Weapon.M_TargetingData.SetTargetGround(GroundLocation);
	}

	static void SetRangeClosingState(ASquadUnit& Soldier, AAISquadUnit& Controller, const EAbilityID CombatAbility)
	{
		Soldier.M_AISquadUnit = &Controller;
		Soldier.M_ActiveCommand = EAbilityID::IdNoAbility_MoveCloserToTarget;
		Soldier.M_CombatAbilityBeforeRangeClosing = CombatAbility;
		Soldier.M_CurrentPath = MakeShared<FNavigationPath, ESPMode::ThreadSafe>();
	}

	static bool GetHasCommandPath(const ASquadUnit& Soldier)
	{
		return Soldier.M_CurrentPath.IsValid();
	}

	static void ExecuteAttack(ASquadUnit& Soldier, AActor& Target, const bool bIsAutonomousOrder)
	{
		Soldier.ExecuteAttackCommand(&Target, bIsAutonomousOrder);
	}

	static AActor* GetCoverJudgementTarget(const ASquadUnit& Soldier, bool& bOutIsAttackOrderTarget)
	{
		FVector TargetLocation = FVector::ZeroVector;
		return Soldier.GetCoverJudgementTarget(TargetLocation, bOutIsAttackOrderTarget);
	}

	static void SetStance(URTSTargetAcquisition& Acquisition, const ERTSAggroBehaviour Stance)
	{
		// Bypass timer startup: this test checks the decision, not asynchronous target discovery.
		Acquisition.EngagementStance = Stance;
	}

	static bool GetCanAggro(const URTSTargetAcquisition& Acquisition)
	{
		return Acquisition.CanAggroEnemies();
	}
};

namespace SquadAdvanceDiagnosticTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr int32 EnemyOwnedTeam = 2;
	constexpr float PlayerSearchRadius = 1200.0f;
	constexpr float EnemySearchRadius = 2500.0f;
	constexpr float RepositionRadius = 900.0f;
	constexpr float ProtectionWeight = 1.0f;
	constexpr float FacingWeight = 0.5f;
	constexpr float TravelWeight = 0.4f;
	constexpr float MinimumScoreGain = 0.3f;
	constexpr float MaximumAimYaw = 90.0f;
	constexpr float ProtectedHalfAngle = 60.0f;
	constexpr float MinimumEngagementRangeRatio = 0.35f;
	constexpr float MaximumAdvanceDistance = 600.0f;
	constexpr float WeaponRange = 3000.0f;
	constexpr float ShortWeaponRange = 800.0f;
	constexpr float PointBlankDistance = 200.0f;

	/** @brief Restores every overridden setting on all exits so tests cannot leave the editor's tuning changed. */
	struct FScopedCoverSettings
	{
		TGuardValue<float> PlayerRadius;
		TGuardValue<float> EnemyRadius;
		TGuardValue<float> RepositionDistance;
		TGuardValue<float> Protection;
		TGuardValue<float> Facing;
		TGuardValue<float> Travel;
		TGuardValue<float> ScoreGain;
		TGuardValue<float> AimYaw;
		TGuardValue<float> ProtectedAngle;
		TGuardValue<float> EngagementRangeRatio;
		TGuardValue<float> AdvanceDistance;

		explicit FScopedCoverSettings(URTSCoverFinderDeveloperSettings& Settings)
			: PlayerRadius(Settings.M_AutomaticCoverSearchRadius, PlayerSearchRadius),
			  EnemyRadius(Settings.M_EnemyAutomaticCoverSearchRadius, EnemySearchRadius),
			  RepositionDistance(Settings.M_CombatCoverRepositionRadius, RepositionRadius),
			  Protection(Settings.M_CombatCoverProtectionWeight, ProtectionWeight),
			  Facing(Settings.M_CombatCoverFacingWeight, FacingWeight),
			  Travel(Settings.M_CombatCoverTravelWeight, TravelWeight),
			  ScoreGain(Settings.M_CombatCoverMinimumScoreGain, MinimumScoreGain),
			  AimYaw(Settings.M_CombatCoverMaximumAimYawDegrees, MaximumAimYaw),
			  ProtectedAngle(Settings.M_CombatCoverProtectedHalfAngleDegrees, ProtectedHalfAngle),
			  EngagementRangeRatio(Settings.M_CombatCoverMinimumEngagementRangeRatio, MinimumEngagementRangeRatio),
			  AdvanceDistance(Settings.M_CombatCoverMaximumAdvanceDistance, MaximumAdvanceDistance)
		{
		}
	};

	/** @brief Isolates combat decisions and real collision traces from maps, Blueprint initialization and combat damage. */
	class FDecisionWorld
	{
	public:
		FDecisionWorld() : M_World(UWorld::CreateWorld(EWorldType::EditorPreview, false))
		{
		}

		~FDecisionWorld()
		{
			UWorld* World = GetWorld();
			if (not IsValid(World))
			{
				return;
			}
			World->DestroyWorld(false);
		}

		UWorld* GetWorld() const
		{
			return M_World.Get();
		}

		template <typename TActor>
		TActor* Spawn(const FVector& Location = FVector::ZeroVector) const
		{
			UWorld* World = GetWorld();
			if (not IsValid(World))
			{
				return nullptr;
			}
			// Native actors need no Blueprint lifecycle, timers, anim instances, managers or squad spawning here.
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.bDeferConstruction = true;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<TActor>(Location, FRotator::ZeroRotator, SpawnParameters);
		}

		AActor* SpawnTarget(const FVector& Location) const
		{
			AActor* Target = Spawn<AActor>();
			if (not IsValid(Target))
			{
				return nullptr;
			}
			constexpr float TargetHalfWidth = 80.0f;
			constexpr float TargetHalfHeight = 300.0f;
			UBoxComponent* TargetBox = NewObject<UBoxComponent>(Target);
			Target->AddInstanceComponent(TargetBox);
			Target->SetRootComponent(TargetBox);
			TargetBox->SetBoxExtent(FVector(TargetHalfWidth, TargetHalfWidth, TargetHalfHeight));
			TargetBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			TargetBox->SetCollisionResponseToAllChannels(ECR_Block);
			TargetBox->RegisterComponent();
			Target->SetActorLocation(Location);
			Target->Tags.Add(DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision);
			return Target;
		}

	private:
		TStrongObjectPtr<UWorld> M_World;
	};

	FRTSCoverPoint MakePoint(const int64 PointId, const FVector& Location, const float FacingYaw = 0.0f)
	{
		FRTSCoverPoint Point;
		Point.PointId = PointId;
		Point.Location = Location;
		Point.CoverNormal = -FVector::ForwardVector.RotateAngleAxis(FacingYaw, FVector::UpVector);
		return Point;
	}

	FRTSCombatCoverThreats MakeThreats(const FVector& TargetLocation)
	{
		FRTSCombatCoverThreats Threats;
		Threats.PrimaryTargetLocation = TargetLocation;
		Threats.ThreatLocations.Add(TargetLocation);
		return Threats;
	}

	FRTSCombatCoverSettings MakeLimitSettings()
	{
		FRTSCombatCoverSettings Settings;
		Settings.MinimumEngagementRangeRatio = MinimumEngagementRangeRatio;
		Settings.MaximumAdvanceTowardTarget = MaximumAdvanceDistance;
		return Settings;
	}

	/** @return Whether a point PointDistance from a target at the origin passes for a soldier SoldierDistance away. */
	bool GetIsAllowedOnLine(const float SoldierDistance, const float PointDistance, const float Range)
	{
		const FRTSCombatCoverDistanceLimits Limits = FRTSCombatCoverScoring::BuildDistanceLimits(Range, MakeLimitSettings());
		return FRTSCombatCoverScoring::GetIsWithinDistanceLimits(
			MakePoint(1, FVector(PointDistance, 0.0f, 0.0f)),
			FVector(SoldierDistance, 0.0f, 0.0f),
			FVector::ZeroVector,
			Limits);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceInitialCoverTest,
	"RTS.Squads.AdvanceDiagnostics.EnemyInitialCoverKeepsEngagementDistance",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceInitialCoverTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	FScopedCoverSettings Settings(*GetMutableDefault<URTSCoverFinderDeveloperSettings>());
	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	constexpr float TargetDistance = 2600.0f;
	constexpr float LateralDistance = 400.0f;
	constexpr float SidewaysYaw = 70.0f;
	constexpr float NoWeaponLimits = 0.0f;
	AActor* Target = Fixture.SpawnTarget(FVector(TargetDistance, 0.0f, 0.0f));
	if (not TestNotNull(TEXT("Isolated soldier exists"), Soldier) || not TestNotNull(TEXT("Traceable target exists"), Target))
	{
		return false;
	}
	URTSCoverFinderWorldSubsystem* Subsystem = NewObject<URTSCoverFinderWorldSubsystem>(Fixture.GetWorld());
	const FRTSCoverPoint ForwardPoint = MakePoint(1, FVector(TargetDistance - PointBlankDistance, 0.0f, 0.0f));
	const FRTSCoverPoint NearbyPoint = MakePoint(2, FVector(0.0f, -LateralDistance, 0.0f), SidewaysYaw);
	FSquadAdvanceDiagnosticAccess::PublishCover(*Subsystem, {ForwardPoint, NearbyPoint});
	const FRTSCombatCoverThreats Threats = MakeThreats(Target->GetActorLocation());
	FRTSCoverPoint ChosenPoint;
	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, PlayerOwnedTeam);
	TestTrue(TEXT("Player finds lateral cover with a real target collision trace"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, nullptr, ChosenPoint));
	TestEqual(TEXT("The player takes the nearby cover"), ChosenPoint.PointId, NearbyPoint.PointId);
	Subsystem->ReleaseCoverReservation(*Soldier, ChosenPoint.PointId);

	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, EnemyOwnedTeam);
	TestTrue(TEXT("Control: without weapon limits the enemy still finds cover"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, NoWeaponLimits, nullptr, ChosenPoint));
	TestEqual(TEXT("Control: on score alone the 24 m forward walk still beats sideways cover"),
		ChosenPoint.PointId, ForwardPoint.PointId);
	Subsystem->ReleaseCoverReservation(*Soldier, ChosenPoint.PointId);
	TestTrue(TEXT("Enemy finds cover with the identical geometry"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, nullptr, ChosenPoint));
	TestEqual(TEXT("The rifle's engagement distance rules out the point 2 m from the target"),
		ChosenPoint.PointId, NearbyPoint.PointId);
	Subsystem->ReleaseCoverReservation(*Soldier, ChosenPoint.PointId);

	FRTSCoverPoint GoodNearbyPoint = NearbyPoint;
	GoodNearbyPoint.CoverNormal = (GoodNearbyPoint.Location - Target->GetActorLocation()).GetSafeNormal2D();
	FSquadAdvanceDiagnosticAccess::PublishCover(*Subsystem, {ForwardPoint, GoodNearbyPoint});
	TestTrue(TEXT("Control: well-facing nearby cover remains usable"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, nullptr, ChosenPoint));
	TestEqual(TEXT("Control: equally protective nearby cover wins"), ChosenPoint.PointId, GoodNearbyPoint.PointId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceCoverRepositionTest,
	"RTS.Squads.AdvanceDiagnostics.BetterAngleRepositionKeepsEngagementDistance",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceCoverRepositionTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	FScopedCoverSettings Settings(*GetMutableDefault<URTSCoverFinderDeveloperSettings>());
	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	constexpr float TargetDistance = 1000.0f;
	constexpr float FlankedCoverYaw = 75.0f;
	constexpr float SidestepDistance = 300.0f;
	AActor* Target = Fixture.SpawnTarget(FVector(TargetDistance, 0.0f, 0.0f));
	if (not TestNotNull(TEXT("Isolated soldier exists"), Soldier) || not TestNotNull(TEXT("Traceable target exists"), Target))
	{
		return false;
	}
	URTSCoverFinderWorldSubsystem* Subsystem = NewObject<URTSCoverFinderWorldSubsystem>(Fixture.GetWorld());
	const FRTSCoverPoint OccupiedPoint = MakePoint(1, FVector::ZeroVector, FlankedCoverYaw);
	const FRTSCoverPoint ForwardPoint = MakePoint(2, FVector(TargetDistance - PointBlankDistance, 0.0f, 0.0f));
	FSquadAdvanceDiagnosticAccess::PublishCover(*Subsystem, {OccupiedPoint, ForwardPoint});
	const FRTSCombatCoverThreats Threats = MakeThreats(Target->GetActorLocation());
	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, EnemyOwnedTeam);
	const FRTSCombatCoverSettings CombatSettings = FSquadAdvanceDiagnosticAccess::GetCombatSettings(*Subsystem);
	float CurrentScore = 0.0f;
	float ForwardScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(OccupiedPoint, Soldier->GetActorLocation(), Threats, CombatSettings, CurrentScore);
	FRTSCombatCoverScoring::TryScoreCoverPoint(ForwardPoint, Soldier->GetActorLocation(), Threats, CombatSettings, ForwardScore);
	TestTrue(TEXT("Control: on score alone the forward point would be worth the walk"),
		FRTSCombatCoverScoring::GetIsWorthRepositioning(CurrentScore, ForwardScore, CombatSettings));
	FRTSCoverPoint ChosenPoint;
	TestFalse(TEXT("The occupant does not walk to the point 2 m from the target"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, &OccupiedPoint, ChosenPoint));

	// Squarely faces the target from beside the soldier: no closer to the enemy, so it is still a valid move.
	const FVector SidestepLocation(0.0f, SidestepDistance, 0.0f);
	FRTSCoverPoint SidestepPoint = MakePoint(3, SidestepLocation);
	SidestepPoint.CoverNormal = (SidestepLocation - Target->GetActorLocation()).GetSafeNormal2D();
	FSquadAdvanceDiagnosticAccess::PublishCover(*Subsystem, {OccupiedPoint, ForwardPoint, SidestepPoint});
	TestTrue(TEXT("A flanked occupant still repositions to better cover beside it"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, &OccupiedPoint, ChosenPoint));
	TestEqual(TEXT("It takes the sidestep, not the forward point"), ChosenPoint.PointId, SidestepPoint.PointId);
	AddInfo(FString::Printf(TEXT("Forward point refused despite scores %.3f -> %.3f."), CurrentScore, ForwardScore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceDistanceLimitsTest,
	"RTS.Squads.AdvanceDiagnostics.EngagementDistanceFollowsWeaponRange",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceDistanceLimitsTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	const FRTSCombatCoverDistanceLimits RifleLimits = FRTSCombatCoverScoring::BuildDistanceLimits(
		WeaponRange, MakeLimitSettings());
	TestEqual(TEXT("A rifle keeps 35% of its range"), RifleLimits.MinimumDistanceToTarget, 1050.0f);
	TestEqual(TEXT("Cover stays inside 90% of weapon range"), RifleLimits.MaximumDistanceToTarget, 2700.0f);
	TestTrue(TEXT("A rifleman may step 3 m forward to cover 12 m from the target"), GetIsAllowedOnLine(1500.0f, 1200.0f, WeaponRange));
	TestFalse(TEXT("A rifleman does not walk to 2 m from the target"), GetIsAllowedOnLine(1500.0f, PointBlankDistance, WeaponRange));
	TestFalse(TEXT("Nor 9 m forward, even to a point at a good distance"), GetIsAllowedOnLine(2600.0f, 1700.0f, WeaponRange));
	TestTrue(TEXT("A short-ranged weapon may close to 3 m"), GetIsAllowedOnLine(700.0f, 300.0f, ShortWeaponRange));
	TestFalse(TEXT("The same 3 m point is refused for a rifle"), GetIsAllowedOnLine(700.0f, 300.0f, WeaponRange));
	TestTrue(TEXT("An enemy that walked up still leaves cover no closer than the soldier"),
		GetIsAllowedOnLine(400.0f, 450.0f, WeaponRange));
	TestFalse(TEXT("But not cover closer to that enemy"), GetIsAllowedOnLine(400.0f, 300.0f, WeaponRange));
	TestTrue(TEXT("A soldier beyond the maximum may close that gap as well"), GetIsAllowedOnLine(2900.0f, 2200.0f, WeaponRange));
	TestFalse(TEXT("Only that gap on top of the advance limit"), GetIsAllowedOnLine(2900.0f, 2000.0f, WeaponRange));
	TestTrue(TEXT("Without a weapon range nothing is limited"), GetIsAllowedOnLine(2600.0f, PointBlankDistance, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceCoverDistanceTest,
	"RTS.Squads.AdvanceDiagnostics.CoverScoreChargesTheWholeWalk",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceCoverDistanceTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	const FRTSCombatCoverSettings Settings;
	const FRTSCoverPoint Point = MakePoint(1, FVector::ZeroVector);
	float FarTargetScore = 0.0f;
	float CloseTargetScore = 0.0f;
	TestTrue(TEXT("Cover can aim at a distant enemy"), FRTSCombatCoverScoring::TryScoreCoverPoint(
		Point, Point.Location, MakeThreats(FVector(WeaponRange, 0.0f, 0.0f)), Settings, FarTargetScore));
	TestTrue(TEXT("Cover can aim at an enemy 2 m away"), FRTSCombatCoverScoring::TryScoreCoverPoint(
		Point, Point.Location, MakeThreats(FVector(PointBlankDistance, 0.0f, 0.0f)), Settings, CloseTargetScore));
	TestEqual(TEXT("The score leaves the distance to the enemy to the weapon's distance limits"),
		CloseTargetScore, FarTargetScore);

	constexpr float ReferenceWalk = 900.0f;
	constexpr float LongWalk = 2400.0f;
	const FRTSCombatCoverThreats Threats = MakeThreats(FVector(WeaponRange, 0.0f, 0.0f));
	float ReferenceWalkScore = 0.0f;
	float LongWalkScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(
		Point, FVector(0.0f, ReferenceWalk, 0.0f), Threats, Settings, ReferenceWalkScore);
	FRTSCombatCoverScoring::TryScoreCoverPoint(Point, FVector(0.0f, LongWalk, 0.0f), Threats, Settings, LongWalkScore);
	TestTrue(TEXT("A 24 m walk costs more than a 9 m walk"), LongWalkScore < ReferenceWalkScore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceRangeClosingExemptionTest,
	"RTS.Squads.AdvanceDiagnostics.InRangeSoldiersKeepFiringWhileSquadClosesRange",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceRangeClosingExemptionTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	AInfantryWeaponMaster* Weapon = Fixture.Spawn<AInfantryWeaponMaster>();
	constexpr float TargetDistance = 2000.0f;
	AActor* Target = Fixture.SpawnTarget(FVector(TargetDistance, 0.0f, 0.0f));
	AActor* OtherTarget = Fixture.SpawnTarget(FVector(0.0f, TargetDistance, 0.0f));
	if (not TestNotNull(TEXT("Soldier exists"), Soldier) || not TestNotNull(TEXT("Weapon exists"), Weapon) ||
		not TestNotNull(TEXT("Target exists"), Target) || not TestNotNull(TEXT("Other target exists"), OtherTarget))
	{
		return false;
	}
	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, EnemyOwnedTeam);
	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *Target, WeaponRange);
	FSquadAdvanceDiagnosticAccess::SetCombatState(*Soldier, EAbilityID::IdAttack, Target);
	TestTrue(TEXT("The actual weapon reports this soldier already in range"), Weapon->GetIsCurrentTargetInRange());
	TestTrue(TEXT("An in-range soldier in the open keeps firing"), Soldier->GetMayKeepFiringWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetCoverState(*Soldier, ESquadUnitCoverState::MovingToCover);
	TestTrue(TEXT("An in-range soldier walking into cover keeps going there"), Soldier->GetMayKeepFiringWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetCoverState(*Soldier, ESquadUnitCoverState::Protected);
	TestTrue(TEXT("An in-range soldier occupying cover keeps it"), Soldier->GetMayKeepFiringWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *OtherTarget, WeaponRange);
	TestFalse(TEXT("A soldier whose weapon is on another enemy closes on the ordered one"),
		Soldier->GetMayKeepFiringWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *Target, PointBlankDistance);
	TestFalse(TEXT("A short-range weapon out of range does close in"), Soldier->GetMayKeepFiringWhileSquadClosesRange());

	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *Target, WeaponRange);
	FSquadAdvanceDiagnosticAccess::SetWeaponGroundTarget(*Weapon, Target->GetActorLocation());
	FSquadAdvanceDiagnosticAccess::SetCombatState(*Soldier, EAbilityID::IdAttackGround, nullptr);
	TestTrue(TEXT("An attack-ground soldier in range keeps firing"), Soldier->GetMayKeepFiringWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetCombatState(*Soldier, EAbilityID::IdMove, nullptr);
	TestFalse(TEXT("A soldier without a combat command is not exempt"), Soldier->GetMayKeepFiringWhileSquadClosesRange());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceRangeStopTest,
	"RTS.Squads.AdvanceDiagnostics.InRangeStopsRangeClosingButDoesNotStopCoverWalk",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceRangeStopTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	AAISquadUnit* Controller = Fixture.Spawn<AAISquadUnit>();
	if (not TestNotNull(TEXT("Soldier exists"), Soldier) || not TestNotNull(TEXT("AI controller exists"), Controller))
	{
		return false;
	}
	for (const EAbilityID CombatAbility : {EAbilityID::IdAttack, EAbilityID::IdAttackGround})
	{
		FSquadAdvanceDiagnosticAccess::SetRangeClosingState(*Soldier, *Controller, CombatAbility);
		Soldier->OnSpecificTargetInRange();
		TestEqual(TEXT("The range notification restores the original combat command"), Soldier->GetActiveCommand(), CombatAbility);
		TestFalse(TEXT("The range-closing command path is cleared"), FSquadAdvanceDiagnosticAccess::GetHasCommandPath(*Soldier));
	}
	FSquadAdvanceDiagnosticAccess::SetCombatState(*Soldier, EAbilityID::IdAttack, nullptr);
	FSquadAdvanceDiagnosticAccess::SetCoverState(*Soldier, ESquadUnitCoverState::MovingToCover);
	Soldier->OnSpecificTargetInRange();
	// Intended: cover walks are kept inside the engagement distance by the cover selection instead.
	TestTrue(TEXT("The same range notification leaves a cover walk active"), Soldier->GetIsOnItsWayIntoCover());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceAttackOrderToleranceTest,
	"RTS.Squads.AdvanceDiagnostics.AggressiveAttackKeepsCoverTargetTolerance",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceAttackOrderToleranceTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	const TStrongObjectPtr<URTSTargetAcquisition> Acquisition(NewObject<URTSTargetAcquisition>());
	TestFalse(TEXT("Hold position does not acquire a squad-wide attack order"),
		FSquadAdvanceDiagnosticAccess::GetCanAggro(*Acquisition));
	FSquadAdvanceDiagnosticAccess::SetStance(*Acquisition, ERTSAggroBehaviour::Stance_Aggressive);
	TestTrue(TEXT("Aggressive stance allows autonomous attack orders"), FSquadAdvanceDiagnosticAccess::GetCanAggro(*Acquisition));

	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	AInfantryWeaponMaster* Weapon = Fixture.Spawn<AInfantryWeaponMaster>();
	constexpr float TargetDistance = 2000.0f;
	AActor* Target = Fixture.SpawnTarget(FVector(TargetDistance, 0.0f, 0.0f));
	if (not TestNotNull(TEXT("Soldier exists"), Soldier) || not TestNotNull(TEXT("Weapon exists"), Weapon) ||
		not TestNotNull(TEXT("Target exists"), Target))
	{
		return false;
	}
	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, EnemyOwnedTeam);
	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *Target, WeaponRange);
	constexpr bool bAutonomousOrder = true;
	constexpr bool bGivenOrder = false;
	bool bIsAttackOrderTarget = false;
	FSquadAdvanceDiagnosticAccess::ExecuteAttack(*Soldier, *Target, bGivenOrder);
	TestEqual(TEXT("A given order judges cover against the ordered enemy"),
		FSquadAdvanceDiagnosticAccess::GetCoverJudgementTarget(*Soldier, bIsAttackOrderTarget), Target);
	TestTrue(TEXT("A given order keeps its stronger intent"), bIsAttackOrderTarget);
	FSquadAdvanceDiagnosticAccess::ExecuteAttack(*Soldier, *Target, bAutonomousOrder);
	TestEqual(TEXT("An autonomous attack judges cover against the weapon's target"),
		FSquadAdvanceDiagnosticAccess::GetCoverJudgementTarget(*Soldier, bIsAttackOrderTarget), Target);
	TestFalse(TEXT("An autonomous attack is not treated as an attack order"), bIsAttackOrderTarget);

	constexpr float TargetChangeToleranceSeconds = 2.5f;
	constexpr float JustLostTargetSeconds = 0.0f;
	TestEqual(TEXT("So an autonomous attack target gets the cover grace time"),
		FRTSCombatCoverScoring::DecideOnInvalidTarget(bIsAttackOrderTarget, JustLostTargetSeconds, TargetChangeToleranceSeconds),
		ERTSCoverInvalidTargetDecision::StayInCover);
	TestEqual(TEXT("While a given attack order still leaves cover at once"),
		FRTSCombatCoverScoring::DecideOnInvalidTarget(true, JustLostTargetSeconds, TargetChangeToleranceSeconds),
		ERTSCoverInvalidTargetDecision::LeaveCover);
	return true;
}

#endif
