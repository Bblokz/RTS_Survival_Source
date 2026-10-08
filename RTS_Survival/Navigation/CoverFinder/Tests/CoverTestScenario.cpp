#include "CoverTestScenario.h"

#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverTest, Log, All);

namespace CoverTestScenarioPrivate
{
	constexpr float MaximumScanWaitSeconds = 240.0f;
	constexpr float MinimumIdleObservationSeconds = 1.0f;
	constexpr float DefaultCombatObservationSeconds = 90.0f;
	// A resting unit must stand where its animation set says it does; this only allows for floor adjustment.
	constexpr float MaximumSettledCapsuleError = 15.0f;
	constexpr int32 PlayerOwnedTeam = 1;
	// Enemy squads stop this far from their target so the fight starts from idle auto-engagement.
	constexpr float ApproachStandOffDistance = 1500.0f;
	constexpr float ApproachLateralSpacing = 500.0f;
	constexpr float MaximumAttackOrderDistance = 3000.0f;
	constexpr float AttackOrderIntervalSeconds = 6.0f;
	constexpr int32 MaximumAttackOrdersPerSquad = 3;
	constexpr float CombatSampleLogIntervalSeconds = 5.0f;
	// An advancing squad moves this far every interval and stops this close to its nearest enemy.
	constexpr float AdvanceBoundDistance = 800.0f;
	constexpr float AdvanceIntervalSeconds = 12.0f;
	constexpr float AdvanceMinimumEnemyDistance = 700.0f;
	constexpr float AttackDelayAfterAdvanceSeconds = 4.0f;
	// The largest legitimate single-frame capsule move is the in-place enter snap of a standing clip.
	constexpr float MaximumLocationJumpPerFrame = 160.0f;
	constexpr float MaximumSaneMeshBoundsRadius = 400.0f;

	bool GetIsCommandCompatibleWithCover(const EAbilityID Command)
	{
		return Command == EAbilityID::IdIdle || Command == EAbilityID::IdAttack;
	}

	int32 GetSquadOwningPlayer(const ASquadController& SquadController)
	{
		const URTSComponent* SquadRTSComponent = SquadController.GetRTSComponent();
		return IsValid(SquadRTSComponent) ? SquadRTSComponent->GetOwningPlayer() : INDEX_NONE;
	}

	ASquadUnit* FindNearestUnitOfOtherPlayer(UWorld& World, const FVector& Origin, const int32 OwningPlayer)
	{
		ASquadUnit* NearestUnit = nullptr;
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
		{
			ASquadUnit* CandidateUnit = *UnitIterator;
			if (not IsValid(CandidateUnit) || not CandidateUnit->IsUnitAlive() ||
				CandidateUnit->GetOwningPlayer() == OwningPlayer)
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared(Origin, CandidateUnit->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestUnit = CandidateUnit;
			}
		}
		return NearestUnit;
	}

	/** @return A defending unit standing in high cover, whose peek only happens when opponents face its wall. */
	const ASquadUnit* FindStandingCoverOccupant(UWorld& World, const bool bDefenderIsPlayerOwned)
	{
		for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
		{
			ASquadUnit* SquadUnit = *UnitIterator;
			if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive() || not SquadUnit->GetIsOccupyingCover())
			{
				continue;
			}
			const bool bStandingCover =
				SquadUnit->GetCoverRuntimeState().AssignedCoverPoint.CoverType != ERTSCoverType::Crouch;
			const bool bIsPlayerOwned = SquadUnit->GetOwningPlayer() == PlayerOwnedTeam;
			if (bStandingCover && bIsPlayerOwned == bDefenderIsPlayerOwned)
			{
				return SquadUnit;
			}
		}
		return nullptr;
	}

	float FindClosestOpposingUnitDistance(UWorld& World)
	{
		float ClosestDistanceSquared = TNumericLimits<float>::Max();
		for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
		{
			ASquadUnit* SquadUnit = *UnitIterator;
			if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
			{
				continue;
			}
			const ASquadUnit* OpposingUnit = FindNearestUnitOfOtherPlayer(
				World,
				SquadUnit->GetActorLocation(),
				SquadUnit->GetOwningPlayer());
			if (not IsValid(OpposingUnit))
			{
				continue;
			}
			ClosestDistanceSquared = FMath::Min(
				ClosestDistanceSquared,
				FVector::DistSquared(SquadUnit->GetActorLocation(), OpposingUnit->GetActorLocation()));
		}
		return ClosestDistanceSquared < TNumericLimits<float>::Max() ? FMath::Sqrt(ClosestDistanceSquared) : -1.0f;
	}

	FString DescribeAlivePerPlayer(const TMap<int32, int32>& AliveUnitsPerPlayer)
	{
		TArray<int32> Players;
		AliveUnitsPerPlayer.GetKeys(Players);
		Players.Sort();
		FString Description;
		for (const int32 Player : Players)
		{
			Description += FString::Printf(TEXT("p%d:%d "), Player, AliveUnitsPerPlayer[Player]);
		}
		return Description.TrimEnd();
	}
}

void FCoverTestScenario::Start(const float IdleObservationSeconds, const bool bCaptureScreenshot)
{
	M_IdleObservationSeconds = FMath::Max(
		CoverTestScenarioPrivate::MinimumIdleObservationSeconds,
		IdleObservationSeconds);
	M_CombatObservationSeconds = CoverTestScenarioPrivate::DefaultCombatObservationSeconds;
	FParse::Value(FCommandLine::Get(), TEXT("CoverFinderCombatSeconds="), M_CombatObservationSeconds);
	bM_CaptureScreenshot = bCaptureScreenshot;
	bM_PlayerSquadsApproach = FParse::Param(FCommandLine::Get(), TEXT("CoverFinderPlayerApproaches"));
	bM_ApproachingSquadsAdvance = FParse::Param(FCommandLine::Get(), TEXT("CoverFinderEnemyAdvance"));
	bM_ObserveCombat = FParse::Param(FCommandLine::Get(), TEXT("CoverFinderObserveCombat"));
	M_NextAdvanceOrderSeconds = CoverTestScenarioPrivate::AdvanceIntervalSeconds;
	M_CombatObserver.Reset();
	bM_IdlePhasePassed = false;
	M_CombatTotals = FCoverTestCombatTotals();
	M_AttackingSquads.Reset();
	M_LastUnitLocations.Reset();
	M_UnitLocationJumpCount = 0;
	M_UnitMeshBlowUpCount = 0;
	EnterPhase(ECoverTestScenarioPhase::WaitingForCoverScan);
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST scheduled idle_seconds=%.1f combat_seconds=%.1f"),
		M_IdleObservationSeconds,
		M_CombatObservationSeconds);
}

void FCoverTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem, const float DeltaTime)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ECoverTestScenarioPhase::Inactive || not IsValid(World))
	{
		return;
	}
	ResumeWorldIfPaused(*World);
	KeepUnitAnimationTicking(*World);
	WatchUnitsForVisualGlitches(*World);
	M_PhaseElapsedSeconds += FMath::Max(0.0f, DeltaTime);

	switch (M_Phase)
	{
	case ECoverTestScenarioPhase::WaitingForCoverScan:
		TickWaitingForCoverScan(*World, CoverSubsystem);
		break;
	case ECoverTestScenarioPhase::ObservingIdleCover:
		if (M_PhaseElapsedSeconds >= M_IdleObservationSeconds)
		{
			FinishIdlePhase(*World, CoverSubsystem);
		}
		break;
	case ECoverTestScenarioPhase::ObservingCombatCover:
		if (bM_ApproachingSquadsAdvance)
		{
			OrderApproachingSquadsToAdvance(*World);
		}
		if (bM_ObserveCombat)
		{
			M_CombatObserver.Tick(*World, CoverSubsystem, DeltaTime, M_PhaseElapsedSeconds);
		}
		TickCombatPhase(*World, CoverSubsystem);
		if (M_PhaseElapsedSeconds >= M_CombatObservationSeconds)
		{
			FinishCombatPhase(*World, CoverSubsystem);
		}
		break;
	case ECoverTestScenarioPhase::Inactive:
	default:
		break;
	}
}

void FCoverTestScenario::KeepUnitAnimationTicking(UWorld& World) const
{
	// Reapplied every tick because the distance-based unit optimizer keeps switching unrendered meshes off.
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		const ASquadUnit* SquadUnit = *UnitIterator;
		USkeletalMeshComponent* UnitMesh = IsValid(SquadUnit) ? SquadUnit->GetMesh() : nullptr;
		if (not IsValid(UnitMesh))
		{
			continue;
		}
		UnitMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		UnitMesh->SetComponentTickEnabled(true);
	}
}

void FCoverTestScenario::WatchUnitsForVisualGlitches(UWorld& World)
{
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (IsValid(SquadUnit) && SquadUnit->IsUnitAlive())
		{
			WatchUnitForVisualGlitches(*SquadUnit);
		}
	}
}

void FCoverTestScenario::WatchUnitForVisualGlitches(ASquadUnit& SquadUnit)
{
	const FVector UnitLocation = SquadUnit.GetActorLocation();
	const FVector* LastLocation = M_LastUnitLocations.Find(&SquadUnit);
	const float JumpDistance = LastLocation != nullptr ? FVector::Dist(*LastLocation, UnitLocation) : 0.0f;
	M_LastUnitLocations.Add(&SquadUnit, UnitLocation);
	const USkeletalMeshComponent* UnitMesh = SquadUnit.GetMesh();
	const float MeshBoundsRadius = IsValid(UnitMesh) ? UnitMesh->Bounds.SphereRadius : 0.0f;
	const float MeshOffset = IsValid(UnitMesh) ? FVector::Dist(UnitMesh->Bounds.Origin, UnitLocation) : 0.0f;
	// Walking units tick their movement in coarse steps when unrendered, so only resting units are checked.
	const bool bJumped = SquadUnit.GetIsOccupyingCover() &&
		JumpDistance > CoverTestScenarioPrivate::MaximumLocationJumpPerFrame;
	const bool bMeshBlewUp = MeshBoundsRadius > CoverTestScenarioPrivate::MaximumSaneMeshBoundsRadius ||
		MeshOffset > CoverTestScenarioPrivate::MaximumSaneMeshBoundsRadius;
	if (not bJumped && not bMeshBlewUp)
	{
		return;
	}
	M_UnitLocationJumpCount += bJumped ? 1 : 0;
	M_UnitMeshBlowUpCount += bMeshBlewUp ? 1 : 0;
	const USquadUnitAnimInstance* UnitAnimation = SquadUnit.GetAnimBP_SquadUnit();
	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit.GetCoverRuntimeState();
	UE_LOG(
		LogRTSCoverTest,
		Warning,
		TEXT("RTS_COVER_TEST glitch unit=%s jump_cm=%.0f mesh_radius=%.0f mesh_offset=%.0f location=%s cover_state=%s type=%s anim_action=%s graph_pose=%s command=%s"),
		*SquadUnit.GetName(),
		JumpDistance,
		MeshBoundsRadius,
		MeshOffset,
		*UnitLocation.ToCompactString(),
		*UEnum::GetValueAsString(CoverState.State),
		*UEnum::GetValueAsString(CoverState.AssignedCoverPoint.CoverType),
		IsValid(UnitAnimation) ? *UEnum::GetValueAsString(UnitAnimation->GetCoverAnimAction()) : TEXT("none"),
		IsValid(UnitAnimation) ? *UEnum::GetValueAsString(UnitAnimation->GetCoverGraphPose()) : TEXT("none"),
		*UEnum::GetValueAsString(SquadUnit.GetActiveCommand()));
	if (not bMeshBlewUp || not IsValid(UnitMesh) || not IsValid(UnitAnimation))
	{
		return;
	}
	// Name the bone that is furthest out, and its scale: that tells a bad pose from a bad root or a bad scale.
	int32 WorstBoneIndex = INDEX_NONE;
	float WorstBoneDistance = 0.0f;
	for (int32 BoneIndex = 0; BoneIndex < UnitMesh->GetNumBones(); ++BoneIndex)
	{
		const float BoneDistance = FVector::Dist(
			UnitMesh->GetBoneTransform(BoneIndex).GetLocation(),
			UnitLocation);
		if (BoneDistance > WorstBoneDistance)
		{
			WorstBoneDistance = BoneDistance;
			WorstBoneIndex = BoneIndex;
		}
	}
	const FTransform RootTransform = UnitMesh->GetBoneTransform(0);
	UE_LOG(
		LogRTSCoverTest,
		Warning,
		TEXT("RTS_COVER_TEST glitch_pose unit=%s worst_bone=%s worst_bone_cm=%.0f worst_bone_scale=%s root_offset_cm=%.0f root_scale=%s mesh_scale=%s %s"),
		*SquadUnit.GetName(),
		WorstBoneIndex != INDEX_NONE ? *UnitMesh->GetBoneName(WorstBoneIndex).ToString() : TEXT("none"),
		WorstBoneDistance,
		WorstBoneIndex != INDEX_NONE
			? *UnitMesh->GetBoneTransform(WorstBoneIndex).GetScale3D().ToCompactString()
			: TEXT("-"),
		FVector::Dist(RootTransform.GetLocation(), UnitLocation),
		*RootTransform.GetScale3D().ToCompactString(),
		*UnitMesh->GetComponentScale().ToCompactString(),
		*UnitAnimation->GetPoseDebugString());
}

void FCoverTestScenario::ResumeWorldIfPaused(UWorld& World) const
{
	// Use the controller's own start path so its pause bookkeeping is cleared together with the world pause.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		UE_LOG(LogRTSCoverTest, Display, TEXT("RTS_COVER_TEST resuming game paused at the start-game gate"));
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (UGameplayStatics::IsGamePaused(&World))
	{
		UGameplayStatics::SetGamePaused(&World, false);
	}
}

void FCoverTestScenario::TickWaitingForCoverScan(
	UWorld& World,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	if (CoverSubsystem.GetHasCompletedFullScan())
	{
		UE_LOG(
			LogRTSCoverTest,
			Display,
			TEXT("RTS_COVER_TEST scan_ready map=%s points=%d waited_seconds=%.1f"),
			*World.GetMapName(),
			CoverSubsystem.GetCoverPointsView().Num(),
			M_PhaseElapsedSeconds);
		EnterPhase(ECoverTestScenarioPhase::ObservingIdleCover);
		return;
	}
	if (M_PhaseElapsedSeconds < CoverTestScenarioPrivate::MaximumScanWaitSeconds)
	{
		return;
	}
	UE_LOG(
		LogRTSCoverTest,
		Error,
		TEXT("RTS_COVER_TEST RESULT FAIL reason=cover_scan_never_completed map=%s"),
		*World.GetMapName());
	EnterPhase(ECoverTestScenarioPhase::Inactive);
}

void FCoverTestScenario::FinishIdlePhase(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	const FCoverTestUnitCounts Counts = GatherUnitCounts(World);
	const bool bCorrectMap = World.GetMapName().Contains(TEXT("TestCover"));
	const bool bHasBothSides = Counts.OwningPlayers.Num() >= 2;
	const bool bHasDiscoveredCover = not CoverSubsystem.GetCoverPointsView().IsEmpty();
	const bool bUnitsOccupyCover = Counts.OccupyingUnitCount > 0;
	const bool bReservationsAreUnique = Counts.DuplicateReservationCount == 0;
	const bool bOccupantsAreAtTheirPoints = Counts.OccupantsAwayFromPointCount == 0;
	const bool bCommandsUntouched = Counts.CommandInterferenceCount == 0;
	int32 UnitsShootingOwnCoverCount = 0;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		const ASquadUnit* SquadUnit = *UnitIterator;
		UnitsShootingOwnCoverCount += IsValid(SquadUnit) && SquadUnit->GetWouldShootOwnCover(CoverSubsystem) ? 1 : 0;
	}
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST own_cover units_that_would_shoot_their_own_cover=%d"),
		UnitsShootingOwnCoverCount);
	const int32 OverCapacityThinObstacleCount = CoverSubsystem.GetOverCapacityThinObstacleCount();
	int32 ThinObstaclePointCount = 0;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		ThinObstaclePointCount += CoverPoint.ThinObstacleId != 0 ? 1 : 0;
	}
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST thin_obstacles points_around_them=%d holding_more_soldiers_than_room=%d"),
		ThinObstaclePointCount,
		OverCapacityThinObstacleCount);
	bM_IdlePhasePassed = bCorrectMap && bHasBothSides && bHasDiscoveredCover && bUnitsOccupyCover &&
		bReservationsAreUnique && bOccupantsAreAtTheirPoints && bCommandsUntouched &&
		OverCapacityThinObstacleCount == 0 && UnitsShootingOwnCoverCount == 0;
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST phase=idle %s map=%s teams=%d alive=[%s] points=%d assigned=%d moving=%d occupying=%d crouch=%d standing_left=%d standing_right=%d duplicate_reservations=%d misaligned=%d max_capsule_error_cm=%.1f command_interference=%d"),
		bM_IdlePhasePassed ? TEXT("PASS") : TEXT("FAIL"),
		*World.GetMapName(),
		Counts.OwningPlayers.Num(),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.AliveUnitsPerPlayer),
		CoverSubsystem.GetCoverPointsView().Num(),
		Counts.AssignedUnitCount,
		Counts.MovingUnitCount,
		Counts.OccupyingUnitCount,
		Counts.CrouchOccupantCount,
		Counts.StandingLeftOccupantCount,
		Counts.StandingRightOccupantCount,
		Counts.DuplicateReservationCount,
		Counts.OccupantsAwayFromPointCount,
		Counts.MaximumSettledCapsuleError,
		Counts.CommandInterferenceCount);
	LogUnitStates(World);

	M_CombatTotals.AliveUnitsPerPlayerAtStart = Counts.AliveUnitsPerPlayer;
	M_CombatTotals.ApproachOrdersIssued = OrderSquadsToApproach(World);
	M_NextCombatSampleLogSeconds = 0.0f;
	EnterPhase(ECoverTestScenarioPhase::ObservingCombatCover);
}

void FCoverTestScenario::TickCombatPhase(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	OrderIdleAttackingSquadsToAttack(World);
	const FCoverTestUnitCounts Counts = GatherUnitCounts(World);
	const FRTSTacticalCoverPerformance TacticalPerformance = CoverSubsystem.GetTacticalPerformanceSnapshot();
	M_CombatTotals.FiringLaneTraceCount += TacticalPerformance.FiringLaneTracesLastFrame;
	M_CombatTotals.FiringLaneRejectionCount += TacticalPerformance.FiringLaneRejectionsLastFrame;
	M_CombatTotals.PeakOccupyingUnitCount = FMath::Max(
		M_CombatTotals.PeakOccupyingUnitCount,
		Counts.OccupyingUnitCount);
	M_CombatTotals.PeakEngagingFromCoverCount = FMath::Max(
		M_CombatTotals.PeakEngagingFromCoverCount,
		Counts.EngagingFromCoverCount);
	M_CombatTotals.PeakAttackCommandOccupantCount = FMath::Max(
		M_CombatTotals.PeakAttackCommandOccupantCount,
		Counts.AttackCommandOccupantCount);
	M_CombatTotals.PeakExposedUnitCount = FMath::Max(
		M_CombatTotals.PeakExposedUnitCount,
		Counts.ExposedUnitCount);
	M_CombatTotals.PeakExposedLeftUnitCount = FMath::Max(
		M_CombatTotals.PeakExposedLeftUnitCount,
		Counts.ExposedLeftUnitCount);
	M_CombatTotals.PeakExposedRightUnitCount = FMath::Max(
		M_CombatTotals.PeakExposedRightUnitCount,
		Counts.ExposedRightUnitCount);
	M_CombatTotals.PeakCrouchOccupantCount = FMath::Max(
		M_CombatTotals.PeakCrouchOccupantCount,
		Counts.CrouchOccupantCount);
	M_CombatTotals.MaximumSettledCapsuleError = FMath::Max(
		M_CombatTotals.MaximumSettledCapsuleError,
		Counts.MaximumSettledCapsuleError);
	M_CombatTotals.MisalignedOccupantSamples += Counts.OccupantsAwayFromPointCount > 0 ? 1 : 0;
	M_CombatTotals.DuplicateReservationSamples += Counts.DuplicateReservationCount > 0 ? 1 : 0;
	M_CombatTotals.CommandInterferenceSamples += Counts.CommandInterferenceCount > 0 ? 1 : 0;
	if (M_PhaseElapsedSeconds < M_NextCombatSampleLogSeconds)
	{
		return;
	}
	M_NextCombatSampleLogSeconds = M_PhaseElapsedSeconds +
		CoverTestScenarioPrivate::CombatSampleLogIntervalSeconds;
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST combat_sample t=%.0f alive=[%s] health=[%s] with_target=[%s] occupying=[%s] engaging_from_cover=[%s] moving_to_cover=%d exposed=%d max_capsule_error_cm=%.1f lane_traces=%lld lane_rejections=%lld"),
		M_PhaseElapsedSeconds,
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.AliveUnitsPerPlayer),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.HealthPerPlayer),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.UnitsWithTargetInRangePerPlayer),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.OccupyingUnitsPerPlayer),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.EngagingFromCoverPerPlayer),
		Counts.MovingUnitCount,
		Counts.ExposedUnitCount,
		Counts.MaximumSettledCapsuleError,
		M_CombatTotals.FiringLaneTraceCount,
		M_CombatTotals.FiringLaneRejectionCount);
}

void FCoverTestScenario::FinishCombatPhase(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	const FCoverTestUnitCounts Counts = GatherUnitCounts(World);
	const bool bAttackWasOrdered = M_CombatTotals.ApproachOrdersIssued > 0 &&
		M_CombatTotals.AttackOrdersIssued > 0;
	const bool bUnitsFoughtFromCover = M_CombatTotals.PeakEngagingFromCoverCount > 0;
	const bool bLanesWereValidated = M_CombatTotals.FiringLaneTraceCount > 0;
	const bool bReservationsStayedUnique = M_CombatTotals.DuplicateReservationSamples == 0;
	const bool bCommandsUntouched = M_CombatTotals.CommandInterferenceSamples == 0;
	const bool bOccupantsStayedAligned = M_CombatTotals.MisalignedOccupantSamples == 0;
	const bool bNoVisualGlitches = M_UnitLocationJumpCount == 0 && M_UnitMeshBlowUpCount == 0;
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST glitches location_jumps=%d mesh_blow_ups=%d"),
		M_UnitLocationJumpCount,
		M_UnitMeshBlowUpCount);
	const bool bCombatPhasePassed = bAttackWasOrdered && bUnitsFoughtFromCover && bLanesWereValidated &&
		bReservationsStayedUnique && bCommandsUntouched && bOccupantsStayedAligned && bNoVisualGlitches;
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST phase=combat %s approach_orders=%d attack_orders=%d alive_start=[%s] alive_end=[%s] closest_opposing_units_cm=%.0f peak_occupying=%d peak_engaging_from_cover=%d peak_attack_command_in_cover=%d peak_exposed=%d peak_exposed_left=%d peak_exposed_right=%d peak_crouch=%d misaligned_samples=%d max_capsule_error_cm=%.1f lane_traces=%lld lane_rejections=%lld duplicate_reservation_samples=%d command_interference_samples=%d"),
		bCombatPhasePassed ? TEXT("PASS") : TEXT("FAIL"),
		M_CombatTotals.ApproachOrdersIssued,
		M_CombatTotals.AttackOrdersIssued,
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(M_CombatTotals.AliveUnitsPerPlayerAtStart),
		*CoverTestScenarioPrivate::DescribeAlivePerPlayer(Counts.AliveUnitsPerPlayer),
		CoverTestScenarioPrivate::FindClosestOpposingUnitDistance(World),
		M_CombatTotals.PeakOccupyingUnitCount,
		M_CombatTotals.PeakEngagingFromCoverCount,
		M_CombatTotals.PeakAttackCommandOccupantCount,
		M_CombatTotals.PeakExposedUnitCount,
		M_CombatTotals.PeakExposedLeftUnitCount,
		M_CombatTotals.PeakExposedRightUnitCount,
		M_CombatTotals.PeakCrouchOccupantCount,
		M_CombatTotals.MisalignedOccupantSamples,
		M_CombatTotals.MaximumSettledCapsuleError,
		M_CombatTotals.FiringLaneTraceCount,
		M_CombatTotals.FiringLaneRejectionCount,
		M_CombatTotals.DuplicateReservationSamples,
		M_CombatTotals.CommandInterferenceSamples);
	LogUnitStates(World);
	if (bM_ObserveCombat)
	{
		M_CombatObserver.LogSummary();
	}

	const bool bPassed = bM_IdlePhasePassed && bCombatPhasePassed;
	if (bPassed)
	{
		UE_LOG(LogRTSCoverTest, Display, TEXT("RTS_COVER_TEST RESULT PASS"));
	}
	else
	{
		UE_LOG(LogRTSCoverTest, Error, TEXT("RTS_COVER_TEST RESULT FAIL"));
	}
	CoverSubsystem.LogPerformanceReport();
	if (bM_CaptureScreenshot)
	{
		CoverSubsystem.RequestDebugCapture(TEXT("CoverFinder_TestCover_UnitValidation.png"));
	}
	EnterPhase(ECoverTestScenarioPhase::Inactive);
}

int32 FCoverTestScenario::OrderSquadsToApproach(UWorld& World)
{
	int32 ApproachOrdersIssued = 0;
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
	{
		ASquadController* SquadController = *SquadIterator;
		if (not IsValid(SquadController) || SquadController->GetSquadUnitsCount() <= 0)
		{
			continue;
		}
		const int32 SquadOwner = CoverTestScenarioPrivate::GetSquadOwningPlayer(*SquadController);
		if (SquadOwner == INDEX_NONE)
		{
			continue;
		}
		// Both teams receive the later attack order; only one of them walks into contact.
		FCoverTestAttackingSquad& AttackingSquad = M_AttackingSquads.AddDefaulted_GetRef();
		AttackingSquad.SquadController = SquadController;
		FVector ApproachLocation = FVector::ZeroVector;
		const bool bSquadIsPlayerOwned = SquadOwner == CoverTestScenarioPrivate::PlayerOwnedTeam;
		if (bSquadIsPlayerOwned != bM_PlayerSquadsApproach ||
			not FindApproachLocation(World, *SquadController, ApproachOrdersIssued, ApproachLocation))
		{
			continue;
		}
		const ECommandQueueError OrderResult = SquadController->MoveToLocation(
			ApproachLocation,
			true,
			FRotator::ZeroRotator);
		UE_LOG(
			LogRTSCoverTest,
			Display,
			TEXT("RTS_COVER_TEST approach_order squad=%s owner=%d distance_cm=%.0f result=%s"),
			*SquadController->GetName(),
			SquadOwner,
			FVector::Dist(SquadController->GetActorLocation(), ApproachLocation),
			*UEnum::GetValueAsString(OrderResult));
		ApproachOrdersIssued += OrderResult == ECommandQueueError::NoError ? 1 : 0;
	}
	return ApproachOrdersIssued;
}

bool FCoverTestScenario::FindApproachLocation(
	UWorld& World,
	const ASquadController& SquadController,
	const int32 ApproachingSquadIndex,
	FVector& OutApproachLocation) const
{
	const float LateralOffset = (static_cast<float>(ApproachingSquadIndex) - 1.0f) *
		CoverTestScenarioPrivate::ApproachLateralSpacing;
	const ASquadUnit* StandingOccupant = CoverTestScenarioPrivate::FindStandingCoverOccupant(
		World,
		not bM_PlayerSquadsApproach);
	if (IsValid(StandingOccupant))
	{
		// The cover normal points from the wall to the soldier, so the exposed side lies against it.
		const FRTSCoverPoint& CoverPoint = StandingOccupant->GetCoverRuntimeState().AssignedCoverPoint;
		const FVector ExposedSideDirection = -CoverPoint.CoverNormal.GetSafeNormal2D();
		const FVector LateralDirection = FVector::CrossProduct(FVector::UpVector, ExposedSideDirection);
		OutApproachLocation = CoverPoint.Location +
			ExposedSideDirection * CoverTestScenarioPrivate::ApproachStandOffDistance +
			LateralDirection * LateralOffset;
		return true;
	}

	const FVector SquadLocation = SquadController.GetActorLocation();
	const ASquadUnit* TargetUnit = CoverTestScenarioPrivate::FindNearestUnitOfOtherPlayer(
		World,
		SquadLocation,
		CoverTestScenarioPrivate::GetSquadOwningPlayer(SquadController));
	if (not IsValid(TargetUnit))
	{
		return false;
	}
	const FVector TargetLocation = TargetUnit->GetActorLocation();
	OutApproachLocation = TargetLocation +
		(SquadLocation - TargetLocation).GetSafeNormal2D() * CoverTestScenarioPrivate::ApproachStandOffDistance;
	return true;
}

void FCoverTestScenario::OrderApproachingSquadsToAdvance(UWorld& World)
{
	using namespace CoverTestScenarioPrivate;
	if (M_PhaseElapsedSeconds < M_NextAdvanceOrderSeconds)
	{
		return;
	}
	M_NextAdvanceOrderSeconds = M_PhaseElapsedSeconds + AdvanceIntervalSeconds;
	for (FCoverTestAttackingSquad& AttackingSquad : M_AttackingSquads)
	{
		ASquadController* SquadController = AttackingSquad.SquadController.Get();
		if (not IsValid(SquadController) || SquadController->GetSquadUnitsCount() <= 0)
		{
			continue;
		}
		const int32 SquadOwner = GetSquadOwningPlayer(*SquadController);
		const bool bSquadIsPlayerOwned = SquadOwner == PlayerOwnedTeam;
		const FVector SquadLocation = SquadController->GetActorLocation();
		const ASquadUnit* NearestEnemy = FindNearestUnitOfOtherPlayer(World, SquadLocation, SquadOwner);
		if (bSquadIsPlayerOwned != bM_PlayerSquadsApproach || not IsValid(NearestEnemy))
		{
			continue;
		}
		const FVector ToEnemy = NearestEnemy->GetActorLocation() - SquadLocation;
		const float BoundDistance = FMath::Min(AdvanceBoundDistance, ToEnemy.Size2D() - AdvanceMinimumEnemyDistance);
		if (BoundDistance <= 0.0f)
		{
			continue;
		}
		const ECommandQueueError OrderResult = SquadController->MoveToLocation(
			SquadLocation + ToEnemy.GetSafeNormal2D() * BoundDistance,
			true,
			FRotator::ZeroRotator);
		// The squad attacks again once it has arrived, however many attack orders it already had.
		AttackingSquad.AttackOrdersIssued = 0;
		AttackingSquad.NextAttackOrderSeconds = M_PhaseElapsedSeconds + AttackDelayAfterAdvanceSeconds;
		UE_LOG(
			LogRTSCoverTest,
			Display,
			TEXT("RTS_COVER_TEST advance_order t=%.0f squad=%s owner=%d bound_cm=%.0f enemy_distance_cm=%.0f result=%s"),
			M_PhaseElapsedSeconds,
			*SquadController->GetName(),
			SquadOwner,
			BoundDistance,
			ToEnemy.Size2D(),
			*UEnum::GetValueAsString(OrderResult));
	}
}

void FCoverTestScenario::OrderIdleAttackingSquadsToAttack(UWorld& World)
{
	for (FCoverTestAttackingSquad& AttackingSquad : M_AttackingSquads)
	{
		ASquadController* SquadController = AttackingSquad.SquadController.Get();
		const bool bMayOrderAgain = M_PhaseElapsedSeconds >= AttackingSquad.NextAttackOrderSeconds &&
			AttackingSquad.AttackOrdersIssued < CoverTestScenarioPrivate::MaximumAttackOrdersPerSquad;
		if (not IsValid(SquadController) || not bMayOrderAgain || not SquadController->GetIsUnitIdle())
		{
			continue;
		}
		AttackingSquad.NextAttackOrderSeconds = M_PhaseElapsedSeconds +
			CoverTestScenarioPrivate::AttackOrderIntervalSeconds;
		if (not TryOrderSquadToAttack(World, *SquadController))
		{
			continue;
		}
		++AttackingSquad.AttackOrdersIssued;
		++M_CombatTotals.AttackOrdersIssued;
	}
}

bool FCoverTestScenario::TryOrderSquadToAttack(UWorld& World, ASquadController& SquadController) const
{
	const FVector SquadLocation = SquadController.GetActorLocation();
	const int32 SquadOwner = CoverTestScenarioPrivate::GetSquadOwningPlayer(SquadController);
	ASquadUnit* TargetUnit = CoverTestScenarioPrivate::FindNearestUnitOfOtherPlayer(
		World,
		SquadLocation,
		SquadOwner);
	if (not IsValid(TargetUnit))
	{
		return false;
	}
	const float TargetDistance = FVector::Dist(SquadLocation, TargetUnit->GetActorLocation());
	if (TargetDistance > CoverTestScenarioPrivate::MaximumAttackOrderDistance)
	{
		return false;
	}
	const ECommandQueueError OrderResult = SquadController.AttackActor(TargetUnit, true);
	UE_LOG(
		LogRTSCoverTest,
		Display,
		TEXT("RTS_COVER_TEST attack_order squad=%s owner=%d target=%s distance_cm=%.0f result=%s"),
		*SquadController.GetName(),
		SquadOwner,
		*TargetUnit->GetName(),
		TargetDistance,
		*UEnum::GetValueAsString(OrderResult));
	return OrderResult == ECommandQueueError::NoError;
}

FCoverTestUnitCounts FCoverTestScenario::GatherUnitCounts(UWorld& World) const
{
	FCoverTestUnitCounts Counts;
	TSet<int64> ReservedPointIds;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
		{
			continue;
		}
		AccumulateUnitCounts(*SquadUnit, ReservedPointIds, Counts);
	}
	return Counts;
}

void FCoverTestScenario::AccumulateUnitCounts(
	ASquadUnit& SquadUnit,
	TSet<int64>& InOutReservedPointIds,
	FCoverTestUnitCounts& InOutCounts) const
{
	const int32 OwningPlayer = SquadUnit.GetOwningPlayer();
	InOutCounts.OwningPlayers.Add(OwningPlayer);
	++InOutCounts.AliveUnitsPerPlayer.FindOrAdd(OwningPlayer);
	AInfantryWeaponMaster* InfantryWeapon = SquadUnit.GetInfantryWeapon();
	const bool bHasTargetInRange = IsValid(InfantryWeapon) &&
		IsValid(InfantryWeapon->GetCurrentTargetActor()) &&
		InfantryWeapon->GetIsCurrentTargetInRange();
	InOutCounts.UnitsWithTargetInRangePerPlayer.FindOrAdd(OwningPlayer) += bHasTargetInRange ? 1 : 0;
	const UHealthComponent* UnitHealth = SquadUnit.GetHealthComponent();
	InOutCounts.HealthPerPlayer.FindOrAdd(OwningPlayer) += IsValid(UnitHealth)
		? FMath::RoundToInt32(UnitHealth->GetCurrentHealth())
		: 0;
	if (not SquadUnit.GetHasCoverAssignment())
	{
		return;
	}

	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit.GetCoverRuntimeState();
	++InOutCounts.AssignedUnitCount;
	InOutCounts.MovingUnitCount += CoverState.State == ESquadUnitCoverState::MovingToCover ? 1 : 0;
	const ERTSCoverType CoverType = CoverState.AssignedCoverPoint.CoverType;
	const bool bIsExposed = CoverState.State == ESquadUnitCoverState::Exposed;
	InOutCounts.ExposedUnitCount += bIsExposed ? 1 : 0;
	InOutCounts.ExposedLeftUnitCount += bIsExposed && CoverType == ERTSCoverType::StandingLeft ? 1 : 0;
	InOutCounts.ExposedRightUnitCount += bIsExposed && CoverType == ERTSCoverType::StandingRight ? 1 : 0;
	bool bPointWasAlreadyReserved = false;
	InOutReservedPointIds.Add(CoverState.AssignedCoverPoint.PointId, &bPointWasAlreadyReserved);
	InOutCounts.DuplicateReservationCount += bPointWasAlreadyReserved ? 1 : 0;

	ASquadController* SquadController = SquadUnit.GetSquadControllerChecked();
	const EAbilityID SquadCommand = IsValid(SquadController)
		? SquadController->GetActiveCommandID()
		: EAbilityID::IdIdle;
	const bool bCommandsCompatible =
		CoverTestScenarioPrivate::GetIsCommandCompatibleWithCover(SquadUnit.GetActiveCommand()) &&
		CoverTestScenarioPrivate::GetIsCommandCompatibleWithCover(SquadCommand);
	InOutCounts.CommandInterferenceCount += bCommandsCompatible ? 0 : 1;
	if (not SquadUnit.GetIsOccupyingCover())
	{
		return;
	}

	++InOutCounts.OccupyingUnitCount;
	++InOutCounts.OccupyingUnitsPerPlayer.FindOrAdd(OwningPlayer);
	InOutCounts.CrouchOccupantCount += CoverType == ERTSCoverType::Crouch ? 1 : 0;
	InOutCounts.StandingLeftOccupantCount += CoverType == ERTSCoverType::StandingLeft ? 1 : 0;
	InOutCounts.StandingRightOccupantCount += CoverType == ERTSCoverType::StandingRight ? 1 : 0;
	float SettledCapsuleError = 0.0f;
	if (SquadUnit.TryGetSettledCoverCapsuleError(SettledCapsuleError))
	{
		InOutCounts.MaximumSettledCapsuleError = FMath::Max(
			InOutCounts.MaximumSettledCapsuleError,
			SettledCapsuleError);
		const bool bIsMisaligned = SettledCapsuleError > CoverTestScenarioPrivate::MaximumSettledCapsuleError;
		InOutCounts.OccupantsAwayFromPointCount += bIsMisaligned ? 1 : 0;
		if (bIsMisaligned)
		{
			UE_LOG(
				LogRTSCoverTest,
				Warning,
				TEXT("RTS_COVER_TEST misaligned unit=%s error_cm=%.1f state=%s type=%s speed=%.0f"),
				*SquadUnit.GetName(),
				SettledCapsuleError,
				*UEnum::GetValueAsString(CoverState.State),
				*UEnum::GetValueAsString(CoverType),
				SquadUnit.GetVelocity().Size2D());
		}
	}

	InOutCounts.EngagingFromCoverCount += bHasTargetInRange ? 1 : 0;
	InOutCounts.EngagingFromCoverPerPlayer.FindOrAdd(OwningPlayer) += bHasTargetInRange ? 1 : 0;
	InOutCounts.AttackCommandOccupantCount += SquadCommand == EAbilityID::IdAttack ? 1 : 0;
}

void FCoverTestScenario::LogUnitStates(UWorld& World) const
{
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive() || not SquadUnit->GetHasCoverAssignment())
		{
			continue;
		}
		const FSquadUnitCoverRuntimeState& CoverState = SquadUnit->GetCoverRuntimeState();
		UE_LOG(
			LogRTSCoverTest,
			Display,
			TEXT("RTS_COVER_TEST unit=%s owner=%d state=%s type=%s reason=%s unit_command=%s distance_to_point_cm=%.0f"),
			*SquadUnit->GetName(),
			SquadUnit->GetOwningPlayer(),
			*UEnum::GetValueAsString(CoverState.State),
			*UEnum::GetValueAsString(CoverState.AssignedCoverPoint.CoverType),
			*UEnum::GetValueAsString(CoverState.UseReason),
			*UEnum::GetValueAsString(SquadUnit->GetActiveCommand()),
			FVector::Dist2D(SquadUnit->GetActorLocation(), CoverState.AssignedCoverPoint.Location));
	}
}

void FCoverTestScenario::EnterPhase(const ECoverTestScenarioPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseElapsedSeconds = 0.0f;
}
