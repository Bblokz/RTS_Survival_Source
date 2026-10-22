// Copyright (C) Bas Blokzijl - All rights reserved.

// Characterization tests: passing means the current advance policy was reproduced, not that it is desirable.
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
	constexpr float WeaponRange = 3000.0f;
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

		explicit FScopedCoverSettings(URTSCoverFinderDeveloperSettings& Settings)
			: PlayerRadius(Settings.M_AutomaticCoverSearchRadius, PlayerSearchRadius),
			  EnemyRadius(Settings.M_EnemyAutomaticCoverSearchRadius, EnemySearchRadius),
			  RepositionDistance(Settings.M_CombatCoverRepositionRadius, RepositionRadius),
			  Protection(Settings.M_CombatCoverProtectionWeight, ProtectionWeight),
			  Facing(Settings.M_CombatCoverFacingWeight, FacingWeight),
			  Travel(Settings.M_CombatCoverTravelWeight, TravelWeight),
			  ScoreGain(Settings.M_CombatCoverMinimumScoreGain, MinimumScoreGain),
			  AimYaw(Settings.M_CombatCoverMaximumAimYawDegrees, MaximumAimYaw),
			  ProtectedAngle(Settings.M_CombatCoverProtectedHalfAngleDegrees, ProtectedHalfAngle)
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceInitialCoverTest,
	"RTS.Squads.AdvanceDiagnostics.EnemyInitialCoverCanApproachWithinTwoMetres",
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
	TestEqual(TEXT("The 24 m advance is outside the player's 12 m search"), ChosenPoint.PointId, NearbyPoint.PointId);
	Subsystem->ReleaseCoverReservation(*Soldier, ChosenPoint.PointId);
	FSquadAdvanceDiagnosticAccess::SetOwningPlayer(*Soldier, EnemyOwnedTeam);
	TestTrue(TEXT("Enemy finds cover with the identical geometry"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, nullptr, ChosenPoint));
	TestEqual(TEXT("The enemy chooses the 24 m forward walk over nearby sideways cover"), ChosenPoint.PointId, ForwardPoint.PointId);
	TestEqual(TEXT("The selected cover is only 2 m from the enemy"),
		FVector::Dist2D(ChosenPoint.Location, Target->GetActorLocation()), static_cast<double>(PointBlankDistance));
	FRTSCoverPoint GoodNearbyPoint = NearbyPoint;
	GoodNearbyPoint.CoverNormal = (GoodNearbyPoint.Location - Target->GetActorLocation()).GetSafeNormal2D();
	FSquadAdvanceDiagnosticAccess::PublishCover(*Subsystem, {ForwardPoint, GoodNearbyPoint});
	TestTrue(TEXT("Control: well-facing nearby cover remains usable"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, nullptr, ChosenPoint));
	TestEqual(TEXT("Control: equally protective nearby cover wins; there is no unconditional forward preference"),
		ChosenPoint.PointId, GoodNearbyPoint.PointId);
	AddInfo(TEXT("REPRODUCED: cover selection alone accepts a 2400 cm advance ending 200 cm from the target."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceCoverRepositionTest,
	"RTS.Squads.AdvanceDiagnostics.BetterAngleCanRepositionWithinTwoMetres",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceCoverRepositionTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	FScopedCoverSettings Settings(*GetMutableDefault<URTSCoverFinderDeveloperSettings>());
	FDecisionWorld Fixture;
	ASquadUnit* Soldier = Fixture.Spawn<ASquadUnit>();
	constexpr float TargetDistance = 1000.0f;
	constexpr float FlankedCoverYaw = 75.0f;
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
	FRTSCoverPoint ChosenPoint;
	TestTrue(TEXT("An existing occupant is offered the forward point"), Subsystem->TryReserveCombatCoverPoint(
		*Soldier, *Target, Threats, WeaponRange, &OccupiedPoint, ChosenPoint));
	TestEqual(TEXT("The score-gain threshold accepts the point 2 m from the target"), ChosenPoint.PointId, ForwardPoint.PointId);
	const FRTSCombatCoverSettings CombatSettings = FSquadAdvanceDiagnosticAccess::GetCombatSettings(*Subsystem);
	float CurrentScore = 0.0f;
	float ForwardScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(OccupiedPoint, Soldier->GetActorLocation(), Threats, CombatSettings, CurrentScore);
	FRTSCombatCoverScoring::TryScoreCoverPoint(ForwardPoint, Soldier->GetActorLocation(), Threats, CombatSettings, ForwardScore);
	TestTrue(TEXT("The real reposition score confirms the selection"),
		FRTSCombatCoverScoring::GetIsWorthRepositioning(CurrentScore, ForwardScore, CombatSettings));
	AddInfo(FString::Printf(TEXT("REPRODUCED: improving the angle accepts an 800 cm advance; scores %.3f -> %.3f."),
		CurrentScore, ForwardScore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceCoverDistanceTest,
	"RTS.Squads.AdvanceDiagnostics.CoverScoreHasNoEnemyDistancePenalty",
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
	TestEqual(TEXT("30 m and 2 m separation receive exactly the same cover score"), CloseTargetScore, FarTargetScore);
	AddInfo(TEXT("REPRODUCED: scoring uses protection, facing and travel, with no preferred combat separation."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceRangeClosingExemptionTest,
	"RTS.Squads.AdvanceDiagnostics.OnlyOccupiedCoverIsExemptFromSquadRangeClosing",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceRangeClosingExemptionTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
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
	FSquadAdvanceDiagnosticAccess::SetCombatState(*Soldier, EAbilityID::IdAttack, Target);
	TestTrue(TEXT("The actual weapon reports this soldier already in range"), Weapon->GetIsCurrentTargetInRange());
	TestFalse(TEXT("An in-range soldier in the open is NOT exempt from squad range closing"),
		Soldier->GetCanHoldCoverWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetCoverState(*Soldier, ESquadUnitCoverState::MovingToCover);
	TestFalse(TEXT("An in-range soldier walking into cover is also NOT exempt"), Soldier->GetCanHoldCoverWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetCoverState(*Soldier, ESquadUnitCoverState::Protected);
	TestTrue(TEXT("An in-range soldier occupying cover IS exempt"), Soldier->GetCanHoldCoverWhileSquadClosesRange());
	FSquadAdvanceDiagnosticAccess::SetupWeapon(*Soldier, *Weapon, *Target, PointBlankDistance);
	TestFalse(TEXT("A short-range weapon loses that exemption, even in occupied cover"), Soldier->GetCanHoldCoverWhileSquadClosesRange());
	AddInfo(TEXT("REPRODUCED: the squad-wide range-closing filter protects occupied cover, not all in-range soldiers."));
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
	TestTrue(TEXT("The same range notification leaves a cover walk active"), Soldier->GetIsOnItsWayIntoCover());
	AddInfo(TEXT("REPRODUCED: the in-range stop exists for range closing; cover movement bypasses that stop."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadAdvanceAttackOrderToleranceTest,
	"RTS.Squads.AdvanceDiagnostics.AggressiveAttackOrdersBypassCoverTargetTolerance",
	SquadAdvanceDiagnosticTestsPrivate::TestFlags)

bool FSquadAdvanceAttackOrderToleranceTest::RunTest(const FString& Parameters)
{
	using namespace SquadAdvanceDiagnosticTestsPrivate;
	const TStrongObjectPtr<URTSTargetAcquisition> Acquisition(NewObject<URTSTargetAcquisition>());
	TestFalse(TEXT("Hold position does not acquire a squad-wide attack order"),
		FSquadAdvanceDiagnosticAccess::GetCanAggro(*Acquisition));
	FSquadAdvanceDiagnosticAccess::SetStance(*Acquisition, ERTSAggroBehaviour::Stance_Aggressive);
	TestTrue(TEXT("Aggressive stance allows autonomous attack orders"), FSquadAdvanceDiagnosticAccess::GetCanAggro(*Acquisition));
	constexpr float TargetChangeToleranceSeconds = 2.5f;
	constexpr float JustLostTargetSeconds = 0.0f;
	TestEqual(TEXT("An automatically engaged weapon target gets cover grace time"),
		FRTSCombatCoverScoring::DecideOnInvalidTarget(false, JustLostTargetSeconds, TargetChangeToleranceSeconds),
		ERTSCoverInvalidTargetDecision::StayInCover);
	TestEqual(TEXT("The same invalid target on an attack order causes immediate cover exit"),
		FRTSCombatCoverScoring::DecideOnInvalidTarget(true, JustLostTargetSeconds, TargetChangeToleranceSeconds),
		ERTSCoverInvalidTargetDecision::LeaveCover);
	AddInfo(TEXT("REPRODUCED: attack-order cover policy has no distinction between an aggressive auto-order and a player order."));
	return true;
}

#endif
